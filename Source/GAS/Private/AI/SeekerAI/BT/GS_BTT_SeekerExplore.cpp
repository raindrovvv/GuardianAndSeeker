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
	
	// If no valid target, find a new one
	if (ExplorationTarget.IsZero() || FVector::Dist(Pawn->GetActorLocation(), ExplorationTarget) < AcceptableRadius)
	{
		if (!AIController->FindNewExplorationTarget())
		{
			// Failed to find exploration target
			return EBTNodeResult::Failed;
		}
		ExplorationTarget = Blackboard->GetValueAsVector(AGS_SeekerAIController::ExplorationTargetKey);
	}

	// Check if already at exploration target
	if (FVector::Dist(Pawn->GetActorLocation(), ExplorationTarget) < AcceptableRadius)
	{
		// Find new target
		AIController->FindNewExplorationTarget();
		return EBTNodeResult::Succeeded;
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
		return EBTNodeResult::InProgress;
	}

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

	// Check if movement is complete
	UPathFollowingComponent* PFC = AIController->GetPathFollowingComponent();
	if (PFC)
	{
		EPathFollowingStatus::Type Status = PFC->GetStatus();
		if (Status == EPathFollowingStatus::Type::Idle ||
			Status == EPathFollowingStatus::Type::Paused)
		{
			// Movement completed - find new target
			AIController->FindNewExplorationTarget();
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
			return;
		}
	}

	// Still moving - check for interrupts (enemies, traps)
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
