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

UGS_BTT_SeekerEvade::UGS_BTT_SeekerEvade()
{
	NodeName = "Seeker Evade";
	bNotifyTick = true;

	// Default blackboard key setup
	TrapLocationKey.SelectedKeyName = AGS_SeekerAIController::NearbyTrapKey;
}

EBTNodeResult::Type UGS_BTT_SeekerEvade::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	float CurrentTime = GetWorld()->GetTimeSeconds();

	// Global Evasion Cooldown: prevent AI from thrashering between evasion and goal movement
	// This helps AI bypass "trap forests" by focusing on the goal if a dodge was recently done.
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

	// Prevent evasion if already performing an action (e.g. rolling)
	if (UAnimInstance* AnimInstance = Seeker->GetMesh()->GetAnimInstance())
	{
		if (AnimInstance->Montage_IsPlaying(nullptr))
		{
			return EBTNodeResult::InProgress;
		}
	}

	// Get nearby trap from blackboard
	UObject* TrapObject = Blackboard->GetValueAsObject(TrapLocationKey.SelectedKeyName);
	AGS_TrapBase* NearbyTrap = Cast<AGS_TrapBase>(TrapObject);

	if (!NearbyTrap)
	{
		// No trap threat - no need to evade
		AIController->ClearNearbyTrap();
		return EBTNodeResult::Succeeded;
	}

	// Probabilistic evasion (70% chance to notice and evade)
	if (FMath::FRand() > 0.7f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AI] Seeker ignored trap threat (luck)"));
		AIController->ClearNearbyTrap();
		return EBTNodeResult::Succeeded;
	}

	// Calculate safe direction away from trap
	FVector SafeDirection = CalculateSafeEvadeDirection(NearbyTrap->GetActorLocation(), Seeker->GetActorLocation());

	if (SafeDirection.IsNearlyZero())
	{
		// If no truly safe spot found (away from other traps), just ignore this one
		// This prevents "shaking" between multiple traps in a cluster
		UE_LOG(LogTemp, Warning, TEXT("[AI] Seeker could not find a safe evade spot, skipping evasion."));
		AIController->ClearNearbyTrap();
		return EBTNodeResult::Succeeded;
	}

	// Check if we should use roll for evasion
	CurrentTime = GetWorld()->GetTimeSeconds();
	bool bCanRoll = (CurrentTime - LastRollTime >= RollCooldown);

	AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(Pawn->GetOwner());

	if (bPreferRollOverWalk && bCanRoll)
	{
		// Try to use roll skill
		if (AISeeker && AISeeker->CanUseSkill(static_cast<int32>(ESkillSlot::Rolling)))
		{
			AISeeker->PerformRoll(SafeDirection);
			LastRollTime = CurrentTime;
			UE_LOG(LogTemp, Warning, TEXT("[AI] Seeker rolling to evade trap"));
			return EBTNodeResult::InProgress;
		}
	}

	// Try evasion skill if available
	if (bUseEvadeSkillIfAvailable && AISeeker)
	{
		// Try MovingSkill (E skill) for repositioning
		if (AISeeker->CanUseSkill(static_cast<int32>(ESkillSlot::Moving)))
		{
			AISeeker->PerformSkill(static_cast<int32>(ESkillSlot::Moving));
			UE_LOG(LogTemp, Warning, TEXT("[AI] Seeker using skill to evade trap"));
			return EBTNodeResult::InProgress;
		}
	}

	// Fallback: walk away from trap
	FVector EvadeLocation = Seeker->GetActorLocation() + SafeDirection * (EvadeDistance * 0.5f); // Reduced default distance for smoothness

	// Project to navigation mesh
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSystem)
	{
		FNavLocation NavLocation;
		if (NavSystem->ProjectPointToNavigation(EvadeLocation, NavLocation))
		{
			EvadeLocation = NavLocation.Location;
		}
	}

	// Check if already at a safe distance (reduced threshold for non-blocking feel)
	if (FVector::Dist(Seeker->GetActorLocation(), EvadeLocation) < 30.0f)
	{
		return EBTNodeResult::Succeeded;
	}

	// Use bUsePathfinding=false for tiny adjustments to avoid clearing the global goal path
	// Use a large acceptance radius so it finishes the "dodge" quickly
	FAIRequestID MoveResult = AIController->MoveToLocation(EvadeLocation, 40.0f, true, true, false, false);

	if (MoveResult.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AI] Seeker making small dodge adjustment"));
		return EBTNodeResult::InProgress;
	}

	return EBTNodeResult::Failed;
}

void UGS_BTT_SeekerEvade::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FGS_BTTEvadeMemory* MyMemory = reinterpret_cast<FGS_BTTEvadeMemory*>(NodeMemory);
	float CurrentTime = GetWorld()->GetTimeSeconds();

	// Timeout fallback: don't stay in evade state forever (e.g. 2.0s max)
	if (CurrentTime - MyMemory->StartTime > 2.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AI] Seeker evade task timed out, finishing..."));
		LastEvadeTaskFinishTime = CurrentTime; // Set cooldown
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Check if we're still near a trap
	if (!AIController->ShouldEvade())
	{
		AIController->StopMovement();
		LastEvadeTaskFinishTime = CurrentTime; // Set cooldown
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Check movement status
	UPathFollowingComponent* PFC = AIController->GetPathFollowingComponent();
	if (PFC)
	{
		EPathFollowingStatus::Type Status = PFC->GetStatus();
		if (Status == EPathFollowingStatus::Type::Idle ||
		    Status == EPathFollowingStatus::Type::Paused)
		{
			// Movement complete
			LastEvadeTaskFinishTime = CurrentTime; // Set cooldown
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
		// If path is blocked or aborted on the move-to level
		else if (Status == EPathFollowingStatus::Type::Waiting)
		{
			// Could be stuck, let it finish or wait for next tick
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
	// Base direction: away from threat
	FVector AwayDirection = CurrentLocation - ThreatLocation;
	AwayDirection.Z = 0.0f;

	if (AwayDirection.IsNearlyZero())
	{
		// If at same location, pick random direction
		AwayDirection = FVector(FMath::FRand() - 0.5f, FMath::FRand() - 0.5f, 0.0f);
	}

	AwayDirection.Normalize();

	// Check if base direction is blocked
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSystem)
	{
		auto IsLocationSafeFromAllTraps = [this](const FVector& Loc) -> bool
		{
			TArray<AActor*> NearbyTraps;
			TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
			ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

			UKismetSystemLibrary::SphereOverlapActors(
			    GetWorld(), Loc, 150.0f, // Safety radius from other traps
			    ObjectTypes, AGS_TrapBase::StaticClass(), TArray<AActor*>(), NearbyTraps);

			return NearbyTraps.Num() == 0;
		};

		// Determine dynamic evade distance
		float currentEvadeDist = EvadeDistance;
		AGS_Seeker* Seeker = Cast<AGS_Seeker>(GetWorld()->GetFirstPlayerController()->GetPawn()); // Fallback if needed, but better use Seeker ref
		// Since we don't have Seeker easily here, we'll use a conservative distance increase if we detect a ranged AI
		// Better: the distance is passed via property, but we can nudge it.

		FVector TestLocation = CurrentLocation + AwayDirection * currentEvadeDist;
		FNavLocation NavLocation;

		// Use a generous extent to find valid navmesh
		if (NavSystem->ProjectPointToNavigation(TestLocation, NavLocation, FVector(200.f, 200.f, 200.f)))
		{
			if (IsLocationSafeFromAllTraps(NavLocation.Location))
			{
				return AwayDirection;
			}
		}

		// Try alternate directions (rotate 45, 90, 135 degrees)
		const float RotationAngles[] = {45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f};

		for (float Angle : RotationAngles)
		{
			FVector RotatedDirection = AwayDirection.RotateAngleAxis(Angle, FVector::UpVector);
			TestLocation = CurrentLocation + RotatedDirection * currentEvadeDist;

			if (NavSystem->ProjectPointToNavigation(TestLocation, NavLocation, FVector(200.f, 200.f, 200.f)))
			{
				if (IsLocationSafeFromAllTraps(NavLocation.Location))
				{
					return RotatedDirection;
				}
			}
		}
	}

	// If no truly safe spot found, return ZeroVector to bypass evasion
	// (procedure with goal movement is better than jittering between traps)
	return FVector::ZeroVector;
}

FString UGS_BTT_SeekerEvade::GetStaticDescription() const
{
	return FString::Printf(TEXT("Evade Traps\nDistance: %.0f\nRoll Cooldown: %.1fs\nPrefer Roll: %s"),
	                       EvadeDistance, RollCooldown, bPreferRollOverWalk ? TEXT("Yes") : TEXT("No"));
}
