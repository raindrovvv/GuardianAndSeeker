// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "AI/SeekerAI/GS_AISeeker.h"
#include "AI/SeekerAI/GS_AIGoalTrigger.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "NavigationSystem.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Component/Seeker/GS_MarkerPlacementComponent.h"
#include "Props/Trap/GS_TrapBase.h"
#include "Props/Interactables/GS_Door.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"
#include "Engine/OverlapResult.h"

// Blackboard Key Names
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

AGS_SeekerAIController::AGS_SeekerAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent")))
{
	// Create and configure sight perception
	SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception")));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	if (SightConfig)
	{
		SightConfig->SightRadius = SightRadius;
		SightConfig->LoseSightRadius = LoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
		SightConfig->SetMaxAge(5.0f);
		SightConfig->AutoSuccessRangeFromLastSeenLocation = AutoSuccessRangeFromLastSeenLocation;

		// Detect enemies (monsters) and allies (for revive)
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

		GetPerceptionComponent()->ConfigureSense(*SightConfig);
		GetPerceptionComponent()->SetDominantSense(SightConfig->GetSenseImplementation());
	}

	// Team setup - Seeker team
	SetGenericTeamId(FGenericTeamId(1)); // Team 1: Seeker
}

void AGS_SeekerAIController::BeginPlay()
{
	Super::BeginPlay();

	// Bind perception update
	if (GetPerceptionComponent())
	{
		GetPerceptionComponent()->OnTargetPerceptionUpdated.AddDynamic(this, &AGS_SeekerAIController::OnTargetPerceptionUpdated);
	}
}

void AGS_SeekerAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UE_LOG(LogTemp, Error, TEXT("[SeekerAIController::OnPossess] InPawn: %s (Class: %s)"),
	       InPawn ? *InPawn->GetName() : TEXT("NULL"),
	       InPawn ? *InPawn->GetClass()->GetName() : TEXT("NULL"));

	if (InPawn)
	{
		AActor* PawnOwner = InPawn->GetOwner();
		UE_LOG(LogTemp, Error, TEXT("[SeekerAIController::OnPossess] InPawn Owner: %s (Class: %s)"),
		       PawnOwner ? *PawnOwner->GetName() : TEXT("NULL"),
		       PawnOwner ? *PawnOwner->GetClass()->GetName() : TEXT("NULL"));
	}

	// Cache the controlled seeker
	if (AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(GetOwner()))
	{
		ControlledSeeker = AISeeker;
		UE_LOG(LogTemp, Log, TEXT("[SeekerAIController::OnPossess] ControlledSeeker SET: %s"), *AISeeker->GetName());
	}

	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(InPawn))
	{
		// Configure Seeker for AI rotation management
		if (UCharacterMovementComponent* MoveComp = Seeker->GetCharacterMovement())
		{
			MoveComp->bOrientRotationToMovement = false; // We will handle rotation in Controller
			MoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);

			// Monster Collision Fix: Enable RVO Avoidance to prevent "rubbing/pushing" issues
			MoveComp->bUseRVOAvoidance = true;
			MoveComp->AvoidanceConsiderationRadius = 200.0f;
		}
		Seeker->bUseControllerRotationYaw = false;
	}

	// Initialize Behavior Tree
	if (BehaviorTreeAsset.IsValid() || BehaviorTreeAsset.ToSoftObjectPath().IsValid())
	{
		UBehaviorTree* LoadedBT = BehaviorTreeAsset.LoadSynchronous();
		if (LoadedBT)
		{
			// Initialize Blackboard first
			UBlackboardComponent* BlackboardComponent = nullptr;
			if (BlackboardAsset.IsValid() || BlackboardAsset.ToSoftObjectPath().IsValid())
			{
				UBlackboardData* LoadedBB = BlackboardAsset.LoadSynchronous();
				if (LoadedBB)
				{
					UseBlackboard(LoadedBB, BlackboardComponent);
					Blackboard = BlackboardComponent;
				}
			}
			else if (LoadedBT->BlackboardAsset)
			{
				UseBlackboard(LoadedBT->BlackboardAsset, BlackboardComponent);
				Blackboard = BlackboardComponent;
			}

			// Run the behavior tree
			RunBehaviorTree(LoadedBT);
		}
	}
}

void AGS_SeekerAIController::OnUnPossess()
{
	Super::OnUnPossess();
}

void AGS_SeekerAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Stuck detection for trap forests/corners
	APawn* ControlledPawn = GetPawn();
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(ControlledPawn);
	if (ControlledPawn)
	{
		float DistanceMoved = FVector::Dist(ControlledPawn->GetActorLocation(), LastPosition);

		// If barely moved (threshold 5.0 units)
		bool bOnDangerousTrap = DetectNearbyTraps() != nullptr;
		float StuckThreshold = bOnDangerousTrap ? 1.0f : 3.0f; // More aggressive escape if on/near trap

		// Only count as stationary if we are actually TRYING to move
		bool bIsTryingToMove = GetMoveStatus() == EPathFollowingStatus::Moving;

		// If it's a ranged character (Merci), being stationary while aiming/drawing is normal
		if (Seeker)
		{
			if (Seeker->GetAimState() || Seeker->GetDrawState())
			{
				bIsTryingToMove = false;
			}
		}

		if (DistanceMoved < 5.0f && bIsTryingToMove)
		{
			StationaryTime += DeltaTime;
		}
		else
		{
			StationaryTime = 0.0f;
			LastPosition = ControlledPawn->GetActorLocation();
		}

		// If stuck, suppress trap evasion and try to escape
		if (StationaryTime > StuckThreshold && !bIsEvasionSuppressed)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AI] %s stuck for %.1fs (OnTrap: %s), suppressed evasion to escape."),
			       *ControlledPawn->GetName(), StationaryTime, bOnDangerousTrap ? TEXT("Yes") : TEXT("No"));
			bIsEvasionSuppressed = true;
			EvasionSuppressionTimer = 5.0f;

			// Try to find a safe spot to "jostle" the AI out of the corner
			UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
			if (NavSystem)
			{
				FNavLocation RandomSafeSpot;
				// First try to move towards the goal if available
				FVector EscapeDir = ControlledPawn->GetActorForwardVector() * -1.0f; // Default try back
				if (Blackboard)
				{
					FVector GoalLoc = Blackboard->GetValueAsVector(ExplorationTargetKey);
					if (!GoalLoc.IsNearlyZero())
					{
						EscapeDir = (GoalLoc - ControlledPawn->GetActorLocation()).GetSafeNormal();
					}
				}

				FVector TestLoc = ControlledPawn->GetActorLocation() + EscapeDir * 300.0f;
				if (NavSystem->ProjectPointToNavigation(TestLoc, RandomSafeSpot))
				{
					MoveToLocation(RandomSafeSpot.Location, 50.0f);
				}
				else if (NavSystem->GetRandomReachablePointInRadius(ControlledPawn->GetActorLocation(), 500.0f, RandomSafeSpot))
				{
					MoveToLocation(RandomSafeSpot.Location, 50.0f);
				}
			}
		}

		// Handle suppression timer
		if (bIsEvasionSuppressed)
		{
			EvasionSuppressionTimer -= DeltaTime;
			if (EvasionSuppressionTimer <= 0.0f)
			{
				bIsEvasionSuppressed = false;
				StationaryTime = 0.0f; // Reset stationary time to avoid immediate re-suppression
			}
		}

		// Periodically mark exploration locations with decals (same as player decal system)
		MarkLocationVisited(ControlledPawn->GetActorLocation());

		// --- Orientation Logic (Fix Moonwalking) ---
		if (AActor* FocusActor = GetFocusActor())
		{
			// In combat: look at the enemy
			FVector TargetLoc = FocusActor->GetActorLocation();
			FVector Dir = TargetLoc - ControlledPawn->GetActorLocation();
			Dir.Z = 0.0f;
			if (!Dir.IsNearlyZero())
			{
				FRotator TargetRot = Dir.Rotation();

				// Smoothly rotate the pawn toward the focus target
				float InterpSpeed = 12.0f;
				FRotator NewRot = FMath::RInterpTo(ControlledPawn->GetActorRotation(), TargetRot, DeltaTime, InterpSpeed);
				ControlledPawn->SetActorRotation(NewRot);
			}
		}
		else if (bIsTryingToMove)
		{
			// Exploring: look toward movement direction
			FVector Velocity = ControlledPawn->GetVelocity();
			if (Velocity.SizeSquared() > 100.0f)
			{
				FRotator TargetRot = Velocity.Rotation();
				TargetRot.Pitch = 0.0f;
				TargetRot.Roll = 0.0f;

				// Exploration rotation can be slightly slower/smoother
				FRotator NewRot = FMath::RInterpTo(ControlledPawn->GetActorRotation(), TargetRot, DeltaTime, 8.0f);
				ControlledPawn->SetActorRotation(NewRot);
			}
		}

		// --- Dying State & Monster Crowding Logic ---
		if (Seeker)
		{
			if (Seeker->IsInDyingState())
			{
				if (Seeker->IsBeingRevived())
				{
					// Stay still while being rescued as requested
					StopMovement();
					return;
				}

				// Find nearest healthy comrade to crawl towards (don't play normal AI)
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
				return; // Skip normal combat/explore logic while dying
			}

			// Monster Collision Jittering Prevention: Stop pushing if "hugging" any monster too tightly
			TArray<FOverlapResult> Overlaps;
			FCollisionQueryParams CollisionParams;
			CollisionParams.AddIgnoredActor(ControlledPawn);
			FCollisionShape Sphere = FCollisionShape::MakeSphere(180.0f);
			if (GetWorld() && GetWorld()->OverlapMultiByChannel(Overlaps, ControlledPawn->GetActorLocation(), FQuat::Identity, ECC_Pawn, Sphere, CollisionParams))
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
							// If moving towards this monster
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
					StopMovement();
					StationaryTime = 0.0f; // Intentionally stopped
				}
			}

			// --- Door Wait Logic: Stop pushing against closed doors ---
			// Only check when actually close to a door to avoid stopping too early
			TArray<AActor*> OverlappingDoors;
			TArray<TEnumAsByte<EObjectTypeQuery>> DoorObjectTypes;
			DoorObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

			TArray<AActor*> ActorsToIgnore;
			ActorsToIgnore.Add(ControlledPawn);

			// Smaller detection radius - only check in immediate front
			UKismetSystemLibrary::SphereOverlapActors(
			    GetWorld(),
			    ControlledPawn->GetActorLocation(),
			    300.0f, // Detection radius around the AI
			    DoorObjectTypes,
			    AGS_Door::StaticClass(),
			    ActorsToIgnore,
			    OverlappingDoors);

			for (AActor* Actor : OverlappingDoors)
			{
				if (AGS_Door* Door = Cast<AGS_Door>(Actor))
				{
					// Calculate actual distance to the door
					float DistToDoor = FVector::Dist(ControlledPawn->GetActorLocation(), Door->GetActorLocation());

					// Only stop if VERY close to a closed door (within 200 units)
					if (!Door->bIsOpen && DistToDoor < 200.0f && GetMoveStatus() == EPathFollowingStatus::Moving)
					{
						// Door is closed and we're close to it - wait instead of pushing
						FVector ToDoor = (Door->GetActorLocation() - ControlledPawn->GetActorLocation()).GetSafeNormal();
						FVector VelocityDir = ControlledPawn->GetVelocity().GetSafeNormal();

						// Only stop if moving toward this door
						if (FVector::DotProduct(ToDoor, VelocityDir) > 0.2f)
						{
							StopMovement();
							StationaryTime = 0.0f; // Intentionally waiting for door
							UE_LOG(LogTemp, Verbose, TEXT("[SeekerAI] %s waiting for door (dist: %.0f)"), *ControlledPawn->GetName(), DistToDoor);
							break;
						}
					}
				}
			}
		}
	}
}

void AGS_SeekerAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor)
	{
		return;
	}

	// Check if it's a monster (enemy)
	if (AGS_Monster* Monster = Cast<AGS_Monster>(Actor))
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			// Enemy detected - set as target if we don't have one
			if (!CurrentTargetEnemy.IsValid())
			{
				SetTargetEnemy(Monster);
			}
		}
		else
		{
			// Lost sight of enemy
			if (CurrentTargetEnemy == Actor)
			{
				// Store last known location
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
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn || !Blackboard)
	{
		return;
	}

	float HealthPercent = 1.0f;

	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(ControlledPawn))
	{
		if (UGS_StatComp* StatComp = Seeker->GetStatComp())
		{
			HealthPercent = StatComp->GetCurrentHealth() / StatComp->GetMaxHealth();
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
	FVector NewTarget = GetRandomPointInNavigableRadius(ExplorationRadius);

	if (!NewTarget.IsZero() && !HasVisitedLocation(NewTarget))
	{
		if (Blackboard)
		{
			Blackboard->SetValueAsVector(ExplorationTargetKey, NewTarget);
		}
		return true;
	}

	return false;
}

FVector AGS_SeekerAIController::GetRandomPointInNavigableRadius(float Radius) const
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return FVector::ZeroVector;
	}

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		return FVector::ZeroVector;
	}

	FNavLocation ResultLocation;
	bool bSuccess = NavSystem->GetRandomReachablePointInRadius(
	    ControlledPawn->GetActorLocation(),
	    Radius,
	    ResultLocation);

	return bSuccess ? ResultLocation.Location : FVector::ZeroVector;
}

AActor* AGS_SeekerAIController::FindBestTarget() const
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
		return nullptr;

	UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>();
	if (!Registry)
		return nullptr;

	const TArray<TWeakObjectPtr<AGS_Monster>>& FoundMonsters = Registry->GetMonsters();

	AActor* BestTarget = nullptr;
	float BestDistance = MAX_FLT;

	for (const TWeakObjectPtr<AGS_Monster>& MonsterPtr : FoundMonsters)
	{
		AGS_Monster* Monster = MonsterPtr.Get();
		if (Monster && !Monster->IsDead())
		{
			float Distance = FVector::Dist(ControlledPawn->GetActorLocation(), Monster->GetActorLocation());
			if (Distance < BestDistance && Distance <= SightRadius)
			{
				BestDistance = Distance;
				BestTarget = Monster;
			}
		}
	}

	return BestTarget;
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

	// Use Sphere Overlap instead of GetAllActorsOfClass for performance
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

	// Get movement direction
	FVector Velocity = ControlledPawn->GetVelocity();
	bool bIsMoving = Velocity.Size() > 10.0f;
	FVector MoveDir = bIsMoving ? Velocity.GetSafeNormal() : ControlledPawn->GetActorForwardVector();

	for (AActor* Actor : FoundTraps)
	{
		AGS_TrapBase* Trap = Cast<AGS_TrapBase>(Actor);
		if (Trap && Trap->bIsActivated)
		{
			FVector ToTrap = Trap->GetActorLocation() - ControlledPawn->GetActorLocation();
			float Distance = ToTrap.Size();

			if (Distance < NearestDistance)
			{
				// Path Relevancy Check: Is the trap ahead of us?
				FVector ToTrapDir = ToTrap.GetSafeNormal();
				float CosTheta = FVector::DotProduct(MoveDir, ToTrapDir);

				// Only consider traps within a ~120 degree cone in front (Cos(60) = 0.5)
				// If not moving/idling, be more cautious (Cos(90) = 0)
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

	// Get direction away from trap
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

	TArray<AActor*> FoundTraps;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGS_TrapBase::StaticClass(), FoundTraps);

	FVector Start = ControlledPawn->GetActorLocation();
	FVector Direction = (Destination - Start).GetSafeNormal();
	float PathLength = FVector::Dist(Start, Destination);

	for (AActor* Actor : FoundTraps)
	{
		AGS_TrapBase* Trap = Cast<AGS_TrapBase>(Actor);
		if (Trap && Trap->bIsActivated)
		{
			FVector TrapLocation = Trap->GetActorLocation();

			// Check if trap is close to the path
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
	return FGenericTeamId(1); // Team 1: Seeker
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
			else if (OtherTeamId == FGenericTeamId(2)) // Team 2: Monster
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
		if (!Seeker || Seeker == MyPawn || Seeker->IsDead() || Seeker->IsInDyingState())
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

	// Limit visited locations cache
	if (VisitedLocations.Num() > 100)
	{
		VisitedLocations.RemoveAt(0);
	}

	// Implement exploration markers - same mechanism as player decal system
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastMarkerPlaceTime >= MarkerCooldown)
	{
		if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(GetPawn()))
		{
			if (Seeker->MarkerPlacementComponent)
			{
				// Set marker type to exploration (assuming Type::X or similar represents 'explored')
				Seeker->MarkerPlacementComponent->SetSelectedMarkerType(EMarkerType::X);

				// Drop marker in front of the AI (where they're looking), not at feet
				FVector ForwardDir = Seeker->GetActorForwardVector();
				ForwardDir.Z = 0.0f;
				ForwardDir.Normalize();

				// Place marker 200 units in front of the AI
				FVector MarkerLocation = Seeker->GetActorLocation() + ForwardDir * 200.0f;

				// Trace down to find the floor
				FHitResult FloorHit;
				FVector TraceStart = MarkerLocation + FVector(0.0f, 0.0f, 100.0f);
				FVector TraceEnd = MarkerLocation - FVector(0.0f, 0.0f, 500.0f);

				FCollisionQueryParams TraceParams;
				TraceParams.AddIgnoredActor(Seeker);

				if (GetWorld()->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
				{
					MarkerLocation = FloorHit.Location;
				}
				else
				{
					// Fallback to ground level if trace fails
					MarkerLocation.Z = Seeker->GetActorLocation().Z - 90.0f;
				}

				FRotator DropRotation = FRotator(-90.0f, Seeker->GetActorRotation().Yaw, 0.0f); // Face floor, aligned with AI direction

				Seeker->MarkerPlacementComponent->Server_SpawnMarker(MarkerLocation, DropRotation, EMarkerType::X);

				LastMarkerPlaceTime = CurrentTime;
				UE_LOG(LogTemp, Log, TEXT("[SeekerAI] %s placed exploration marker at %s"), *Seeker->GetName(), *MarkerLocation.ToString());
			}
		}
	}
}
