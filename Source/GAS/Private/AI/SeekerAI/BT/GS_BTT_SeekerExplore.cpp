// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/BT/GS_BTT_SeekerExplore.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "AI/SeekerAI/GS_AIGoalTrigger.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"

UGS_BTT_SeekerExplore::UGS_BTT_SeekerExplore()
{
	NodeName = "Seeker Explore";
	bNotifyTick = true;
	bCreateNodeInstance = true; // Essential for per-instance BT task state
}

EBTNodeResult::Type UGS_BTT_SeekerExplore::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	// First, check if there's a goal to move towards
	UObject* GoalObject = Blackboard->GetValueAsObject(AGS_SeekerAIController::GoalActorKey);
	if (AGS_AIGoalTrigger* GoalTrigger = Cast<AGS_AIGoalTrigger>(GoalObject))
	{
		if (!GoalTrigger->HasBeenReached())
		{
			// Move towards goal
			FVector GoalLocation = GoalTrigger->GetGoalLocation();

			// Check if already at goal
			if (FVector::Dist(Pawn->GetActorLocation(), GoalLocation) < AcceptableRadius)
			{
				AIController->OnGoalReached();
				return EBTNodeResult::Succeeded;
			}

			FAIRequestID MoveResult = AIController->MoveToLocation(
			    GoalLocation,
			    AcceptableRadius,
			    true, // bStopOnOverlap
			    true, // bUsePathfinding
			    true, // bProjectDestinationToNavigation
			    false, // bCanStrafe
			    nullptr,
			    true // bAllowPartialPath
			);

			if (MoveResult.IsValid())
			{
				return EBTNodeResult::InProgress;
			}
		}
	}

	// No goal or goal reached - explore randomly
	FVector ExplorationTarget = FVector::ZeroVector;

	// Try to get exploration target from blackboard
	ExplorationTarget = Blackboard->GetValueAsVector(AGS_SeekerAIController::ExplorationTargetKey);

	// ⭐ If no valid target, find a new one
	bool bNeedNewTarget = ExplorationTarget.IsNearlyZero() ||
	                      FVector::Dist(Pawn->GetActorLocation(), ExplorationTarget) < AcceptableRadius;

	if (bNeedNewTarget)
	{
		if (!AIController->FindNewExplorationTarget())
		{
			// ⭐ Failed to find exploration target - wait and retry later
			UE_LOG(LogTemp, Warning, TEXT("[Explore Task] Failed to find new target, will retry"));
			return EBTNodeResult::Failed;
		}
		ExplorationTarget = Blackboard->GetValueAsVector(AGS_SeekerAIController::ExplorationTargetKey);

		// ⭐ 타겟을 찾았지만 여전히 invalid하면 실패
		if (ExplorationTarget.IsNearlyZero())
		{
			UE_LOG(LogTemp, Error, TEXT("[Explore Task] Invalid target after FindNewExplorationTarget!"));
			return EBTNodeResult::Failed;
		}
	}

	// Move to exploration target
	FAIRequestID MoveResult = AIController->MoveToLocation(
	    ExplorationTarget,
	    AcceptableRadius,
	    true, // bStopOnOverlap
	    true, // bUsePathfinding
	    true, // bProjectDestinationToNavigation
	    false, // bCanStrafe
	    nullptr,
	    true // bAllowPartialPath
	);

	if (MoveResult.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[Explore Task] Moving to: %s (RequestID: %u)"), *ExplorationTarget.ToString(), (uint32)MoveResult.GetID());
		return EBTNodeResult::InProgress;
	}

	UE_LOG(LogTemp, Error, TEXT("[Explore Task] MoveToLocation failed! Target: %s"), *ExplorationTarget.ToString());
	return EBTNodeResult::Failed;
}

void UGS_BTT_SeekerExplore::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 이동 완료 확인
	UPathFollowingComponent* PFC = AIController->GetPathFollowingComponent();
	if (PFC)
	{
		EPathFollowingStatus::Type Status = PFC->GetStatus();
		if (Status == EPathFollowingStatus::Type::Idle)
		{
			// 이동 완료 또는 시작되지 않음 - 타겟 도달 여부 확인
			UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
			if (Blackboard)
			{
				FVector ExplorationTarget = Blackboard->GetValueAsVector(AGS_SeekerAIController::ExplorationTargetKey);
				float DistanceToTarget = FVector::Dist(Pawn->GetActorLocation(), ExplorationTarget);

				// 실제로 타겟에 도달했는지 확인
				if (DistanceToTarget < AcceptableRadius * 2.0f)
				{
					// 정상 도착 - 실패 카운터 리셋 및 새 타겟 찾기
					ConsecutiveFailures = 0;
					if (AIController->FindNewExplorationTarget())
					{
						FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
					}
					else
					{
						FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
					}
				}
				else
				{
					// 타겟 미도달 상태에서 Idle = 이동 실패
					ConsecutiveFailures++;

					UE_LOG(LogTemp, Warning, TEXT("[Explore Task] 이동 실패 (거리: %.1f, 연속 실패: %d)"),
					       DistanceToTarget, ConsecutiveFailures);

					// 3회 이상 연속 실패 시 현재 타겟 무효화
					if (ConsecutiveFailures >= 3)
					{
						// 현재 타겟 클리어하여 새로운 타겟 강제 탐색
						Blackboard->SetValueAsVector(AGS_SeekerAIController::ExplorationTargetKey, FVector::ZeroVector);
						ConsecutiveFailures = 0;
						UE_LOG(LogTemp, Warning, TEXT("[Explore Task] 3회 연속 실패 - 탐색 타겟 리셋"));
					}

					FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
				}
			}
			else
			{
				FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			}
			return;
		}
		else if (Status == EPathFollowingStatus::Type::Paused)
		{
			// Paused 상태 - 재개 시도
			AIController->ResumeMovement();
		}
	}

	// 아직 이동 중 - 인터럽트(적, 트랩) 또는 고착 상태 확인
	float CurrentStationaryTime = AIController->GetStationaryTime();
	if (CurrentStationaryTime > 4.0f) // 4초 이상 제자리면 고착으로 판단
	{
		UE_LOG(LogTemp, Warning, TEXT("[Explore Task] %s 고착 감지 (%.1fs), 새 타켓 탐색 유도"),
		       *GetNameSafe(Pawn), CurrentStationaryTime);

		AIController->StopMovement();

		// 타겟 무효화하여 다음 실행 때 새 타겟 찾게 함
		UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
		if (Blackboard)
		{
			Blackboard->SetValueAsVector(AGS_SeekerAIController::ExplorationTargetKey, FVector::ZeroVector);
		}

		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (AIController->IsInCombat() || AIController->ShouldEvade())
	{
		AIController->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UGS_BTT_SeekerExplore::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner()))
	{
		AIController->StopMovement();
	}
	return EBTNodeResult::Aborted;
}

FString UGS_BTT_SeekerExplore::GetStaticDescription() const
{
	return FString::Printf(TEXT("Explore dungeon\nRadius: %.0f\nAcceptable: %.0f"),
	                       ExplorationRadius, AcceptableRadius);
}
