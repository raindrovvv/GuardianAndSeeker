// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GS_AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "System/Utility/GS_AssetLoader.h"

const FName AGS_AIController::HomePosKey(TEXT("HomePosition"));
const FName AGS_AIController::MoveLocationKey(TEXT("MoveLocation"));
const FName AGS_AIController::TargetActorKey(TEXT("TargetActor"));
const FName AGS_AIController::CommandKey(TEXT("RTSCommand"));
const FName AGS_AIController::TargetLockedKey(TEXT("bTargetLocked"));
const FName AGS_AIController::DebuffLockedKey(TEXT("bDebuffLocked"));
const FName AGS_AIController::CanAttackKey(TEXT("bCanAttack"));
const FName AGS_AIController::LastAttackTimeKey(TEXT("LastAttackTime"));

AGS_AIController::AGS_AIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>("PathFollowingComponent"))
{
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	// DetectionByAffiliation 설정을 ConfigureSense 전에 해야 함
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
}

void AGS_AIController::BeginPlay()
{
	Super::BeginPlay();

	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AGS_AIController::TargetPerceptionUpdated);

	if (UCrowdFollowingComponent* CrowdComp = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		CrowdComp->SetCrowdSimulationState(ECrowdSimulationState::Enabled);
		CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Low);
		CrowdComp->SetAvoidanceGroup(1);
		CrowdComp->SetGroupsToAvoid(1);
		CrowdComp->SetCrowdCollisionQueryRange(200.0f);
		CrowdComp->SetCrowdPathOptimizationRange(100.0f);
		CrowdComp->SetCrowdSeparation(true);
		CrowdComp->SetCrowdSeparationWeight(0.2f);

		CrowdComp->SetCrowdOptimizeVisibility(false);
		CrowdComp->SetCrowdOptimizeTopology(false);
		CrowdComp->SetCrowdRotateToVelocity(false);
	}
}

void AGS_AIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (AGS_Monster* Monster = Cast<AGS_Monster>(InPawn))
	{
		TArray<FSoftObjectPath> AssetsToLoad;
		if (!Monster->BTAsset.IsNull())
		{
			AssetsToLoad.Add(Monster->BTAsset.ToSoftObjectPath());
		}
		if (!Monster->BBAsset.IsNull())
		{
			AssetsToLoad.Add(Monster->BBAsset.ToSoftObjectPath());
		}

		if (AssetsToLoad.Num() > 0)
		{
			// 비동기 로드 시작
			UGS_AssetLoader::AsyncLoadMultipleAssets(AssetsToLoad, [this, Monster, InPawn]()
			                                         {
				// 로드 완료 후 컨트롤러가 여전히 이 폰을 소유하고 있는지 확인
				if (!IsValid(this) || !IsValid(Monster) || GetPawn() != InPawn)
				{
					return;
				}

				UBehaviorTree* LoadedBT = Monster->BTAsset.Get();
				UBlackboardData* LoadedBB = Monster->BBAsset.Get();

				UBlackboardComponent* BlackboardComponent = Blackboard;
				if (LoadedBB && UseBlackboard(LoadedBB, BlackboardComponent))
				{
					BlackboardComponent->SetValueAsVector(HomePosKey, InPawn->GetActorLocation());

					if (LoadedBT)
					{
						RunBehaviorTree(LoadedBT);
					}
				} });
		}
	}

	SetGenericTeamId(GetGenericTeamId());
}

FGenericTeamId AGS_AIController::GetGenericTeamId() const
{
	if (AGS_Character* Char = Cast<AGS_Character>(GetPawn()))
	{
		return Char->TeamId;
	}
	return FGenericTeamId::NoTeam;
}

ETeamAttitude::Type AGS_AIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	const IGenericTeamAgentInterface* OtherTeamAgent = Cast<IGenericTeamAgentInterface>(&Other);
	if (!OtherTeamAgent)
	{
		return ETeamAttitude::Neutral;
	}

	const FGenericTeamId MyTeamId = GetGenericTeamId();
	const FGenericTeamId OtherTeamId = OtherTeamAgent->GetGenericTeamId();

	// 몬스터(TeamId=2)의 특수 로직
	if (MyTeamId == FGenericTeamId(2))
	{
		if (OtherTeamId == FGenericTeamId(2))
		{
			// 몬스터끼리는 아군
			return ETeamAttitude::Friendly;
		}
		else if (OtherTeamId == FGenericTeamId(1))
		{
			// 시커는 적
			return ETeamAttitude::Hostile;
		}
		else
		{
			// 가디언(TeamId=0) 등은 중립
			return ETeamAttitude::Neutral;
		}
	}

	// 다른 팀들은 기본 로직 (같은 팀 = 아군, 다른 팀 = 적)
	if (MyTeamId == OtherTeamId)
	{
		return ETeamAttitude::Friendly;
	}

	return ETeamAttitude::Hostile;
}

void AGS_AIController::TargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (Blackboard->GetValueAsBool(DebuffLockedKey))
	{
		return;
	}

	if (Blackboard->GetValueAsBool(TargetLockedKey))
	{
		return;
	}

	AActor* CurrentTarget = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey));
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	// 1. 새로운 자극이 감지된 경우 (시야에 들어옴)
	if (Stimulus.WasSuccessfullySensed())
	{
		// 적대적인 대상인지 확인
		if (Actor && GetTeamAttitudeTowards(*Actor) == ETeamAttitude::Hostile)
		{
			if (!CurrentTarget)
			{
				// 타겟이 없으면 즉시 설정
				SetNewTarget(Actor);
			}
			else
			{
				// 이미 타겟이 있다면, 새로운 적이 현재 타겟보다 더 가까운지 확인
				const float CurrentDistSq = FVector::DistSquared(ControlledPawn->GetActorLocation(), CurrentTarget->GetActorLocation());
				const float NewDistSq = FVector::DistSquared(ControlledPawn->GetActorLocation(), Actor->GetActorLocation());

				if (NewDistSq < CurrentDistSq)
				{
					SetNewTarget(Actor);
				}
			}
		}
	}
	// 2. 자극을 놓친 경우 (시야에서 사라짐)
	else
	{
		// 시야에서 사라진 게 현재 타겟인 경우에만 새로운 타겟 재탐색
		if (Actor == CurrentTarget)
		{
			TArray<AActor*> PerceivedActors;
			PerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

			AActor* NearestTarget = nullptr;
			float ClosestDistSq = TNumericLimits<float>::Max();

			for (AActor* PotentialTarget : PerceivedActors)
			{
				if (PotentialTarget && GetTeamAttitudeTowards(*PotentialTarget) == ETeamAttitude::Hostile)
				{
					const float DistSq = FVector::DistSquared(ControlledPawn->GetActorLocation(), PotentialTarget->GetActorLocation());
					if (DistSq < ClosestDistSq)
					{
						ClosestDistSq = DistSq;
						NearestTarget = PotentialTarget;
					}
				}
			}

			if (NearestTarget)
			{
				SetNewTarget(NearestTarget);
			}
			else
			{
				ClearCurrentTarget();
			}
		}
	}
}

void AGS_AIController::SetNewTarget(AActor* NewTarget)
{
	if (NewTarget)
	{
		Blackboard->SetValueAsObject(TargetActorKey, NewTarget);

		// 새 타겟인 경우 델리게이트 연결
		AGS_Character* NewTargetCharacter = Cast<AGS_Character>(NewTarget);
		if (NewTargetCharacter && TargetCharacter.Get() != NewTargetCharacter)
		{
			if (TargetCharacter.IsValid())
			{
				TargetCharacter->OnDeathDelegate.RemoveDynamic(this, &AGS_AIController::OnTargetDied);
			}

			TargetCharacter = NewTargetCharacter;
			if (!NewTargetCharacter->IsDead())
			{
				TargetCharacter->OnDeathDelegate.AddDynamic(this, &AGS_AIController::OnTargetDied);
			}
		}
	}
}

void AGS_AIController::OnTargetDied()
{
	if (Blackboard)
	{
		Blackboard->ClearValue(TargetActorKey);
		Blackboard->SetValueAsEnum(CommandKey, 0);
	}

	TargetCharacter = nullptr;
}

void AGS_AIController::ClearCurrentTarget()
{
	Blackboard->ClearValue(TargetActorKey);

	if (TargetCharacter.IsValid())
	{
		TargetCharacter->OnDeathDelegate.RemoveDynamic(this, &AGS_AIController::OnTargetDied);
		TargetCharacter = nullptr;
	}
}


void AGS_AIController::LockTarget(AGS_Character* Target)
{
	Blackboard->SetValueAsObject(TargetActorKey, Target);
	Blackboard->SetValueAsBool(TargetLockedKey, true);
}

void AGS_AIController::UnlockTarget()
{
	Blackboard->SetValueAsBool(TargetLockedKey, false);
}

void AGS_AIController::EnterConfuseState()
{
	PrevTargetActor = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey));
	Blackboard->ClearValue(TargetActorKey);
	Blackboard->SetValueAsBool(DebuffLockedKey, true);
	PerceptionComponent->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
}

void AGS_AIController::ExitConfuseState()
{
	PerceptionComponent->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	Blackboard->SetValueAsBool(DebuffLockedKey, false);

	if (PrevTargetActor.IsValid())
	{
		Blackboard->SetValueAsObject(TargetActorKey, PrevTargetActor.Get());
	}
	else
	{
		PerceptionComponent->RequestStimuliListenerUpdate();
	}
	PrevTargetActor.Reset();
}
