// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/BT/GS_BTS_SeekerPerception.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "AI/SeekerAI/GS_AIGoalTrigger.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Component/GS_StatComp.h"
#include "Props/Trap/GS_TrapBase.h"
#include "Props/Item/EmberChest/GS_EmberChest.h"
#include "Interface/GS_InteractableInterface.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"

UGS_BTS_SeekerPerception::UGS_BTS_SeekerPerception()
{
	NodeName = "Seeker Perception";
	Interval = 0.2f; // Update every 0.2 seconds
	RandomDeviation = 0.05f;

	// Default blackboard key setup
	TargetEnemyKey.SelectedKeyName = AGS_SeekerAIController::TargetEnemyKey;
	NearbyTrapKey.SelectedKeyName = AGS_SeekerAIController::NearbyTrapKey;
	ShouldHealKey.SelectedKeyName = AGS_SeekerAIController::ShouldHealKey;
	IsInCombatKey.SelectedKeyName = AGS_SeekerAIController::IsInCombatKey;
	GoalActorKey.SelectedKeyName = AGS_SeekerAIController::GoalActorKey;
	LastKnownLocationKey.SelectedKeyName = AGS_SeekerAIController::LastKnownEnemyLocationKey;
	DownedAllyKey.SelectedKeyName = AGS_SeekerAIController::DownedAllyKey;
	InteractiveItemKey.SelectedKeyName = AGS_SeekerAIController::InteractiveItemKey;
}

void UGS_BTS_SeekerPerception::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UpdateEnemyPerception(OwnerComp);
	UpdateTrapPerception(OwnerComp);
	UpdateHealthStatus(OwnerComp);
	UpdateGoalPerception(OwnerComp);
	UpdateAllyPerception(OwnerComp);
	UpdateInteractivePerception(OwnerComp);
}

void UGS_BTS_SeekerPerception::UpdateEnemyPerception(UBehaviorTreeComponent& OwnerComp)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController || !AIController->GetPerceptionComponent())
	{
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn);
	if (!Seeker)
	{
		return;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return;
	}

	// Check current target validity
	UObject* CurrentTarget = Blackboard->GetValueAsObject(TargetEnemyKey.SelectedKeyName);
	if (AGS_Monster* CurrentMonster = Cast<AGS_Monster>(CurrentTarget))
	{
		if (CurrentMonster->IsDead())
		{
			Blackboard->ClearValue(TargetEnemyKey.SelectedKeyName);
			Blackboard->SetValueAsBool(IsInCombatKey.SelectedKeyName, false);
			CurrentTarget = nullptr;
		}
	}

	// Use Perception Component instead of GetAllActorsOfClass
	TArray<AActor*> PerceivedActors;
	AIController->GetPerceptionComponent()->GetKnownPerceivedActors(nullptr, PerceivedActors);

	// Check for downed ally to prioritize nearby enemies
	AActor* DownedAlly = Cast<AActor>(Blackboard->GetValueAsObject(DownedAllyKey.SelectedKeyName));

	AActor* BestTarget = nullptr;
	float MinDistance = EnemyDetectionRadius;

	for (AActor* Actor : PerceivedActors)
	{
		if (AGS_Monster* Monster = Cast<AGS_Monster>(Actor))
		{
			if (!Monster->IsDead())
			{
				float DistanceFromSelf = FVector::Dist(Seeker->GetActorLocation(), Monster->GetActorLocation());
				float PriorityScore = DistanceFromSelf;

				// Help colleague logic: prioritize monsters near downed allies
				if (DownedAlly)
				{
					float DistanceToAlly = FVector::Dist(DownedAlly->GetActorLocation(), Monster->GetActorLocation());
					if (DistanceToAlly < 1200.0f) // Within 12m of ally
					{
						PriorityScore -= 1000.0f; // High priority boost
					}
				}
				// Help colleague logic: prioritize monsters near any ally with low health
				else
				{
					if (UWorld* World = GetWorld())
					{
						if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
						{
							const TArray<TWeakObjectPtr<AGS_Seeker>>& NearbySeekers = Registry->GetSeekers();
							for (const TWeakObjectPtr<AGS_Seeker>& AllyPtr : NearbySeekers)
							{
								AGS_Seeker* Ally = AllyPtr.Get();
								if (Ally && Ally != Seeker && !Ally->IsDead())
								{
									float DistToAlly = FVector::Dist(Ally->GetActorLocation(), Monster->GetActorLocation());
									if (DistToAlly < 800.0f)
									{
										if (UGS_StatComp* Stat = Ally->GetStatComp())
										{
											if (Stat->GetCurrentHealth() / Stat->GetMaxHealth() < 0.4f)
											{
												PriorityScore -= 500.0f; // Moderate priority boost
												break;
											}
										}
									}
								}
							}
						}
					}
				}

				if (PriorityScore < MinDistance)
				{
					MinDistance = PriorityScore;
					BestTarget = Monster;
				}
			}
		}
	}

	if (BestTarget)
	{
		Blackboard->SetValueAsObject(TargetEnemyKey.SelectedKeyName, BestTarget);
		const FVector TargetLocation = BestTarget->GetActorLocation();
		Blackboard->SetValueAsVector(LastKnownLocationKey.SelectedKeyName, TargetLocation);
		Blackboard->SetValueAsBool(IsInCombatKey.SelectedKeyName, true);
	}
	else if (CurrentTarget)
	{
		// If we had a target but it's no longer perceived or out of range
		AActor* TargetActor = Cast<AActor>(CurrentTarget);
		float Distance = FVector::Dist(Seeker->GetActorLocation(), TargetActor->GetActorLocation());

		// Update last known location while we still have a valid reference
		const FVector TargetLocationLS = TargetActor->GetActorLocation();
		Blackboard->SetValueAsVector(LastKnownLocationKey.SelectedKeyName, TargetLocationLS);

		if (Distance > EnemyDetectionRadius * 1.5f)
		{
			Blackboard->ClearValue(TargetEnemyKey.SelectedKeyName);
			Blackboard->SetValueAsBool(IsInCombatKey.SelectedKeyName, false);
		}
	}
}

void UGS_BTS_SeekerPerception::UpdateTrapPerception(UBehaviorTreeComponent& OwnerComp)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return;
	}

	// Respect evasion suppression from controller
	if (AGS_SeekerAIController* GSController = Cast<AGS_SeekerAIController>(AIController))
	{
		if (!GSController->ShouldEvade()) // This now returns false if suppressed
		{
			Blackboard->ClearValue(NearbyTrapKey.SelectedKeyName);
			return;
		}
	}

	// Determine dynamic detection radius
	float currentTrapRadius = TrapDetectionRadius;
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn);
	if (Seeker)
	{
		// Ranged characters (Merci) get 50% more detection range for traps
		bool bIsRanged = Seeker->ActorHasTag("Ranged") || Seeker->GetName().Contains("Merci");
		if (bIsRanged)
		{
			currentTrapRadius *= 1.5f;
		}
	}

	// Find nearest trap with dynamic radius
	TArray<AActor*> OverlappingActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Pawn);

	UKismetSystemLibrary::SphereOverlapActors(
	    Pawn->GetWorld(),
	    Pawn->GetActorLocation(),
	    currentTrapRadius,
	    ObjectTypes,
	    AGS_TrapBase::StaticClass(),
	    ActorsToIgnore,
	    OverlappingActors);

	AActor* NearestTrap = nullptr;
	float NearestDistance = currentTrapRadius;

	for (AActor* Actor : OverlappingActors)
	{
		AGS_TrapBase* Trap = Cast<AGS_TrapBase>(Actor);
		if (Trap && Trap->bIsActivated)
		{
			float Distance = FVector::Dist(Pawn->GetActorLocation(), Trap->GetActorLocation());
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				NearestTrap = Trap;
			}
		}
	}

	if (NearestTrap)
	{
		Blackboard->SetValueAsObject(NearbyTrapKey.SelectedKeyName, NearestTrap);
	}
	else
	{
		Blackboard->ClearValue(NearbyTrapKey.SelectedKeyName);
	}
}

void UGS_BTS_SeekerPerception::UpdateHealthStatus(UBehaviorTreeComponent& OwnerComp)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn);
	if (!Seeker)
	{
		return;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return;
	}

	// Get health percentage
	float HealthPercent = 1.0f;
	if (UGS_StatComp* StatComp = Seeker->GetStatComp())
	{
		float MaxHP = StatComp->GetMaxHealth();
		if (MaxHP > 0)
		{
			HealthPercent = StatComp->GetCurrentHealth() / MaxHP;
		}
	}

	// Update should heal flag
	bool bShouldHeal = HealthPercent <= HealThreshold;
	Blackboard->SetValueAsBool(ShouldHealKey.SelectedKeyName, bShouldHeal);

	// Store health percent
	Blackboard->SetValueAsFloat(AGS_SeekerAIController::CurrentHealthPercentKey, HealthPercent);
}

void UGS_BTS_SeekerPerception::UpdateGoalPerception(UBehaviorTreeComponent& OwnerComp)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return;
	}

	// Check if we already have a goal
	UObject* CurrentGoal = Blackboard->GetValueAsObject(GoalActorKey.SelectedKeyName);
	if (AGS_AIGoalTrigger* Goal = Cast<AGS_AIGoalTrigger>(CurrentGoal))
	{
		if (Goal->IsGoalActive() && !Goal->HasBeenReached())
		{
			// Still valid goal
			return;
		}
	}

	// Find nearest goal
	AActor* NearestGoal = FindNearestGoal(Pawn);
	if (NearestGoal)
	{
		Blackboard->SetValueAsObject(GoalActorKey.SelectedKeyName, NearestGoal);
		Blackboard->SetValueAsVector(AGS_SeekerAIController::ExplorationTargetKey, NearestGoal->GetActorLocation());
	}
}

AActor* UGS_BTS_SeekerPerception::FindNearestEnemy(AActor* Seeker) const
{
	if (!Seeker)
	{
		return nullptr;
	}

	AAIController* AIC = Cast<AAIController>(Seeker->GetOwner());
	if (!AIC || !AIC->GetPerceptionComponent())
	{
		// Fallback to expensive check if absolutely needed, but better to rely on perception
		return nullptr;
	}

	TArray<AActor*> PerceivedActors;
	AIC->GetPerceptionComponent()->GetKnownPerceivedActors(nullptr, PerceivedActors);

	AActor* NearestEnemy = nullptr;
	float NearestDistance = EnemyDetectionRadius;

	for (AActor* Actor : PerceivedActors)
	{
		if (AGS_Monster* Monster = Cast<AGS_Monster>(Actor))
		{
			if (!Monster->IsDead())
			{
				float Distance = FVector::Dist(Seeker->GetActorLocation(), Monster->GetActorLocation());
				if (Distance < NearestDistance)
				{
					NearestDistance = Distance;
					NearestEnemy = Monster;
				}
			}
		}
	}

	return NearestEnemy;
}

AActor* UGS_BTS_SeekerPerception::FindNearestTrap(AActor* Seeker) const
{
	if (!Seeker)
	{
		return nullptr;
	}

	// For traps, we can use OverlapMultiByChannel or cached manager
	// For now, let's use a Sphere Overlap which is MUCH cheaper than GetAllActorsOfClass
	TArray<AActor*> OverlappingActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Seeker);

	UKismetSystemLibrary::SphereOverlapActors(
	    Seeker->GetWorld(),
	    Seeker->GetActorLocation(),
	    TrapDetectionRadius,
	    ObjectTypes,
	    AGS_TrapBase::StaticClass(),
	    ActorsToIgnore,
	    OverlappingActors);

	AActor* NearestTrap = nullptr;
	float NearestDistance = TrapDetectionRadius;

	for (AActor* Actor : OverlappingActors)
	{
		AGS_TrapBase* Trap = Cast<AGS_TrapBase>(Actor);
		if (Trap && Trap->bIsActivated)
		{
			float Distance = FVector::Dist(Seeker->GetActorLocation(), Trap->GetActorLocation());
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				NearestTrap = Trap;
			}
		}
	}

	return NearestTrap;
}

AActor* UGS_BTS_SeekerPerception::FindNearestGoal(AActor* Seeker) const
{
	if (!Seeker)
	{
		return nullptr;
	}

	UWorld* World = Seeker->GetWorld();
	if (!World)
		return nullptr;

	UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>();
	if (!Registry)
		return nullptr;

	const TArray<TWeakObjectPtr<AGS_AIGoalTrigger>>& FoundGoals = Registry->GetGoalTriggers();

	AActor* BestGoal = nullptr;
	float BestScore = MAX_FLT;

	for (const TWeakObjectPtr<AGS_AIGoalTrigger>& GoalPtr : FoundGoals)
	{
		AGS_AIGoalTrigger* Goal = GoalPtr.Get();
		if (Goal && Goal->IsGoalActive() && !Goal->HasBeenReached())
		{
			float Distance = FVector::Dist(Seeker->GetActorLocation(), Goal->GetActorLocation());
			if (Distance < GoalDetectionRadius)
			{
				// Score based on distance and priority (lower is better)
				float Score = Distance - (Goal->GoalPriority * 1000.0f);
				if (Score < BestScore)
				{
					BestScore = Score;
					BestGoal = Goal;
				}
			}
		}
	}

	return BestGoal;
}

void UGS_BTS_SeekerPerception::UpdateAllyPerception(UBehaviorTreeComponent& OwnerComp)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController || !AIController->GetPerceptionComponent())
	{
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn);
	if (!Seeker)
	{
		return;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return;
	}

	// Use perception to find allies
	TArray<AActor*> PerceivedActors;
	AIController->GetPerceptionComponent()->GetKnownPerceivedActors(nullptr, PerceivedActors);

	AActor* BestDownedAlly = nullptr;
	float MinDistance = GoalDetectionRadius; // Use a large radius for allies

	for (AActor* Actor : PerceivedActors)
	{
		AGS_Seeker* Ally = Cast<AGS_Seeker>(Actor);
		// Check if it's a seeker, not self, and in dying state
		if (Ally && Ally != Seeker && Ally->IsInDyingState())
		{
			float Distance = FVector::Dist(Seeker->GetActorLocation(), Ally->GetActorLocation());
			if (Distance < MinDistance)
			{
				MinDistance = Distance;
				BestDownedAlly = Ally;
			}
		}
	}

	if (BestDownedAlly)
	{
		Blackboard->SetValueAsObject(DownedAllyKey.SelectedKeyName, BestDownedAlly);
	}
	else
	{
		// Clear immediately if no downed ally found in perception - important for dead allies!
		Blackboard->ClearValue(DownedAllyKey.SelectedKeyName);

		// Fallback: search nearby if perception is tricky
		UWorld* World = GetWorld();
		if (!World)
			return;

		UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>();
		if (!Registry)
			return;

		const TArray<TWeakObjectPtr<AGS_Seeker>>& NearbySeekers = Registry->GetSeekers();

		for (const TWeakObjectPtr<AGS_Seeker>& AllyPtr : NearbySeekers)
		{
			AGS_Seeker* Ally = AllyPtr.Get();
			if (Ally && Ally != Seeker && Ally->IsInDyingState())
			{
				float Distance = FVector::Dist(Seeker->GetActorLocation(), Ally->GetActorLocation());
				if (Distance < 1500.0f) // Within 15m
				{
					if (Distance < MinDistance)
					{
						MinDistance = Distance;
						BestDownedAlly = Ally;
					}
				}
			}
		}

		if (BestDownedAlly)
		{
			Blackboard->SetValueAsObject(DownedAllyKey.SelectedKeyName, BestDownedAlly);
		}
		else
		{
			Blackboard->ClearValue(DownedAllyKey.SelectedKeyName);
		}
	}
}

FString UGS_BTS_SeekerPerception::GetStaticDescription() const
{
	return FString::Printf(TEXT("Perception Service\nEnemy Range: %.0f\nTrap Range: %.0f\nHeal Threshold: %.0f%%"),
	                       EnemyDetectionRadius, TrapDetectionRadius, HealThreshold * 100.0f);
}
void UGS_BTS_SeekerPerception::UpdateInteractivePerception(UBehaviorTreeComponent& OwnerComp)
{
	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return;
	}

	// Find nearest interactive item (Chest, etc.)
	AActor* NearestItem = FindNearestInteractiveItem(Pawn);
	Blackboard->SetValueAsObject(InteractiveItemKey.SelectedKeyName, NearestItem);
}

AActor* UGS_BTS_SeekerPerception::FindNearestInteractiveItem(AActor* Seeker) const
{
	if (!Seeker)
	{
		return nullptr;
	}

	// Use Sphere Overlap to find nearby chests
	TArray<AActor*> OverlappingActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Seeker);

	UKismetSystemLibrary::SphereOverlapActors(
	    Seeker->GetWorld(),
	    Seeker->GetActorLocation(),
	    1000.0f, // Detection radius for chests
	    ObjectTypes,
	    AGS_EmberChest::StaticClass(),
	    ActorsToIgnore,
	    OverlappingActors);

	AActor* NearestItem = nullptr;
	float NearestDistanceSq = 1000.0f * 1000.0f;

	for (AActor* Actor : OverlappingActors)
	{
		// Check if it's an interactable we care about and if it can be interacted with
		if (IGS_InteractableInterface* Interactable = Cast<IGS_InteractableInterface>(Actor))
		{
			if (Interactable->Execute_CanInteract(Actor, Seeker))
			{
				float DistanceSq = FVector::DistSquared(Seeker->GetActorLocation(), Actor->GetActorLocation());
				if (DistanceSq < NearestDistanceSq)
				{
					NearestDistanceSq = DistanceSq;
					NearestItem = Actor;
				}
			}
		}
	}

	return NearestItem;
}
