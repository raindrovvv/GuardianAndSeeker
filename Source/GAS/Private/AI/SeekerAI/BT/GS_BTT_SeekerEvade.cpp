// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/BT/GS_BTT_SeekerEvade.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "AI/SeekerAI/GS_AISeeker.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Props/Trap/GS_TrapBase.h"
#include "NavigationSystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AI/GS_AIConstants.h"

UGS_BTT_SeekerEvade::UGS_BTT_SeekerEvade()
{
	NodeName = "Seeker Evade";
	bNotifyTick = true;
	TrapLocationKey.SelectedKeyName = AGS_SeekerAIController::NearbyTrapKey;
}

EBTNodeResult::Type UGS_BTT_SeekerEvade::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	float CurrentTime = GetWorld()->GetTimeSeconds();

	// 전역 회피 쿨다운: 회피와 목표 이동 사이의 잦은 전환 방지
	// "트랩" 지역에서 최근 회피 후 목표에 집중할 수 있게 함
	if (CurrentTime - LastEvadeTaskFinishTime < GlobalEvadeCooldown)
	{
		return EBTNodeResult::Failed;
	}

	FGS_BTTEvadeMemory* MyMemory = reinterpret_cast<FGS_BTTEvadeMemory*>(NodeMemory);
	MyMemory->StartTime = CurrentTime;

	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = AIController->GetPawn();
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn);
	if (!Seeker || Seeker->IsDead())
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	// 이미 액션 수행 중이면 회피 방지 (구르기 등)
	if (UAnimInstance* AnimInstance = Seeker->GetMesh()->GetAnimInstance())
	{
		if (AnimInstance->Montage_IsPlaying(nullptr))
		{
			return EBTNodeResult::InProgress;
		}
	}

	// 블랙보드에서 근처 트랩 가져오기
	UObject* TrapObject = Blackboard->GetValueAsObject(TrapLocationKey.SelectedKeyName);
	AGS_TrapBase* NearbyTrap = Cast<AGS_TrapBase>(TrapObject);

	if (!NearbyTrap)
	{
		// 트랩 위협 없음 - 회피 불필요
		AIController->ClearNearbyTrap();
		return EBTNodeResult::Succeeded;
	}

	// 트랩으로부터 안전한 방향 계산
	FVector SafeDirection = CalculateSafeEvadeDirection(NearbyTrap->GetActorLocation(), Seeker->GetActorLocation());

	if (SafeDirection.IsNearlyZero())
	{
		// 안전한 지점을 찾지 못함 - 회피 건너뛰기
		// 트랩 밀집 지역에서 떨림 현상 방지
		AIController->ClearNearbyTrap();
		return EBTNodeResult::Succeeded;
	}

	// 구르기로 회피할지 확인
	CurrentTime = GetWorld()->GetTimeSeconds();
	bool bCanRoll = (CurrentTime - LastRollTime >= RollCooldown);

	AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(Pawn->GetOwner());

	if (bPreferRollOverWalk && bCanRoll)
	{
		// 구르기 스킬 사용 시도
		if (AISeeker && AISeeker->CanUseSkill(static_cast<int32>(ESkillSlot::Rolling)))
		{
			AISeeker->PerformRoll(SafeDirection);
			LastRollTime = CurrentTime;
			return EBTNodeResult::InProgress;
		}
	}

	// 회피 스킬 사용 가능 시 시도
	if (bUseEvadeSkillIfAvailable && AISeeker)
	{
		// 이동 스킬(E 스킬)로 재배치 시도
		if (AISeeker->CanUseSkill(static_cast<int32>(ESkillSlot::Moving)))
		{
			AISeeker->PerformSkill(static_cast<int32>(ESkillSlot::Moving));
			return EBTNodeResult::InProgress;
		}
	}

	// 대체 방안: 트랩에서 걸어서 멀어지기
	FVector EvadeLocation = Seeker->GetActorLocation() + SafeDirection * (EvadeDistance * 0.5f);

	// 네비메시에 투영
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSystem)
	{
		FNavLocation NavLocation;
		if (NavSystem->ProjectPointToNavigation(EvadeLocation, NavLocation))
		{
			EvadeLocation = NavLocation.Location;
		}
	}

	// 이미 안전 거리에 있으면 완료
	if (FVector::Dist(Seeker->GetActorLocation(), EvadeLocation) < GS_AI::EVADE_ARRIVAL_THRESHOLD)
	{
		return EBTNodeResult::Succeeded;
	}

	// 작은 조정은 경로탐색 없이 이동하여 전역 목표 경로 보존
	// 큰 허용 반경으로 빠르고 부드럽게 회피 완료
	FAIRequestID MoveResult = AIController->MoveToLocation(EvadeLocation, GS_AI::EVADE_ACCEPTANCE_RADIUS, true, true, false, false);

	if (MoveResult.IsValid())
	{
		return EBTNodeResult::InProgress;
	}

	return EBTNodeResult::Failed;
}

void UGS_BTT_SeekerEvade::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FGS_BTTEvadeMemory* MyMemory = reinterpret_cast<FGS_BTTEvadeMemory*>(NodeMemory);
	float CurrentTime = GetWorld()->GetTimeSeconds();

	// 타임아웃 폴백: 회피 상태 무한 유지 방지
	if (CurrentTime - MyMemory->StartTime > GS_AI::EVADE_TASK_TIMEOUT)
	{
		LastEvadeTaskFinishTime = CurrentTime;
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 구르기 스킬 완료 여부 확인
	if (APawn* Pawn = AIController->GetPawn())
	{
		if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn))
		{
			if (UGS_SkillComp* SkillComp = Seeker->GetSkillComp())
			{
				// 구르기 상태가 아니면 완료된 것
				if (!SkillComp->IsSkillActive(ESkillSlot::Rolling))
				{
					// 몽타주 재생 중이 아니면 완료 처리
					if (UAnimInstance* AnimInstance = Seeker->GetMesh()->GetAnimInstance())
					{
						if (!AnimInstance->Montage_IsPlaying(nullptr))
						{
							LastEvadeTaskFinishTime = CurrentTime;
							FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
							return;
						}
					}
				}
			}
		}
	}

	// 여전히 트랩 근처인지 확인
	if (!AIController->ShouldEvade())
	{
		AIController->StopMovement();
		LastEvadeTaskFinishTime = CurrentTime;
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// 이동 상태 확인
	UPathFollowingComponent* PFC = AIController->GetPathFollowingComponent();
	if (PFC)
	{
		EPathFollowingStatus::Type Status = PFC->GetStatus();
		if (Status == EPathFollowingStatus::Type::Idle ||
		    Status == EPathFollowingStatus::Type::Paused)
		{
			// 이동 완료
			LastEvadeTaskFinishTime = CurrentTime;
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
	}
}

EBTNodeResult::Type UGS_BTT_SeekerEvade::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner()))
	{
		AIController->StopMovement();
	}
	return EBTNodeResult::Aborted;
}

FVector UGS_BTT_SeekerEvade::CalculateSafeEvadeDirection(const FVector& ThreatLocation, const FVector& CurrentLocation) const
{
	// 기본 방향: 위협으로부터 반대 방향
	FVector AwayDirection = CurrentLocation - ThreatLocation;
	AwayDirection.Z = 0.0f;

	if (AwayDirection.IsNearlyZero())
	{
		// 같은 위치면 랜덤 방향 선택
		AwayDirection = FVector(FMath::FRand() - 0.5f, FMath::FRand() - 0.5f, 0.0f);
	}

	AwayDirection.Normalize();

	// 기본 방향이 막혀있는지 확인
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSystem)
	{
		// 해당 위치가 모든 트랩으로부터 안전한지 확인하는 람다
		auto IsLocationSafeFromAllTraps = [this](const FVector& Loc) -> bool
		{
			TArray<AActor*> NearbyTraps;
			TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
			ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

			UKismetSystemLibrary::SphereOverlapActors(
			    GetWorld(), Loc, GS_AI::MONSTER_OVERLAP_RADIUS,
			    ObjectTypes, AGS_TrapBase::StaticClass(), TArray<AActor*>(), NearbyTraps);

			return NearbyTraps.Num() == 0;
		};

		float currentEvadeDist = EvadeDistance;

		FVector TestLocation = CurrentLocation + AwayDirection * currentEvadeDist;
		FNavLocation NavLocation;

		// 네비메시 투영 시도
		if (NavSystem->ProjectPointToNavigation(TestLocation, NavLocation, FVector(200.f, 200.f, 200.f)))
		{
			if (IsLocationSafeFromAllTraps(NavLocation.Location))
			{
				return AwayDirection;
			}
		}

		// 대체 방향 시도 (45, 90, 135도 회전)
		const float RotationAngles[] = {45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f};

		FVector BestDirection = FVector::ZeroVector;
		int32 MinTrapCount = INT32_MAX;

		for (float Angle : RotationAngles)
		{
			FVector RotatedDirection = AwayDirection.RotateAngleAxis(Angle, FVector::UpVector);
			TestLocation = CurrentLocation + RotatedDirection * currentEvadeDist;

			if (NavSystem->ProjectPointToNavigation(TestLocation, NavLocation, FVector(200.f, 200.f, 200.f)))
			{
				// 해당 위치 근처 트랩 개수 확인
				TArray<AActor*> NearbyTraps;
				TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
				ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

				UKismetSystemLibrary::SphereOverlapActors(
				    GetWorld(), NavLocation.Location, 150.0f,
				    ObjectTypes, AGS_TrapBase::StaticClass(), TArray<AActor*>(), NearbyTraps);

				// 트랩이 적은 방향 우선
				if (NearbyTraps.Num() < MinTrapCount)
				{
					MinTrapCount = NearbyTraps.Num();
					BestDirection = RotatedDirection;

					// 완전히 안전한 방향을 찾으면 즉시 사용
					if (MinTrapCount == 0)
					{
						return BestDirection;
					}
				}
			}
		}

		// "덜 위험한" 방향을 찾았으면 사용
		if (!BestDirection.IsNearlyZero())
		{
			return BestDirection;
		}
	}

	// 폴백: 완벽한 지점이 없어도 즉각적인 위협에서 벗어나기
	// 트랩에 둘러싸여도 제자리에 멈추지 않도록 함
	return AwayDirection;
}

FString UGS_BTT_SeekerEvade::GetStaticDescription() const
{
	return FString::Printf(TEXT("트랩 회피\n거리: %.0f\n구르기 쿨다운: %.1fs\n구르기 우선: %s"),
	                       EvadeDistance, RollCooldown, bPreferRollOverWalk ? TEXT("예") : TEXT("아니오"));
}
