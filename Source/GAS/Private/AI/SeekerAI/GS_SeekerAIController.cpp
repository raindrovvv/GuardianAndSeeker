// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "AI/SeekerAI/GS_AISeeker.h"
#include "AI/SeekerAI/GS_AIGoalTrigger.h"
#include "Character/Skill/ESkill.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "NavigationSystem.h"
#include "System/Utility/GS_AssetLoader.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Seeker/GS_Merci.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Player/Guardian/GS_Guardian.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Component/Seeker/GS_MarkerPlacementComponent.h"
#include "Props/Trap/GS_TrapBase.h"
#include "Props/Interactables/GS_Door.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"
#include "Engine/OverlapResult.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "AI/GS_AIConstants.h"

// 블랙보드 키 이름
const FName AGS_SeekerAIController::TargetEnemyKey = TEXT("TargetEnemy");
const FName AGS_SeekerAIController::TargetLocationKey = TEXT("TargetLocation");
const FName AGS_SeekerAIController::GoalActorKey = TEXT("GoalActor");
const FName AGS_SeekerAIController::CurrentHealthPercentKey = TEXT("CurrentHealthPercent");
const FName AGS_SeekerAIController::IsInCombatKey = TEXT("IsInCombat");
const FName AGS_SeekerAIController::ShouldHealKey = TEXT("ShouldHeal");
const FName AGS_SeekerAIController::ShouldEvadeKey = TEXT("ShouldEvade");
const FName AGS_SeekerAIController::NearbyTrapKey = TEXT("NearbyTrap");
const FName AGS_SeekerAIController::ExplorationTargetKey = TEXT("ExplorationTarget");
const FName AGS_SeekerAIController::HasReachedGoalKey = TEXT("HasReachedGoal");
const FName AGS_SeekerAIController::LastKnownEnemyLocationKey = TEXT("LastKnownEnemyLocation");
const FName AGS_SeekerAIController::DownedAllyKey = TEXT("DownedAlly");
const FName AGS_SeekerAIController::InteractiveItemKey = TEXT("InteractiveItem");

// EQS 전술적 위치 키
const FName AGS_SeekerAIController::CoverLocationKey = TEXT("CoverLocation");
const FName AGS_SeekerAIController::FlankingLocationKey = TEXT("FlankingLocation");
const FName AGS_SeekerAIController::SafeRetreatLocationKey = TEXT("SafeRetreatLocation");

AGS_SeekerAIController::AGS_SeekerAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer
                .SetDefaultSubobjectClass<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"))
                .SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
	PrimaryActorTick.TickInterval = 0.1f; // 10 FPS 틱 속도 (성능 최적화)

	SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception")));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	if (SightConfig)
	{
		SightConfig->SightRadius = SightRadius;
		SightConfig->LoseSightRadius = LoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
		SightConfig->SetMaxAge(5.0f);
		SightConfig->AutoSuccessRangeFromLastSeenLocation = AutoSuccessRangeFromLastSeenLocation;

		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

		GetPerceptionComponent()->ConfigureSense(*SightConfig);
		GetPerceptionComponent()->SetDominantSense(SightConfig->GetSenseImplementation());
	}

	SetGenericTeamId(FGenericTeamId(1)); // 팀 1: 시커 (플레이어 팀)
}

void AGS_SeekerAIController::BeginPlay()
{
	Super::BeginPlay();

	if (GetPerceptionComponent())
	{
		GetPerceptionComponent()->OnTargetPerceptionUpdated.AddDynamic(this, &AGS_SeekerAIController::OnTargetPerceptionUpdated);
	}
}

void AGS_SeekerAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UE_LOG(LogTemp, Log, TEXT("[SeekerAIController::OnPossess] InPawn: %s (Class: %s)"),
	       InPawn ? *InPawn->GetName() : TEXT("NULL"),
	       InPawn ? *InPawn->GetClass()->GetName() : TEXT("NULL"));

	if (InPawn)
	{
		AActor* PawnOwner = InPawn->GetOwner();
		UE_LOG(LogTemp, Log, TEXT("[SeekerAIController::OnPossess] InPawn Owner: %s (Class: %s)"),
		       PawnOwner ? *PawnOwner->GetName() : TEXT("NULL"),
		       PawnOwner ? *PawnOwner->GetClass()->GetName() : TEXT("NULL"));
	}

	// 제어 중인 시커 개체 캐싱
	if (AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(GetOwner()))
	{
		ControlledSeeker = AISeeker;
		UE_LOG(LogTemp, Log, TEXT("[SeekerAIController::OnPossess] ControlledSeeker SET: %s"), *AISeeker->GetName());
	}

	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(InPawn))
	{
		CachedStatComp = Seeker->GetStatComp();
		CachedSkillComp = Seeker->FindComponentByClass<UGS_SkillComp>();

		if (UCharacterMovementComponent* MoveComp = Seeker->GetCharacterMovement())
		{
			MoveComp->bOrientRotationToMovement = true;
			MoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
			MoveComp->bUseRVOAvoidance = true;
			MoveComp->AvoidanceConsiderationRadius = 200.0f;
		}
		// 탐험 중 이동 방향을 향하도록 설정
		Seeker->bUseControllerRotationYaw = false;

		LastPosition = InPawn->GetActorLocation();
		StationaryTime = 0.0f;
	}

	// AI 에셋 비동기 초기화
	TArray<FSoftObjectPath> AssetsToLoad;
	if (BehaviorTreeAsset.ToSoftObjectPath().IsValid())
		AssetsToLoad.Add(BehaviorTreeAsset.ToSoftObjectPath());
	if (BlackboardAsset.ToSoftObjectPath().IsValid())
		AssetsToLoad.Add(BlackboardAsset.ToSoftObjectPath());
	if (CoverQueryTemplate.ToSoftObjectPath().IsValid())
		AssetsToLoad.Add(CoverQueryTemplate.ToSoftObjectPath());
	if (FlankQueryTemplate.ToSoftObjectPath().IsValid())
		AssetsToLoad.Add(FlankQueryTemplate.ToSoftObjectPath());

	if (AssetsToLoad.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[SeekerAIController::OnPossess] %d개의 AI 에셋 비동기 로드 시작..."), AssetsToLoad.Num());

		TWeakObjectPtr<AGS_SeekerAIController> WeakThis(this);
		UGS_AssetLoader::AsyncLoadMultipleAssets(AssetsToLoad, [WeakThis]()
		                                         {
			if (AGS_SeekerAIController* StrongThis = WeakThis.Get())
			{
				StrongThis->InitializeBehaviorTree();
			} });
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[SeekerAIController::OnPossess] No AI assets to load!"));
	}
}

void AGS_SeekerAIController::InitializeBehaviorTree()
{
	if (!GetPawn())
		return;

	UBehaviorTree* LoadedBT = UGS_AssetLoader::SyncLoadAsset(BehaviorTreeAsset);
	if (LoadedBT)
	{
		UE_LOG(LogTemp, Log, TEXT("[SeekerAIController::InitializeBehaviorTree] 동작 트리 준비 완료: %s"), *LoadedBT->GetName());

		UBlackboardComponent* BlackboardComponent = nullptr;
		UBlackboardData* LoadedBB = UGS_AssetLoader::SyncLoadAsset(BlackboardAsset);

		if (LoadedBB)
		{
			UseBlackboard(LoadedBB, BlackboardComponent);
			Blackboard = BlackboardComponent;
		}
		else if (LoadedBT->BlackboardAsset)
		{
			UseBlackboard(LoadedBT->BlackboardAsset, BlackboardComponent);
			Blackboard = BlackboardComponent;
		}

		if (RunBehaviorTree(LoadedBT))
		{
			UE_LOG(LogTemp, Log, TEXT("[SeekerAIController::InitializeBehaviorTree] 동작 트리 실행 성공"));

			if (AGS_AISeeker* AISeeker = ControlledSeeker.Get())
			{
				if (AISeeker->bAutoStartExploration)
				{
					FindNewExplorationTarget();
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[SeekerAIController::InitializeBehaviorTree] RunBehaviorTree FAILED"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[SeekerAIController::InitializeBehaviorTree] 로드 후 동작 트리 에셋이 null입니다!"));
	}
}

void AGS_SeekerAIController::OnUnPossess()
{
	if (UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent())
	{
		PerceptionComp->OnTargetPerceptionUpdated.RemoveAll(this);
	}

	CachedStatComp = nullptr;
	CachedSkillComp = nullptr;

	Super::OnUnPossess();
}

void AGS_SeekerAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	if (UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent())
	{
		PerceptionComp->OnTargetPerceptionUpdated.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AGS_SeekerAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(ControlledPawn);

	if (!ControlledPawn)
		return;

	// 성능 LOD: 로컬 플레이어와의 거리에 따라 업데이트 빈도 조절
	float DistanceToLocalPlayer = 10000.0f;
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* PlayerPawn = PC->GetPawn())
		{
			DistanceToLocalPlayer = FVector::Dist(ControlledPawn->GetActorLocation(), PlayerPawn->GetActorLocation());
		}
	}

	// LOD 단계별 업데이트 빈도 결정 (상수 파일 GS_AIConstants.h 참조)
	TickFrameCounter++;

	bool bShouldSkipUpdate = false;
	if (DistanceToLocalPlayer > GS_AI::LOD_DISTANCE_FAR && (TickFrameCounter % 5 != 0))
		bShouldSkipUpdate = true;
	else if (DistanceToLocalPlayer > GS_AI::LOD_DISTANCE_MID && (TickFrameCounter % 2 != 0))
		bShouldSkipUpdate = true;

	if (bShouldSkipUpdate)
		return;

	TickUpdatePhase = (TickUpdatePhase + 1) % 3;

	switch (TickUpdatePhase)
	{
	case 0:
		HandleStuckDetection(DeltaTime, ControlledPawn, Seeker, World);
		break;

	case 1:
		if (IsInCombat())
		{
			AActor* BestTarget = FindBestTarget();
			if (BestTarget && BestTarget != CurrentTargetEnemy.Get())
			{
				SetTargetEnemy(BestTarget);
			}
		}
		break;

	case 2:
		UpdateOrientation(DeltaTime, ControlledPawn);
		break;
	}

	if (Seeker)
	{
		if (Seeker->IsInDyingState())
		{
			if (Seeker->IsBeingRevived())
			{
				StopMovement();
				return;
			}

			AActor* Ally = FindNearestHealthyAlly();
			if (Ally)
			{
				float DistToAlly = FVector::Dist(ControlledPawn->GetActorLocation(), Ally->GetActorLocation());
				if (DistToAlly > 250.0f)
				{
					MoveToActor(Ally, 150.0f);
				}
				else
				{
					StopMovement();
				}
			}
			else
			{
				StopMovement();
			}
			return;
		}

		MonsterOverlapTimer += DeltaTime;
		if (MonsterOverlapTimer >= GS_AI::MONSTER_OVERLAP_INTERVAL)
		{
			MonsterOverlapTimer = 0.0f;

			TArray<FOverlapResult> Overlaps;
			FCollisionQueryParams CollisionParams;
			CollisionParams.AddIgnoredActor(ControlledPawn);
			FCollisionShape Sphere = FCollisionShape::MakeSphere(GS_AI::MONSTER_OVERLAP_RADIUS);
			if (World->OverlapMultiByChannel(Overlaps, ControlledPawn->GetActorLocation(), FQuat::Identity, ECC_Pawn, Sphere, CollisionParams))
			{
				bool bShouldStopForMonster = false;
				for (const FOverlapResult& Overlap : Overlaps)
				{
					if (AGS_Monster* Monster = Cast<AGS_Monster>(Overlap.GetActor()))
					{
						if (GetMoveStatus() == EPathFollowingStatus::Moving)
						{
							FVector ToMonster = (Monster->GetActorLocation() - ControlledPawn->GetActorLocation()).GetSafeNormal();
							FVector VelocityDir = ControlledPawn->GetVelocity().GetSafeNormal();
							// 몬스터 방향으로 이동 중인지 확인
							if (FVector::DotProduct(ToMonster, VelocityDir) > 0.3f)
							{
								bShouldStopForMonster = true;
								break;
							}
						}
					}
				}

				if (bShouldStopForMonster)
				{
					// 단순히 멈추는 대신 군중에서 벗어나기 위해 회피 시도
					if (Blackboard)
					{
						Blackboard->SetValueAsBool(ShouldEvadeKey, true);
						// AI를 군중 중심에서 반대 방향으로 밀어내기 위한 가상 목표 설정
						Blackboard->SetValueAsVector(TargetLocationKey, ControlledPawn->GetActorLocation() + ControlledPawn->GetActorForwardVector() * -100.0f);
					}
					StopMovement();
					StationaryTime = 0.0f; // 끼임 감지 로직 중복 실행 방지를 위한 리셋
				}
			}
		}
	}
}

void AGS_SeekerAIController::UpdateAI(float DeltaTime)
{
	// 컨트롤러 작업에 대한 로그 업데이트 등을 여기서 수행할 수 있습니다.
	// 현재 대부분의 유틸리티 AI 업데이트는 UGS_BTS_SeekerUtility에서 처리됩니다.
}

void AGS_SeekerAIController::HandleStuckDetection(float DeltaTime, APawn* ControlledPawn, AGS_Seeker* Seeker, UWorld* World)
{
	if (!ControlledPawn || !World)
	{
		return;
	}

	float DistanceMoved = FVector::Dist(ControlledPawn->GetActorLocation(), LastPosition);
	bool bIsTryingToMove = GetMoveStatus() == EPathFollowingStatus::Moving;

	TrapCheckTimer += DeltaTime;
	bool bOnDangerousTrap = false;
	if (TrapCheckTimer >= GS_AI::TRAP_CHECK_INTERVAL)
	{
		bOnDangerousTrap = DetectNearbyTraps() != nullptr;
		TrapCheckTimer = 0.0f;
	}

	float StuckThreshold = bOnDangerousTrap ? GS_AI::STUCK_THRESHOLD_ON_TRAP : GS_AI::STUCK_THRESHOLD_NORMAL;

	bool bIsInImportantAction = IsInCombat() || (Seeker && (Seeker->GetAimState() || Seeker->GetDrawState()));

	if (DistanceMoved < 5.0f && bIsTryingToMove && !bIsInImportantAction)
	{
		StationaryTime += DeltaTime;
	}
	else
	{
		StationaryTime = 0.0f;
		LastPosition = ControlledPawn->GetActorLocation();
	}

	if (StationaryTime > StuckThreshold && !bIsEvasionSuppressed)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AI] %s 가 %.1f초 동안 끼어있음 (트랩 위: %s). 탈출 경로 검색 중..."),
		       *ControlledPawn->GetName(), StationaryTime, bOnDangerousTrap ? TEXT("예") : TEXT("아니오"));

		bIsEvasionSuppressed = true;
		EvasionSuppressionTimer = 2.0f; // 빠른 복구를 위해 2.5초에서 2.0초로 단축

		UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (NavSystem)
		{
			FNavLocation EscapeSpot;
			FVector CurrentLoc = ControlledPawn->GetActorLocation();
			FVector Forward = ControlledPawn->GetActorForwardVector();

			// --- 스마트 탈출 로직: 주변 환경 스캔 ---
			// 1. 전방 장애물 확인 (레이캐스트)
			FHitResult Hit;
			FVector RayEnd = CurrentLoc + Forward * 150.0f;
			bool bWallInFront = World->LineTraceSingleByChannel(Hit, CurrentLoc, RayEnd, ECC_Visibility);

			FVector EscapeDir = Forward * -1.0f; // 기본은 후진

			if (bWallInFront)
			{
				// 전방이 막혔다면 좌/우 중 더 트인 곳을 탐색
				FVector Right = ControlledPawn->GetActorRightVector();
				FHitResult RightHit, LeftHit;

				World->LineTraceSingleByChannel(RightHit, CurrentLoc, CurrentLoc + Right * 250.0f, ECC_Visibility);
				World->LineTraceSingleByChannel(LeftHit, CurrentLoc, CurrentLoc - Right * 250.0f, ECC_Visibility);

				// 더 먼 거리에 히트가 발생한(더 트인) 쪽을 선택
				if (RightHit.Distance >= LeftHit.Distance)
				{
					EscapeDir = (Right + Forward * -0.3f).GetSafeNormal();
				}
				else
				{
					EscapeDir = (Right * -1.0f + Forward * -0.3f).GetSafeNormal();
				}
			}

			// 2. 탈출 지점 결정 및 투착
			FVector TestLoc = CurrentLoc + EscapeDir * 350.0f;
			if (NavSystem->ProjectPointToNavigation(TestLoc, EscapeSpot, FVector(200.0f, 200.0f, 200.0f)))
			{
				MoveToLocation(EscapeSpot.Location, 50.0f);
			}
			else if (NavSystem->GetRandomReachablePointInRadius(CurrentLoc, 600.0f, EscapeSpot))
			{
				// 최후의 수단: 주변 랜덤 지점으로 이동
				MoveToLocation(EscapeSpot.Location, 50.0f);
			}
		}
	}

	if (bIsEvasionSuppressed)
	{
		EvasionSuppressionTimer -= DeltaTime;
		if (EvasionSuppressionTimer <= 0.0f)
		{
			bIsEvasionSuppressed = false;
			StationaryTime = 0.0f;
		}
	}

	if (DistanceMoved > 100.0f && !IsInCombat())
	{
		MarkLocationVisited(ControlledPawn->GetActorLocation());
	}
}

void AGS_SeekerAIController::UpdateOrientation(float DeltaTime, APawn* ControlledPawn)
{
	if (!ControlledPawn)
	{
		return;
	}

	bool bIsTryingToMove = GetMoveStatus() == EPathFollowingStatus::Moving;

	if (AActor* FocusActor = GetFocusActor())
	{
		FVector TargetLoc = FocusActor->GetActorLocation();
		FVector Dir = TargetLoc - ControlledPawn->GetActorLocation();
		Dir.Z = 0.0f;
		if (!Dir.IsNearlyZero())
		{
			FRotator TargetRot = Dir.Rotation();
			float InterpSpeed = 12.0f;
			FRotator NewRot = FMath::RInterpTo(ControlledPawn->GetActorRotation(), TargetRot, DeltaTime, InterpSpeed);
			ControlledPawn->SetActorRotation(NewRot);
		}
	}
	else if (bIsTryingToMove)
	{
		FVector Velocity = ControlledPawn->GetVelocity();
		if (Velocity.SizeSquared() > 100.0f)
		{
			FRotator TargetRot = Velocity.Rotation();
			TargetRot.Pitch = 0.0f;
			TargetRot.Roll = 0.0f;
			FRotator NewRot = FMath::RInterpTo(ControlledPawn->GetActorRotation(), TargetRot, DeltaTime, 8.0f);
			ControlledPawn->SetActorRotation(NewRot);
		}
	}
}

void AGS_SeekerAIController::UpdateThreatAssessment()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
		return;

	CurrentThreats.Empty();

	UWorld* World = GetWorld();
	UGS_ActorRegistrySubsystem* Registry = World ? World->GetSubsystem<UGS_ActorRegistrySubsystem>() : nullptr;
	if (!Registry)
		return;

	TMap<AActor*, int32> AllyTargetCount;
	const TArray<TWeakObjectPtr<AGS_Seeker>>& AllSeekers = Registry->GetSeekers();
	for (const TWeakObjectPtr<AGS_Seeker>& SeekerPtr : AllSeekers)
	{
		AGS_Seeker* OtherSeeker = SeekerPtr.Get();
		if (!IsValid(OtherSeeker) || OtherSeeker == ControlledPawn)
		{
			continue;
		}

		// 다른 시커의 AI 컨트롤러 확인
		if (AGS_SeekerAIController* OtherAIC = Cast<AGS_SeekerAIController>(OtherSeeker->GetController()))
		{
			if (AActor* OtherTarget = OtherAIC->CurrentTargetEnemy.Get())
			{
				AllyTargetCount.FindOrAdd(OtherTarget, 0)++;
			}
		}
	}

	const TArray<TWeakObjectPtr<AGS_Monster>>& FoundMonsters = Registry->GetMonsters();
	for (const TWeakObjectPtr<AGS_Monster>& MonsterPtr : FoundMonsters)
	{
		AGS_Monster* Monster = MonsterPtr.Get();
		if (IsValid(Monster) && !Monster->IsDead())
		{
			float Distance = FVector::Dist(ControlledPawn->GetActorLocation(), Monster->GetActorLocation());
			if (Distance <= SightRadius)
			{
				float ThreatLevel = 0.0f;

				ThreatLevel += FMath::Clamp(1.0f - (Distance / SightRadius), 0.0f, 1.0f) * 0.4f;

				bool bIsAggro = false;
				AActor* MonsterTarget = nullptr;
				if (AController* C = Monster->GetController())
				{
					if (AAIController* MonsterAIC = Cast<AAIController>(C))
					{
						MonsterTarget = MonsterAIC->GetFocusActor();
						bIsAggro = (MonsterTarget == ControlledPawn);
					}
				}

				if (bIsAggro)
				{
					ThreatLevel += 0.3f;
				}

				// 시야선 체크 (벽에 끼었을 때를 대비해 근접 시 예외 처리)
				if (!LineOfSightTo(Monster))
				{
					// 거리가 800 유닛 이상으로 멀면 위협에서 사실상 제외 (0.1배)
					if (Distance > 800.0f)
					{
						ThreatLevel *= 0.1f;
					}
					else
					{
						// 거리가 가까우면(벽 너머) 위협 유지하되 점수만 약간 감쇄
						ThreatLevel *= 0.7f;
					}
				}

				float MaxHP = 100.0f;
				float CurrentHP = 100.0f;
				if (UGS_StatComp* Stat = Monster->GetStatComp())
				{
					MaxHP = Stat->GetMaxHealth();
					CurrentHP = Stat->GetCurrentHealth();
					if (MaxHP > 0.0f)
					{
						ThreatLevel += (CurrentHP / MaxHP) * 0.2f;
					}
				}

				int32 AllyCount = AllyTargetCount.FindRef(Monster);
				if (AllyCount > 0)
				{
					float DistributionPenalty = (AllyCount == 1) ? 0.8f : (AllyCount == 2 ? 0.6f : 0.4f);

					if (CurrentHP < MaxHP * 0.3f)
					{
						DistributionPenalty = 1.0f;
					}

					ThreatLevel *= DistributionPenalty;

					for (const TWeakObjectPtr<AGS_Seeker>& AllyPtr : AllSeekers)
					{
						AGS_Seeker* Ally = AllyPtr.Get();
						if (Ally && Ally != ControlledPawn && MonsterTarget == Ally && Cast<AGS_Merci>(Ally))
						{
							ThreatLevel *= 1.5f;
							break;
						}
					}

					// 나를 공격하고 있는 적이면 페널티 완화 (자기 방어 최우선)
					if (bIsAggro)
					{
						ThreatLevel *= 1.8f;
					}
				}

				CurrentThreats.Add(FGS_ThreatData(Monster, FMath::Clamp(ThreatLevel, 0.0f, 1.0f), Distance, bIsAggro));
			}
		}
	}

	// ========================================
	// 가디언(드라카) 위협 평가 - 시커AI의 최우선 적
	// ========================================
	if (AGS_Guardian* Guardian = Registry->GetGuardian())
	{
		if (!Guardian->IsDead())
		{
			float Distance = FVector::Dist(ControlledPawn->GetActorLocation(), Guardian->GetActorLocation());
			if (Distance <= SightRadius)
			{
				// 가디언은 최우선 타겟이므로 높은 기본 위협 수준 설정
				float ThreatLevel = 0.8f; // 기본값을 높게 설정

				// 거리 기반 추가 위협
				ThreatLevel += FMath::Clamp(1.0f - (Distance / SightRadius), 0.0f, 1.0f) * 0.2f;

				// 시야선 체크
				if (!LineOfSightTo(Guardian))
				{
					if (Distance > 800.0f)
					{
						ThreatLevel *= 0.5f; // 가디언은 멀어도 중요하므로 페널티 완화
					}
					else
					{
						ThreatLevel *= 0.8f;
					}
				}

				// 가디언은 항상 어그로 상태로 간주 (게임 로직상 항상 위협)
				CurrentThreats.Add(FGS_ThreatData(Guardian, FMath::Clamp(ThreatLevel, 0.0f, 1.0f), Distance, true));
			}
		}
	}

	CurrentThreats.Sort([](const FGS_ThreatData& A, const FGS_ThreatData& B)
	                    { return A.ThreatLevel > B.ThreatLevel; });

	MaxThreat = CurrentThreats.Num() > 0 ? CurrentThreats[0].ThreatLevel : 0.0f;
}

void AGS_SeekerAIController::UpdateUtilityScores()
{
	UtilityScores.Empty();

	UtilityScores.Add(FGS_UtilityScore(ESeekerBehavior::Explore, CalculateExploreUtility()));
	UtilityScores.Add(FGS_UtilityScore(ESeekerBehavior::Combat, CalculateCombatUtility()));
	UtilityScores.Add(FGS_UtilityScore(ESeekerBehavior::Heal, CalculateHealUtility()));
	UtilityScores.Add(FGS_UtilityScore(ESeekerBehavior::Evade, CalculateEvadeUtility()));
	UtilityScores.Add(FGS_UtilityScore(ESeekerBehavior::Revive, CalculateReviveUtility()));
	UtilityScores.Add(FGS_UtilityScore(ESeekerBehavior::Tactical, CalculateTacticalUtility()));

	UtilityScores.Sort([](const FGS_UtilityScore& A, const FGS_UtilityScore& B)
	                   { return A.Score > B.Score; });
}

ESeekerBehavior AGS_SeekerAIController::SelectBestBehavior()
{
	if (UtilityScores.Num() == 0)
	{
		return ESeekerBehavior::Idle;
	}

	ESeekerBehavior BestBehavior = UtilityScores[0].Behavior;

	if (Blackboard)
	{
		CurrentBehavior = BestBehavior;

		// 🟢 Tactical Behavior 선택 시 EQS 쿼리 실행
		if (BestBehavior == ESeekerBehavior::Tactical)
		{
			RunTacticalQuery();
		}
	}

	return BestBehavior;
}

AActor* AGS_SeekerAIController::GetHighestPriorityThreat() const
{
	return CurrentThreats.Num() > 0 ? CurrentThreats[0].ThreatActor.Get() : nullptr;
}

float AGS_SeekerAIController::GetUtilityScoreForBehavior(ESeekerBehavior Behavior) const
{
	for (const FGS_UtilityScore& Score : UtilityScores)
	{
		if (Score.Behavior == Behavior)
		{
			return Score.Score;
		}
	}
	return 0.0f;
}

void AGS_SeekerAIController::GetTeamTacticalIntel(float Radius, bool& bAllyInTrouble, AActor*& Attacker, bool& bAllyNearby)
{
	bAllyInTrouble = false;
	Attacker = nullptr;
	bAllyNearby = false;

	APawn* MyPawn = GetPawn();
	if (!MyPawn)
		return;

	UGS_ActorRegistrySubsystem* Registry = MyPawn->GetWorld()->GetSubsystem<UGS_ActorRegistrySubsystem>();
	if (!Registry)
		return;

	const TArray<TWeakObjectPtr<AGS_Seeker>>& Allies = Registry->GetSeekers();
	for (const auto& AllyPtr : Allies)
	{
		AGS_Seeker* Ally = AllyPtr.Get();
		if (!Ally || Ally == MyPawn || FVector::Dist(MyPawn->GetActorLocation(), Ally->GetActorLocation()) > Radius)
			continue;

		bAllyNearby = true;

		// 아군이 우리가 알고 있는 적의 타겟이 되었는지 확인
		for (const FGS_ThreatData& Threat : CurrentThreats)
		{
			if (!Threat.ThreatActor.IsValid())
				continue;

			if (AGS_Monster* Monster = Cast<AGS_Monster>(Threat.ThreatActor.Get()))
			{
				if (AAIController* MonsterAIC = Cast<AAIController>(Monster->GetController()))
				{
					if (MonsterAIC->GetFocusActor() == Ally)
					{
						bAllyInTrouble = true;
						Attacker = Monster;
						break;
					}
				}
			}
		}

		if (bAllyInTrouble)
			break;

		// 아군 HP가 낮은지도 확인
		if (UGS_StatComp* Stat = Ally->GetStatComp())
		{
			if (Stat->GetCurrentHealth() / Stat->GetMaxHealth() < 0.4f)
			{
				bAllyInTrouble = true;
				break;
			}
		}
	}
}

float AGS_SeekerAIController::CalculateExploreUtility()
{
	// 1. 전투 중이면 탐사 불가
	if (IsInCombat() || CurrentTargetEnemy.IsValid())
	{
		return 0.0f;
	}

	// 2. 아군이 전투 중이면 탐사 우선순위 낮춤 (지원 대기)
	UWorld* World = GetWorld();
	if (World)
	{
		UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>();
		if (Registry)
		{
			const TArray<TWeakObjectPtr<AGS_Seeker>>& AllSeekers = Registry->GetSeekers();
			int32 AlliesInCombat = 0;

			for (const TWeakObjectPtr<AGS_Seeker>& SeekerPtr : AllSeekers)
			{
				AGS_Seeker* OtherSeeker = SeekerPtr.Get();
				if (!IsValid(OtherSeeker) || OtherSeeker == GetPawn())
				{
					continue;
				}

				// 다른 시커가 전투 중인지 확인
				if (AGS_SeekerAIController* OtherAIC = Cast<AGS_SeekerAIController>(OtherSeeker->GetController()))
				{
					if (OtherAIC->IsInCombat() || OtherAIC->CurrentTargetEnemy.IsValid())
					{
						AlliesInCombat++;
					}
				}
			}

			// 아군 1명 이상이 전투 중이면 탐사 우선순위 낮춤
			if (AlliesInCombat > 0)
			{
				// 근처에 적이 있는지 확인 (지원 가능한지)
				if (CurrentThreats.Num() > 0)
				{
					return GS_AI::EXPLORE_UTILITY_ALLY_IN_COMBAT_PENALTY_THREAT;
				}
				// 근처에 적도 없으면 조금 더 높은 탐사 점수
				return GS_AI::EXPLORE_UTILITY_ALLY_IN_COMBAT_PENALTY_NO_THREAT;
			}
		}
	}

	// 3. 기본 탐사 점수
	float BaseScore = GS_AI::EXPLORE_UTILITY_BASE;

	// 4. 목표 도달 여부 확인
	UBlackboardComponent* BB = GetBlackboardComponent();
	bool bHasReachedGoal = BB && BB->GetValueAsBool(HasReachedGoalKey);
	if (bHasReachedGoal)
	{
		return GS_AI::EXPLORE_UTILITY_REACHED_GOAL;
	}

	// 5. 탐사 타겟 존재 여부
	FVector ExploreTarget = BB ? BB->GetValueAsVector(ExplorationTargetKey) : FVector::ZeroVector;
	bool bHasExploreTarget = !ExploreTarget.IsNearlyZero();

	if (bHasExploreTarget)
	{
		BaseScore += GS_AI::EXPLORE_UTILITY_TARGET_BONUS;
	}

	// 최종 점수: 0.7 ~ 1.2 범위
	return FMath::Clamp(BaseScore, 0.7f, 1.2f);
}

float AGS_SeekerAIController::CalculateCombatUtility()
{
	// 1. 타겟이 없으면 combat 불가
	AActor* Target = GetHighestPriorityThreat();
	if (!Target)
	{
		return 0.0f;
	}

	// 2. 기본 전투 점수
	float BaseScore = GS_AI::COMBAT_UTILITY_BASE;

	// 3. MaxThreat 기반 가중치
	float ThreatMultiplier = FMath::Clamp(MaxThreat * GS_AI::COMBAT_UTILITY_THREAT_SCALE, 0.0f, 1.2f);

	// 4. 거리 및 캐릭터별 가중치
	float Distance = GetDistanceToTarget(Target);
	float DistanceScore = 0.0f;
	float CharacterBonus = 0.0f;

	AGS_AISeeker* Seeker = GetControlledSeeker();
	if (Seeker)
	{
		ESeekerAIType SeekerType = Seeker->GetSeekerType();

		if (SeekerType == ESeekerAIType::Merci)
		{
			// Merci (Archer): 원거리 교전 선호
			if (Distance > AttackRange * 0.8f && Distance < AttackRange * 1.5f)
				DistanceScore = GS_AI::COMBAT_UTILITY_MERCI_OPTIMAL_BONUS;
			else if (Distance < 400.0f)
				DistanceScore = GS_AI::COMBAT_UTILITY_MERCI_CLOSE_PENALTY;

			CharacterBonus = 0.2f;
		}
		else
		{
			// Ares/Chan (Melee): 근접 교전 선호
			if (Distance < AttackRange * 1.2f)
				DistanceScore = 0.5f;
			else if (Distance < AttackRange * 2.5f)
				DistanceScore = 0.2f;
		}
	}

	if (Seeker && !Seeker->IsRangedSeeker() && IsTargetNearTrap(Target, 400.0f))
	{
		DistanceScore += GS_AI::COMBAT_UTILITY_MELEE_TRAP_PENALTY;
	}

	// 최종 점수: 1.3 ~ 2.2 범위 (전술적 상황 반영)
	float FinalScore = BaseScore + (ThreatMultiplier * 0.3f) + DistanceScore + CharacterBonus;
	return FMath::Clamp(FinalScore, 0.5f, GS_AI::UTILITY_SCORE_MAX_COMBAT);
}

float AGS_SeekerAIController::CalculateHealUtility()
{
	if (!IsValid(CachedStatComp))
	{
		return 0.0f;
	}

	float HealthPercent = 1.0f;
	float MaxHP = CachedStatComp->GetMaxHealth();
	if (MaxHP > 0.0f)
	{
		HealthPercent = CachedStatComp->GetCurrentHealth() / MaxHP;
	}

	// 체력이 떨어질수록 유틸리티가 기하급수적으로 증가
	float Utility = FMath::Pow(1.0f - HealthPercent, 2.0f);

	// 위기 상황 임계값 보너스 - 생존을 위해 최우선 순위로 설정
	if (HealthPercent < CriticalHealThreshold)
	{
		Utility = 2.5f; // 생존 최우선 순위
	}
	else if (HealthPercent < HealThreshold)
	{
		Utility += 0.4f;
	}

	// 위기 상태가 아닌데 집중 포화 중이면 힐 순위 약간 낮춤 (전투 지속)
	if (HealthPercent >= CriticalHealThreshold && CurrentThreats.Num() > 0 && CurrentThreats[0].bIsAggro)
	{
		Utility *= 0.7f;
	}

	return FMath::Clamp(Utility, 0.0f, GS_AI::UTILITY_SCORE_MAX_HEAL);
}

float AGS_SeekerAIController::CalculateEvadeUtility()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return 0.0f;
	}

	// 0. 회피 억제 중이면 유틸리티 0
	if (bIsEvasionSuppressed)
	{
		return 0.0f;
	}

	float FinalUtility = 0.0f;

	// ========================================
	// PART 1: 트랩 기반 회피
	// ========================================
	AGS_TrapBase* NearestTrap = DetectNearbyTraps();
	if (IsValid(NearestTrap))
	{
		FVector PawnLocation = ControlledPawn->GetActorLocation();
		float Distance = FVector::Dist(PawnLocation, NearestTrap->GetActorLocation());

		// 거리 기반 기본 위험도 (가까울수록 위험)
		float DistanceScore = FMath::Clamp(1.0f - (Distance / TrapDetectionRadius), 0.0f, 1.0f);

		// HP 기반 가중치 (체력 낮으면 회피 더 중요)
		float HealthPercent = 1.0f;
		if (UGS_StatComp* StatComp = ControlledPawn->FindComponentByClass<UGS_StatComp>())
		{
			float MaxHP = StatComp->GetMaxHealth();
			if (MaxHP > 0.0f)
			{
				HealthPercent = StatComp->GetCurrentHealth() / MaxHP;
			}
		}
		float HealthWeight = FMath::Lerp(1.5f, 1.0f, HealthPercent);

		// 전투 중 가중치
		float CombatWeight = IsInCombat() ? 1.3f : 1.0f;

		float TrapUtility = DistanceScore * HealthWeight * CombatWeight;

		// 근접 트랩 부스트
		float ProximityBoost = Distance < 150.0f ? 1.5f : (Distance < 300.0f ? 1.3f : 1.2f);
		TrapUtility *= ProximityBoost;

		FinalUtility = FMath::Max(FinalUtility, TrapUtility);
	}

	// ========================================
	// PART 2: 원거리 몬스터 공격 회피 (NEW!)
	// ========================================
	if (IsInCombat())
	{
		// 전투 중 오래 서있으면 위치 변경 필요
		// StationaryTime이 1.5초 이상이면 회피 유틸리티 상승
		if (StationaryTime > GS_AI::EVADE_UTILITY_STATIONARY_THRESHOLD)
		{
			float StationaryPenalty = FMath::Clamp((StationaryTime - GS_AI::EVADE_UTILITY_STATIONARY_THRESHOLD) / 2.0f, 0.0f, 1.0f);
			FinalUtility = FMath::Max(FinalUtility, StationaryPenalty * GS_AI::EVADE_UTILITY_STATIONARY_SCALE);
		}


		// 원거리 몬스터 근처에 있는지 확인
		UWorld* World = GetWorld();
		if (World)
		{
			TArray<FOverlapResult> Overlaps;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(ControlledPawn);

			// 600 유닛 내 몬스터 탐지
			if (World->OverlapMultiByChannel(
			        Overlaps,
			        ControlledPawn->GetActorLocation(),
			        FQuat::Identity,
			        ECollisionChannel::ECC_Pawn,
			        FCollisionShape::MakeSphere(600.0f),
			        QueryParams))
			{
				int32 RangedThreatCount = 0;
				for (const FOverlapResult& Overlap : Overlaps)
				{
					if (AGS_Monster* Monster = Cast<AGS_Monster>(Overlap.GetActor()))
					{
						// TODO: Monster가 원거리 타입인지 확인하는 로직 추가 가능
						// 현재는 모든 몬스터를 잠재적 위협으로 간주
						RangedThreatCount++;
					}
				}

				// 주변에 몬스터가 많으면 회피 유틸리티 증가
				if (RangedThreatCount > 0)
				{
					float ThreatUtility = FMath::Clamp(RangedThreatCount * GS_AI::EVADE_UTILITY_RANGED_THREAT_UNIT, 0.0f, 1.2f);

					// 체력이 낮으면 더 적극적으로 회피
					float HealthPercent = 1.0f;
					if (UGS_StatComp* StatComp = ControlledPawn->FindComponentByClass<UGS_StatComp>())
					{
						float MaxHP = StatComp->GetMaxHealth();
						if (MaxHP > 0.0f)
						{
							HealthPercent = StatComp->GetCurrentHealth() / MaxHP;
						}
					}

					if (HealthPercent < 0.5f)
					{
						ThreatUtility *= 1.5f; // 체력 50% 미만이면 1.5배
					}

					FinalUtility = FMath::Max(FinalUtility, ThreatUtility);
				}
			}
		}
	}

	// ========================================
	// PART 3: 긴급 도망 (체력 위기)
	// ========================================
	float HealthPercent = 1.0f;
	if (UGS_StatComp* StatComp = ControlledPawn->FindComponentByClass<UGS_StatComp>())
	{
		float MaxHP = StatComp->GetMaxHealth();
		if (MaxHP > 0.0f)
		{
			HealthPercent = StatComp->GetCurrentHealth() / MaxHP;
		}
	}

	if (HealthPercent < 0.2f && IsInCombat())
	{
		FinalUtility += GS_AI::EVADE_UTILITY_EMERGENCY_BONUS; // 긴급 도망 점수 추가
	}

	return FMath::Clamp(FinalUtility, 0.0f, GS_AI::UTILITY_SCORE_MAX_EVADE);
}


float AGS_SeekerAIController::CalculateReviveUtility()
{
	UWorld* World = GetWorld();
	UGS_ActorRegistrySubsystem* Registry = World ? World->GetSubsystem<UGS_ActorRegistrySubsystem>() : nullptr;
	if (!Registry)
	{
		return 0.0f;
	}

	AActor* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return 0.0f;
	}

	// 1. 본인 HP 체크 (체력 30% 이하면 구조 불가)
	float MyHealthPercent = 1.0f;
	if (UGS_StatComp* MyStatComp = MyPawn->FindComponentByClass<UGS_StatComp>())
	{
		float MaxHP = MyStatComp->GetMaxHealth();
		if (MaxHP > 0.0f)
		{
			MyHealthPercent = MyStatComp->GetCurrentHealth() / MaxHP;
		}

		if (MyHealthPercent < 0.3f)
		{
			return 0.0f; // 본인 위험하면 구조 불가
		}
	}

	const TArray<TWeakObjectPtr<AGS_Seeker>>& Seekers = Registry->GetSeekers();

	float MaxReviveUtility = 0.0f;
	AGS_Seeker* BestTarget = nullptr;

	for (const TWeakObjectPtr<AGS_Seeker>& SeekerPtr : Seekers)
	{
		AGS_Seeker* Seeker = SeekerPtr.Get();
		if (!IsValid(Seeker) || Seeker == MyPawn || !Seeker->IsInDyingState())
		{
			continue;
		}

		FVector MyLocation = MyPawn->GetActorLocation();
		FVector TargetLocation = Seeker->GetActorLocation();
		float Dist = FVector::Dist(MyLocation, TargetLocation);

		// 2. 거리 기반 점수 (가까울수록 좋음)
		float DistanceScore = 1.0f - FMath::Clamp(Dist / GS_AI::REVIVE_UTILITY_DISTANCE_MAX, 0.0f, 1.0f);

		// 3. 남은 구조 시간 고려 (8초 내 도달 여부)
		// TODO: 실제 다운 타이머 컴포넌트가 있다면 해당 값 사용
		// 현재는 거리 기반으로 도달 가능 여부만 판단
		float EstimatedTravelTime = Dist / 600.0f; // 600cm/s 이동 속도 가정
		float TimeUrgency = EstimatedTravelTime < GS_AI::REVIVE_UTILITY_URGENCY_TIME_LIMIT ? 1.0f : 0.3f; // 8초 내 도달 가능하면 정상, 아니면 우선순위 낮춤

		// 4. 전투 중 페널티 (전투 중엔 구조 위험)
		float CombatPenalty = IsInCombat() ? GS_AI::REVIVE_UTILITY_COMBAT_PENALTY : 1.0f;

		// 5. 주변 위협도 평가
		float ThreatPenalty = 1.0f;
		for (const FGS_ThreatData& Threat : CurrentThreats)
		{
			if (Threat.ThreatActor.IsValid())
			{
				float ThreatDistToTarget = FVector::Dist(TargetLocation, Threat.ThreatActor->GetActorLocation());
				if (ThreatDistToTarget < GS_AI::REVIVE_UTILITY_THREAT_DISTANCE_LIMIT)
				{
					// 다운된 아군 근처에 적이 있으면 위험
					ThreatPenalty *= GS_AI::REVIVE_UTILITY_THREAT_PROXIMITY_PENALTY;
				}
			}
		}

		// 6. 최종 유틸리티 계산
		// 기본 높은 우선순위 (1.0) + 거리 보너스 (최대 +1.0)
		float BaseUtility = 1.0f + DistanceScore;
		float FinalUtility = BaseUtility * TimeUrgency * CombatPenalty * ThreatPenalty;

		// 7. 본인 HP가 높으면 구조 우선순위 증가
		float HealthBonus = FMath::Lerp(1.0f, 1.3f, MyHealthPercent); // HP 높으면 최대 1.3배
		FinalUtility *= HealthBonus;

		if (FinalUtility > MaxReviveUtility)
		{
			MaxReviveUtility = FinalUtility;
			BestTarget = Seeker;
		}
	}

	// 8. 최적 구조 대상 저장
	if (BestTarget && Blackboard)
	{
		Blackboard->SetValueAsObject(DownedAllyKey, BestTarget);
	}

	return FMath::Clamp(MaxReviveUtility, 0.0f, GS_AI::UTILITY_SCORE_MAX_REVIVE);
}

float AGS_SeekerAIController::CalculateTacticalUtility()
{
	// 1. 전투 중이거나 타겟이 있어야 전술적 이동 고려
	AActor* Target = CurrentTargetEnemy.Get();
	if (!Target || !IsInCombat())
	{
		return 0.0f;
	}

	AGS_AISeeker* Seeker = GetControlledSeeker();
	if (!Seeker)
		return 0.0f;

	float Score = 0.0f;
	float Distance = GetDistanceToTarget(Target);
	ESeekerAIType SeekerType = Seeker->GetSeekerType();

	// 2. 캐릭터 타입별 상황 평가
	if (SeekerType == ESeekerAIType::Merci)
	{
		// Merci: 위협 노출 시 엄폐물 검색 가중치 상향
		bool bIsTargetAggroOnMe = false;
		if (AGS_Monster* Monster = Cast<AGS_Monster>(Target))
		{
			if (AController* C = Monster->GetController())
			{
				if (AAIController* MonsterAIC = Cast<AAIController>(C))
				{
					bIsTargetAggroOnMe = (MonsterAIC->GetFocusActor() == GetPawn());
				}
			}
		}

		if (bIsTargetAggroOnMe)
			Score += GS_AI::TACTICAL_UTILITY_MERCI_AGGRO_BONUS; // 내가 타겟이면 엄폐 중요도 상승
		if (Distance < 800.0f)
			Score += 0.5f; // 거리가 너무 가까우면 재배치 유도
	}
	else
	{
		// Melee (Ares/Chan): 아군이 어그로를 끌고 있으면 우회(Flank) 가중치 상향
		bool bAllyHasAggro = false;
		if (AGS_Monster* Monster = Cast<AGS_Monster>(Target))
		{
			if (AController* C = Monster->GetController())
			{
				if (AAIController* MonsterAIC = Cast<AAIController>(C))
				{
					AActor* MonsterTarget = MonsterAIC->GetFocusActor();
					bAllyHasAggro = (MonsterTarget && MonsterTarget != GetPawn());
				}
			}
		}

		if (bAllyHasAggro)
			Score += 1.2f; // 아군이 탱킹 중이면 옆이나 뒤를 잡으러 함

		if (IsTargetNearTrap(Target, 500.0f))
		{
			Score += GS_AI::TACTICAL_UTILITY_MELEE_TRAP_BONUS; // Strongly prefer tactical repositioning over suicide attack
		}
	}

	// 3. 전투 지속 시간에 따른 전술적 유연성
	// 너무 오래 한 자리에 있으면 점수를 높여 이동 유도
	if (StationaryTime > GS_AI::TACTICAL_UTILITY_STATIONARY_TIME_LIMIT)
	{
		Score += GS_AI::TACTICAL_UTILITY_STATIONARY_BONUS;
	}

	return FMath::Clamp(Score, 0.0f, GS_AI::UTILITY_SCORE_MAX_TACTICAL);
}

void AGS_SeekerAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor)
	{
		return;
	}

	// 적 캐릭터 확인 (몬스터 또는 가디언)
	AGS_Character* EnemyCharacter = nullptr;
	bool bIsEnemy = false;

	// 몬스터(적) 확인
	if (AGS_Monster* Monster = Cast<AGS_Monster>(Actor))
	{
		EnemyCharacter = Monster;
		bIsEnemy = true;
	}
	// 가디언(드라카) 확인 - 시커 AI에게는 적
	else if (AGS_Guardian* Guardian = Cast<AGS_Guardian>(Actor))
	{
		EnemyCharacter = Guardian;
		bIsEnemy = true;
	}

	if (bIsEnemy && EnemyCharacter)
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			// 적 감지됨 - 최적 타겟 평가 및 전환
			AActor* BestPotential = FindBestTarget();
			if (BestPotential)
			{
				SetTargetEnemy(BestPotential);
			}
		}
		else
		{
			// 시야를 잃음 - 그러나 가까이 있으면 계속 추적
			APawn* ControlledPawn = GetPawn();
			if (ControlledPawn && !EnemyCharacter->IsDead())
			{
				float Distance = FVector::Dist(ControlledPawn->GetActorLocation(), EnemyCharacter->GetActorLocation());

				// 600 유닛 이내면 벽에 끼어도 계속 인식
				// (시야가 막혀도 소리나 근접 센서로 인지한다고 가정)
				if (Distance < 600.0f)
				{
					// 현재 타겟이 없으면 이 적을 타겟으로
					if (!CurrentTargetEnemy.IsValid())
					{
						SetTargetEnemy(EnemyCharacter);
					}
					// 시야가 막혀도 가까우면 마지막 위치 저장하지 않음 (계속 추적)
					return;
				}
			}

			// 현재 타겟이 이 액터면 마지막 위치 저장
			if (CurrentTargetEnemy == Actor)
			{
				if (Blackboard)
				{
					Blackboard->SetValueAsVector(LastKnownEnemyLocationKey, Actor->GetActorLocation());
				}
			}
		}
	}
}

void AGS_SeekerAIController::SetTargetEnemy(AActor* NewTarget)
{
	CurrentTargetEnemy = NewTarget;

	if (Blackboard)
	{
		Blackboard->SetValueAsObject(TargetEnemyKey, NewTarget);
		Blackboard->SetValueAsBool(IsInCombatKey, NewTarget != nullptr);

		if (NewTarget)
		{
			Blackboard->SetValueAsVector(TargetLocationKey, NewTarget->GetActorLocation());
			SetFocus(NewTarget);
		}
		else
		{
			ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
}

void AGS_SeekerAIController::ClearTargetEnemy()
{
	CurrentTargetEnemy = nullptr;

	if (Blackboard)
	{
		Blackboard->ClearValue(TargetEnemyKey);
		Blackboard->SetValueAsBool(IsInCombatKey, false);
	}

	ClearFocus(EAIFocusPriority::Gameplay);
}

void AGS_SeekerAIController::SetGoalActor(AGS_AIGoalTrigger* GoalTrigger)
{
	CurrentGoal = GoalTrigger;

	if (Blackboard && GoalTrigger)
	{
		Blackboard->SetValueAsObject(GoalActorKey, GoalTrigger);
		Blackboard->SetValueAsVector(ExplorationTargetKey, GoalTrigger->GetActorLocation());
	}
}

void AGS_SeekerAIController::OnGoalReached()
{
	if (Blackboard)
	{
		Blackboard->SetValueAsBool(HasReachedGoalKey, true);
	}

	if (ControlledSeeker.IsValid())
	{
		ControlledSeeker->NotifyGoalReached();
	}
}

void AGS_SeekerAIController::UpdateHealthStatus()
{
	if (!Blackboard)
	{
		return;
	}

	float HealthPercent = 1.0f;

	if (IsValid(CachedStatComp))
	{
		float MaxHP = CachedStatComp->GetMaxHealth();
		if (MaxHP > 0.0f)
		{
			HealthPercent = CachedStatComp->GetCurrentHealth() / MaxHP;
		}
	}

	Blackboard->SetValueAsFloat(CurrentHealthPercentKey, HealthPercent);
	Blackboard->SetValueAsBool(ShouldHealKey, HealthPercent <= HealThreshold);
}

void AGS_SeekerAIController::SetNearbyTrap(AGS_TrapBase* Trap)
{
	if (Blackboard && Trap)
	{
		Blackboard->SetValueAsObject(NearbyTrapKey, Trap);
		Blackboard->SetValueAsBool(ShouldEvadeKey, true);
	}
}

void AGS_SeekerAIController::ClearNearbyTrap()
{
	if (Blackboard)
	{
		Blackboard->ClearValue(NearbyTrapKey);
		Blackboard->SetValueAsBool(ShouldEvadeKey, false);
	}
}

bool AGS_SeekerAIController::IsInCombat() const
{
	return CurrentTargetEnemy.IsValid();
}

void AGS_SeekerAIController::ResumeMovement()
{
	if (UPathFollowingComponent* PFC = GetPathFollowingComponent())
	{
		PFC->ResumeMove();
	}
}

bool AGS_SeekerAIController::ShouldHeal() const
{
	if (!Blackboard)
	{
		return false;
	}
	return Blackboard->GetValueAsBool(ShouldHealKey);
}

bool AGS_SeekerAIController::ShouldEvade() const
{
	if (bIsEvasionSuppressed)
	{
		return false;
	}

	if (!Blackboard)
	{
		return false;
	}
	return Blackboard->GetValueAsBool(ShouldEvadeKey);
}

bool AGS_SeekerAIController::FindNewExplorationTarget()
{
	// 오랫동안 끼어있었다면 이동 범위를 넓혀서 탈출 시도
	bool bIsStuck = (StationaryTime > 2.0f);
	float CurrentRadius = bIsStuck ? (ExplorationRadius * 2.0f) : ExplorationRadius;

	FVector NewTarget = FVector::ZeroVector;
	int32 MaxAttempts = bIsStuck ? 1 : 3; // stuck 상태가 아니면 3번 시도

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		NewTarget = GetRandomPointInNavigableRadius(CurrentRadius);

		// 유효한 타겟을 찾았고, stuck 상태이거나 방문하지 않은 곳이면 사용
		if (!NewTarget.IsZero())
		{
			if (bIsStuck || !HasVisitedLocation(NewTarget))
			{
				break; // 좋은 타겟 찾음!
			}
		}

		// 마지막 시도에서도 실패하면 방문 여부 무시
		if (Attempt == MaxAttempts - 1 && !NewTarget.IsZero())
		{
			UE_LOG(LogTemp, Warning, TEXT("[SeekerAI] %s: All targets visited, reusing old location"), *GetNameSafe(GetPawn()));
			break; // 방문했어도 사용
		}
	}

	// If truly stuck, clear some history to allow re-exploration of old areas
	if (bIsStuck && VisitedLocations.Num() > 5)
	{
		VisitedLocations.RemoveAt(0, FMath::Min(5, VisitedLocations.Num()));
		UE_LOG(LogTemp, Warning, TEXT("[SeekerAI] %s is stuck! Clearing visited history to find a path."), *GetNameSafe(GetPawn()));
	}

	if (!NewTarget.IsZero())
	{
		if (Blackboard)
		{
			Blackboard->SetValueAsVector(ExplorationTargetKey, NewTarget);
		}
		StationaryTime = 0.0f;
		return true;
	}

	if (VisitedLocations.Num() > 10)
	{
		VisitedLocations.RemoveAt(0, 5);
		UE_LOG(LogTemp, Warning, TEXT("[SeekerAI] %s: Failed to find target, clearing part of history and retrying"), *GetPawn()->GetName());

		NewTarget = GetRandomPointInNavigableRadius(CurrentRadius);
		if (!NewTarget.IsZero())
		{
			if (Blackboard)
			{
				Blackboard->SetValueAsVector(ExplorationTargetKey, NewTarget);
			}
			StationaryTime = 0.0f;
			return true;
		}
	}

	// 🔴 긴급 대체 로직: 이동 가능한 지점을 전혀 찾지 못한 경우, 현재 위치 주변에서 강제로 찾음
	UE_LOG(LogTemp, Error, TEXT("[SeekerAI) %s: 치명적: 탐사 타겟을 완전히 찾지 못했습니다!"), *GetPawn()->GetName());

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSystem && GetPawn())
	{
		FNavLocation Loc;
		if (NavSystem->GetRandomReachablePointInRadius(GetPawn()->GetActorLocation(), 1000.0f, Loc))
		{
			UE_LOG(LogTemp, Warning, TEXT("[SeekerAI) %s: 로컬 검색을 통해 긴급 대체 타겟을 찾았습니다."), *GetPawn()->GetName());
			if (Blackboard)
				Blackboard->SetValueAsVector(ExplorationTargetKey, Loc.Location);
			return true;
		}
	}

	return false;
}

FVector AGS_SeekerAIController::GetRandomPointInNavigableRadius(float Radius) const
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
		return FVector::ZeroVector;

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
		return FVector::ZeroVector;

	// 최적의 지점을 찾기 위해 여러 후보군 생성
	TArray<FNavLocation> Candidates;
	for (int32 i = 0; i < 10; i++)
	{
		FNavLocation Loc;
		if (NavSystem->GetRandomReachablePointInRadius(ControlledPawn->GetActorLocation(), Radius, Loc))
		{
			Candidates.Add(Loc);
		}
	}

	// 대체 수단 1: 후보군이 없으면 탐색 반경 확장
	if (Candidates.Num() == 0)
	{
		float LargerRadius = Radius * 2.0f;
		for (int32 i = 0; i < 10; i++)
		{
			FNavLocation Loc;
			if (NavSystem->GetRandomReachablePointInRadius(ControlledPawn->GetActorLocation(), LargerRadius, Loc))
			{
				Candidates.Add(Loc);
			}
		}
	}

	// 대체 수단 2: 여전히 없으면 방문 여부 무시하고 랜덤 지점 탐색
	if (Candidates.Num() == 0)
	{
		FNavLocation Loc;
		if (NavSystem->GetRandomPointInNavigableRadius(ControlledPawn->GetActorLocation(), Radius, Loc))
		{
			Candidates.Add(Loc);
		}
	}

	// 대체 수단 3: 목표 지점(Goal)이 있다면 그 방향으로 이동 시도
	if (Candidates.Num() == 0 && CurrentGoal.IsValid())
	{
		FVector ToGoal = (CurrentGoal->GetActorLocation() - ControlledPawn->GetActorLocation()).GetSafeNormal();
		FVector TargetLocation = ControlledPawn->GetActorLocation() + ToGoal * FMath::Min(Radius, 500.0f);

		FNavLocation Loc;
		if (NavSystem->ProjectPointToNavigation(TargetLocation, Loc))
		{
			Candidates.Add(Loc);
		}
	}

	// 최종 수단: 모든 옵션 실패 시 제로 벡터 반환
	if (Candidates.Num() == 0)
	{
		return FVector::ZeroVector;
	}

	FNavLocation BestCandidate = Candidates[0];
	float BestScore = -MAX_FLT;

	for (const FNavLocation& Candidate : Candidates)
	{
		float Score = 0.0f;

		// 1. 거리 점수 (탐험을 독려하기 위해 멀리 있는 지점 선호)
		float Dist = FVector::Dist(ControlledPawn->GetActorLocation(), Candidate.Location);
		Score += (Dist / Radius) * 30.0f;

		// 2. 미방문 지점 보너스
		if (!HasVisitedLocation(Candidate.Location))
		{
			Score += 100.0f;
		}

		// 3. 목표 방향 보너스
		if (CurrentGoal.IsValid())
		{
			FVector ToGoal = (CurrentGoal->GetActorLocation() - ControlledPawn->GetActorLocation()).GetSafeNormal();
			FVector ToCandidate = (Candidate.Location - ControlledPawn->GetActorLocation()).GetSafeNormal();
			float Dot = FVector::DotProduct(ToGoal, ToCandidate);
			Score += (Dot + 1.0f) * 50.0f; // 목표 방향일수록 높은 점수 부여
		}

		// 4. 트랩 페널티
		if (IsPathBlockedByTrap(Candidate.Location))
		{
			Score -= 200.0f;
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestCandidate = Candidate;
		}
	}

	return BestCandidate.Location;
}

AActor* AGS_SeekerAIController::FindBestTarget() const
{
	// 성능 향상을 위해 미리 계산된 위협 평가 데이터 사용
	return GetHighestPriorityThreat();
}

float AGS_SeekerAIController::GetDistanceToTarget(AActor* Target) const
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn || !Target)
	{
		return MAX_FLT;
	}

	return FVector::Dist(ControlledPawn->GetActorLocation(), Target->GetActorLocation());
}

bool AGS_SeekerAIController::IsTargetInAttackRange(AActor* Target) const
{
	return GetDistanceToTarget(Target) <= AttackRange;
}

AGS_TrapBase* AGS_SeekerAIController::DetectNearbyTraps() const
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return nullptr;
	}

	// 성능을 위해 GetAllActorsOfClass 대신 구체 오버랩 사용
	TArray<AActor*> FoundTraps;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(ControlledPawn);

	UKismetSystemLibrary::SphereOverlapActors(
	    GetWorld(),
	    ControlledPawn->GetActorLocation(),
	    TrapDetectionRadius,
	    ObjectTypes,
	    AGS_TrapBase::StaticClass(),
	    ActorsToIgnore,
	    FoundTraps);

	AGS_TrapBase* NearestTrap = nullptr;
	float NearestDistance = TrapDetectionRadius;

	// 이동 방향 획득
	FVector Velocity = ControlledPawn->GetVelocity();
	bool bIsMoving = Velocity.Size() > 10.0f;
	FVector MoveDir = bIsMoving ? Velocity.GetSafeNormal() : ControlledPawn->GetActorForwardVector();

	for (AActor* Actor : FoundTraps)
	{
		AGS_TrapBase* Trap = Cast<AGS_TrapBase>(Actor);
		if (IsValid(Trap) && Trap->bIsActivated)
		{
			FVector ToTrap = Trap->GetActorLocation() - ControlledPawn->GetActorLocation();
			float Distance = ToTrap.Size();

			// 천장 함정은 옆으로 회피 불가능하므로 빠르게 통과해야 함
			float HeightDifference = FMath::Abs(ToTrap.Z);
			if (HeightDifference > 200.0f)
			{
				continue; // 천장/바닥 함정 무시
			}

			if (Distance < NearestDistance)
			{
				// 경로 관련성 체크: 트랩이 전방에 있는가?
				FVector ToTrapDir = ToTrap.GetSafeNormal();
				float CosTheta = FVector::DotProduct(MoveDir, ToTrapDir);

				// 전방 약 120도 원뿔 범위 내의 트랩만 고려 (Cos(60) = 0.5)
				// 정지/대기 중일 때는 더 넓게 감지 (Cos(90) = 0)
				float Threshold = bIsMoving ? 0.3f : -0.2f;

				if (CosTheta > Threshold)
				{
					NearestDistance = Distance;
					NearestTrap = Trap;
				}
			}
		}
	}

	return NearestTrap;
}

FVector AGS_SeekerAIController::CalculateTrapAvoidanceDirection(AGS_TrapBase* Trap) const
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn || !Trap)
	{
		return FVector::ZeroVector;
	}

	// 트랩에서 멀어지는 방향 획득
	FVector AwayDirection = ControlledPawn->GetActorLocation() - Trap->GetActorLocation();
	AwayDirection.Z = 0.0f;
	AwayDirection.Normalize();

	return AwayDirection;
}

bool AGS_SeekerAIController::IsPathBlockedByTrap(const FVector& Destination) const
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UGS_ActorRegistrySubsystem* Registry = World ? World->GetSubsystem<UGS_ActorRegistrySubsystem>() : nullptr;
	if (!Registry)
	{
		return false;
	}

	FVector Start = ControlledPawn->GetActorLocation();
	FVector Direction = (Destination - Start).GetSafeNormal();
	float PathLength = FVector::Dist(Start, Destination);

	const TArray<TWeakObjectPtr<AGS_TrapBase>>& FoundTraps = Registry->GetTraps();

	for (const TWeakObjectPtr<AGS_TrapBase>& TrapPtr : FoundTraps)
	{
		AGS_TrapBase* Trap = TrapPtr.Get();
		if (IsValid(Trap) && Trap->bIsActivated)
		{
			FVector TrapLocation = Trap->GetActorLocation();

			// 트랩이 이동 경로 근처에 있는지 확인
			FVector ToTrap = TrapLocation - Start;
			float Projection = FVector::DotProduct(ToTrap, Direction);

			if (Projection > 0 && Projection < PathLength)
			{
				FVector ClosestPointOnPath = Start + Direction * Projection;
				float DistanceToPath = FVector::Dist(TrapLocation, ClosestPointOnPath);

				if (DistanceToPath < TrapAvoidanceDistance)
				{
					return true;
				}
			}
		}
	}

	return false;
}

FGenericTeamId AGS_SeekerAIController::GetGenericTeamId() const
{
	return FGenericTeamId(1); // 팀 1: 시커 (플레이어)
}

ETeamAttitude::Type AGS_SeekerAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	if (const APawn* OtherPawn = Cast<APawn>(&Other))
	{
		if (const IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(OtherPawn->GetController()))
		{
			FGenericTeamId OtherTeamId = TeamAgent->GetGenericTeamId();

			if (OtherTeamId == GetGenericTeamId())
			{
				return ETeamAttitude::Friendly;
			}
			else if (OtherTeamId == FGenericTeamId(2)) // 팀 2: 몬스터 (적)
			{
				return ETeamAttitude::Hostile;
			}
			else if (OtherTeamId == FGenericTeamId(0)) // 팀 0: 가디언 (드라카) - 적대
			{
				return ETeamAttitude::Hostile;
			}
		}
	}

	return ETeamAttitude::Neutral;
}

AActor* AGS_SeekerAIController::FindNearestHealthyAlly() const
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
		return nullptr;

	UWorld* World = GetWorld();
	if (!World)
		return nullptr;

	UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>();
	if (!Registry)
		return nullptr;

	const TArray<TWeakObjectPtr<AGS_Seeker>>& Seekers = Registry->GetSeekers();

	AActor* BestAlly = nullptr;
	float MinDist = MAX_FLT;

	for (const TWeakObjectPtr<AGS_Seeker>& SeekerPtr : Seekers)
	{
		AGS_Seeker* Seeker = SeekerPtr.Get();
		if (!IsValid(Seeker) || Seeker == MyPawn || Seeker->IsDead() || Seeker->IsInDyingState())
		{
			continue;
		}

		float Dist = FVector::Dist(MyPawn->GetActorLocation(), Seeker->GetActorLocation());
		if (Dist < MinDist)
		{
			MinDist = Dist;
			BestAlly = Seeker;
		}
	}

	return BestAlly;
}

bool AGS_SeekerAIController::HasVisitedLocation(const FVector& Location) const
{
	for (const FVector& VisitedLoc : VisitedLocations)
	{
		if (FVector::Dist(Location, VisitedLoc) < VisitedLocationRadius)
		{
			return true;
		}
	}
	return false;
}

void AGS_SeekerAIController::MarkLocationVisited(const FVector& Location)
{
	VisitedLocations.Add(Location);

	if (VisitedLocations.Num() > 50)
	{
		VisitedLocations.RemoveAt(0, 10);
	}

	// 월드 참조 캐싱
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Implement exploration markers - same mechanism as player decal system
	float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastMarkerPlaceTime >= MarkerCooldown)
	{
		if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(GetPawn()))
		{
			if (Seeker->MarkerPlacementComponent)
			{
				// 마커 타입을 탐험(Exploration, X)으로 설정
				Seeker->MarkerPlacementComponent->SetSelectedMarkerType(EMarkerType::X);

				// 발밑이 아닌 시선 정면(바닥)에 마커 배치
				FVector ForwardDir = Seeker->GetActorForwardVector();
				ForwardDir.Z = 0.0f;
				ForwardDir.Normalize();

				// AI 전방 200 유닛 위치에 배치
				FVector MarkerLocation = Seeker->GetActorLocation() + ForwardDir * GS_AI::MARKER_PLACEMENT_FORWARD_DIST;

				// 바닥을 찾기 위한 레이트레이스
				FHitResult FloorHit;
				FVector TraceStart = MarkerLocation + FVector(0.0f, 0.0f, 100.0f);
				FVector TraceEnd = MarkerLocation - FVector(0.0f, 0.0f, 500.0f);

				FCollisionQueryParams TraceParams;
				TraceParams.AddIgnoredActor(Seeker);

				if (World->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
				{
					MarkerLocation = FloorHit.Location;
				}
				// 트레이스 실패 시 기본 발높이로 설정
				MarkerLocation.Z = Seeker->GetActorLocation().Z - 90.0f;

				FRotator DropRotation = FRotator(-90.0f, Seeker->GetActorRotation().Yaw, 0.0f); // 바닥을 향하도록 회전값 설정

				Seeker->MarkerPlacementComponent->Server_SpawnMarker(MarkerLocation, DropRotation, EMarkerType::X);

				LastMarkerPlaceTime = CurrentTime;
				UE_LOG(LogTemp, Log, TEXT("[SeekerAI] %s placed exploration marker at %s"), *Seeker->GetName(), *MarkerLocation.ToString());
			}
		}
	}
}

AGS_AISeeker* AGS_SeekerAIController::GetControlledSeeker() const
{
	return ControlledSeeker.Get();
}

void AGS_SeekerAIController::RunTacticalQuery()
{
	if (TacticalQueryRequestID != -1)
		return; // 이미 쿼리 실행 중

	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	AGS_AISeeker* Seeker = GetControlledSeeker();
	if (!World || !ControlledPawn || !Seeker)
		return;

	UEnvQueryManager* QueryManager = UEnvQueryManager::GetCurrent(World);
	if (!QueryManager)
		return;

	UEnvQuery* SelectedQuery = nullptr;
	ESeekerAIType SeekerType = Seeker->GetSeekerType();

	if (SeekerType == ESeekerAIType::Merci)
	{
		SelectedQuery = UGS_AssetLoader::SyncLoadAsset(CoverQueryTemplate);
	}
	else
	{
		SelectedQuery = UGS_AssetLoader::SyncLoadAsset(FlankQueryTemplate);
	}

	if (!SelectedQuery)
		return;

	FEnvQueryRequest QueryRequest(SelectedQuery, ControlledPawn);
	TacticalQueryRequestID = QueryManager->RunQuery(
	    QueryRequest,
	    EEnvQueryRunMode::SingleResult,
	    FQueryFinishedSignature::CreateUObject(this, &AGS_SeekerAIController::OnTacticalQueryFinished));
}

void AGS_SeekerAIController::OnTacticalQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	TacticalQueryRequestID = -1;

	if (!Result.IsValid() || !Result->IsSuccessful() || !Blackboard)
	{
		return;
	}

	FVector BestLocation = Result->GetItemAsLocation(0);
	AGS_AISeeker* Seeker = GetControlledSeeker();
	if (!Seeker)
		return;

	if (Seeker->GetSeekerType() == ESeekerAIType::Merci)
	{
		Blackboard->SetValueAsVector(CoverLocationKey, BestLocation);
		Blackboard->SetValueAsVector(TargetLocationKey, BestLocation); // Tactical 이동 목표로 설정
	}
	else
	{
		Blackboard->SetValueAsVector(FlankingLocationKey, BestLocation);
		Blackboard->SetValueAsVector(TargetLocationKey, BestLocation); // Tactical 이동 목표로 설정
	}
}

bool AGS_SeekerAIController::IsTargetNearTrap(AActor* Target, float Radius) const
{
	if (!Target)
		return false;

	UWorld* World = GetWorld();
	if (!World)
		return false;

	UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>();
	if (!Registry)
		return false;

	FVector TargetLoc = Target->GetActorLocation();
	const TArray<TWeakObjectPtr<AGS_TrapBase>>& AllTraps = Registry->GetTraps();

	for (const auto& TrapPtr : AllTraps)
	{
		AGS_TrapBase* Trap = TrapPtr.Get();
		if (IsValid(Trap) && Trap->bIsActivated)
		{
			if (FVector::Dist(TargetLoc, Trap->GetActorLocation()) <= Radius)
			{
				return true;
			}
		}
	}

	return false;
}
