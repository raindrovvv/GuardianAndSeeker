// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/BT/GS_BTT_SeekerHeal.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "AI/SeekerAI/GS_AISeeker.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Skill/GS_SkillComp.h"
#include "NavigationSystem.h"

UGS_BTT_SeekerHeal::UGS_BTT_SeekerHeal()
{
	NodeName = "Seeker Heal";
	bNotifyTick = true;
}

EBTNodeResult::Type UGS_BTT_SeekerHeal::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
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

	// Prevent overlapping heals or actions
	if (UAnimInstance* AnimInstance = Seeker->GetMesh()->GetAnimInstance())
	{
		if (AnimInstance->Montage_IsPlaying(nullptr))
		{
			return EBTNodeResult::InProgress;
		}
	}

	// Check cooldown
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastHealTime < HealCooldown)
	{
		// Still on cooldown
		return EBTNodeResult::Failed;
	}

	// Get AI Seeker wrapper
	AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(Pawn->GetOwner());

	// Check if we can actually heal
	if (AISeeker && !AISeeker->CanHeal())
	{
		// No heal available
		return EBTNodeResult::Failed;
	}

	// If configured, find a safe position first
	if (bFindSafePositionFirst && AIController->IsInCombat())
	{
		// Try to move away from combat before healing
		FVector SafePosition = FindSafePosition(AIController, Seeker->GetActorLocation());

		if (!SafePosition.IsZero())
		{
			FAIRequestID MoveResult = AIController->MoveToLocation(SafePosition, 50.0f);
			if (MoveResult.IsValid())
			{
				return EBTNodeResult::InProgress;
			}
		}
	}

	// Perform heal
	if (AISeeker)
	{
		AISeeker->PerformHeal();
		LastHealTime = CurrentTime;
		return EBTNodeResult::InProgress;
	}
	else
	{
		// Direct skill activation fallback
		if (UGS_SkillComp* SkillComp = Seeker->FindComponentByClass<UGS_SkillComp>())
		{
			SkillComp->Server_TryActivateSkill(ESkillSlot::HealPotion);
			LastHealTime = CurrentTime;
			return EBTNodeResult::InProgress;
		}
	}

	return EBTNodeResult::Failed;
}

void UGS_BTT_SeekerHeal::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn);
	if (!Seeker || Seeker->IsDead())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Check if still in healing animation
	if (UAnimInstance* AnimInstance = Seeker->GetMesh()->GetAnimInstance())
	{
		if (!AnimInstance->Montage_IsPlaying(nullptr))
		{
			// Healing complete
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
			return;
		}
	}

	// Check for immediate threats
	if (AIController->ShouldEvade())
	{
		// Danger! Interrupt healing
		AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(Pawn->GetOwner());
		if (AISeeker)
		{
			AISeeker->StopAllActions();
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Check movement status (if we were moving to safe position)
	UPathFollowingComponent* PFC = AIController->GetPathFollowingComponent();
	if (PFC)
	{
		EPathFollowingStatus::Type Status = PFC->GetStatus();
		if (Status == EPathFollowingStatus::Type::Idle ||
			Status == EPathFollowingStatus::Type::Paused)
		{
			// Reached safe position or not moving - perform heal
			AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(Pawn->GetOwner());
			if (AISeeker && AISeeker->CanHeal())
			{
				AISeeker->PerformHeal();
				LastHealTime = GetWorld()->GetTimeSeconds();
			}

			// Short delay then finish
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
	}
}

FString UGS_BTT_SeekerHeal::GetStaticDescription() const
{
	return FString::Printf(TEXT("Heal\nCooldown: %.1fs\nFind Safe Position: %s"),
	                       HealCooldown, bFindSafePositionFirst ? TEXT("Yes") : TEXT("No"));
}

FVector UGS_BTT_SeekerHeal::FindSafePosition(AGS_SeekerAIController* AIController, const FVector& CurrentLocation) const
{
	if (!AIController)
	{
		return FVector::ZeroVector;
	}

	UWorld* World = AIController->GetWorld();
	if (!World)
	{
		return FVector::ZeroVector;
	}

	UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent();
	if (!Blackboard)
	{
		return FVector::ZeroVector;
	}

	// Get enemy location to move away from
	UObject* EnemyObject = Blackboard->GetValueAsObject(AGS_SeekerAIController::TargetEnemyKey);
	AActor* EnemyActor = Cast<AActor>(EnemyObject);

	FVector AwayDirection;
	if (EnemyActor)
	{
		AwayDirection = (CurrentLocation - EnemyActor->GetActorLocation()).GetSafeNormal();
	}
	else
	{
		// Random direction
		AwayDirection = FVector(FMath::FRand() - 0.5f, FMath::FRand() - 0.5f, 0.0f).GetSafeNormal();
	}

	// Find a point in navigation mesh
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (NavSystem)
	{
		FVector TestLocation = CurrentLocation + AwayDirection * SafePositionSearchRadius;
		FNavLocation NavLocation;

		if (NavSystem->ProjectPointToNavigation(TestLocation, NavLocation))
		{
			return NavLocation.Location;
		}
	}

	return FVector::ZeroVector;
}
