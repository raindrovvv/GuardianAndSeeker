// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "GS_SeekerAIController.generated.h"

class UBehaviorTreeComponent;
class UBlackboardComponent;
class UAISenseConfig_Sight;
class AGS_AISeeker;
class AGS_Monster;
class AGS_TrapBase;
class AGS_AIGoalTrigger;

/**
 * AI Controller for autonomous Seeker that explores dungeons
 * Handles perception, navigation, and decision making
 */
UCLASS()
class GAS_API AGS_SeekerAIController : public AAIController
{
	GENERATED_BODY()

public:
	AGS_SeekerAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Blackboard Key Names
	static const FName TargetEnemyKey;
	static const FName TargetLocationKey;
	static const FName GoalActorKey;
	static const FName CurrentHealthPercentKey;
	static const FName IsInCombatKey;
	static const FName ShouldHealKey;
	static const FName ShouldEvadeKey;
	static const FName NearbyTrapKey;
	static const FName ExplorationTargetKey;
	static const FName HasReachedGoalKey;
	static const FName LastKnownEnemyLocationKey;
	static const FName DownedAllyKey;
	static const FName InteractiveItemKey;

	// Perception Configuration
	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float SightRadius = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float LoseSightRadius = 2500.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float PeripheralVisionAngleDegrees = 90.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float AutoSuccessRangeFromLastSeenLocation = 500.0f;

	// Combat Configuration
	UPROPERTY(EditAnywhere, Category = "AI|Combat")
	float AttackRange = 200.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Combat")
	float HealThreshold = 0.3f; // Start healing below 30% HP

	UPROPERTY(EditAnywhere, Category = "AI|Combat")
	float CriticalHealThreshold = 0.15f; // Critical heal below 15% HP

	// Trap Avoidance Configuration
	UPROPERTY(EditAnywhere, Category = "AI|TrapAvoidance")
	float TrapDetectionRadius = 500.0f;

	UPROPERTY(EditAnywhere, Category = "AI|TrapAvoidance")
	float TrapAvoidanceDistance = 300.0f;

	// Exploration Configuration
	UPROPERTY(EditAnywhere, Category = "AI|Exploration")
	float ExplorationRadius = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Exploration")
	float MinExplorationDistance = 500.0f;

	// AI State Management
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetTargetEnemy(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "AI")
	void ClearTargetEnemy();

	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetGoalActor(AGS_AIGoalTrigger* GoalTrigger);

	UFUNCTION(BlueprintCallable, Category = "AI")
	void OnGoalReached();

	UFUNCTION(BlueprintCallable, Category = "AI")
	void UpdateHealthStatus();

	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetNearbyTrap(AGS_TrapBase* Trap);

	UFUNCTION(BlueprintCallable, Category = "AI")
	void ClearNearbyTrap();

	UFUNCTION(BlueprintPure, Category = "AI")
	bool IsInCombat() const;

	UFUNCTION(BlueprintPure, Category = "AI")
	bool ShouldHeal() const;

	UFUNCTION(BlueprintPure, Category = "AI")
	bool ShouldEvade() const;

	UFUNCTION(BlueprintPure, Category = "AI")
	AGS_AISeeker* GetControlledSeeker() const { return ControlledSeeker.Get(); }

	// Exploration
	UFUNCTION(BlueprintCallable, Category = "AI|Exploration")
	bool FindNewExplorationTarget();

	UFUNCTION(BlueprintCallable, Category = "AI|Exploration")
	FVector GetRandomPointInNavigableRadius(float Radius) const;

	// Combat
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	AActor* FindBestTarget() const;

	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	float GetDistanceToTarget(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	bool IsTargetInAttackRange(AActor* Target) const;

	// Trap Avoidance
	UFUNCTION(BlueprintCallable, Category = "AI|TrapAvoidance")
	AGS_TrapBase* DetectNearbyTraps() const;

	UFUNCTION(BlueprintCallable, Category = "AI|TrapAvoidance")
	FVector CalculateTrapAvoidanceDirection(AGS_TrapBase* Trap) const;

	UFUNCTION(BlueprintCallable, Category = "AI|TrapAvoidance")
	bool IsPathBlockedByTrap(const FVector& Destination) const;

	// IGenericTeamAgentInterface
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	UFUNCTION(BlueprintCallable, Category = "AI|Revive")
	bool IsHoldingReviveKey() const { return bIsHoldingReviveKey; }

	UFUNCTION(BlueprintCallable, Category = "AI|Revive")
	AActor* FindNearestHealthyAlly() const;

	UFUNCTION(BlueprintCallable, Category = "AI|Revive")
	void SetHoldingReviveKey(bool bHolding) { bIsHoldingReviveKey = bHolding; }

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaTime) override;

	// Perception
	UPROPERTY(VisibleAnywhere, Category = "AI|Perception")
	UAISenseConfig_Sight* SightConfig;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	// Behavior Tree Assets (set in Blueprint)
	UPROPERTY(EditDefaultsOnly, Category = "AI|BehaviorTree")
	TSoftObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UPROPERTY(EditDefaultsOnly, Category = "AI|BehaviorTree")
	TSoftObjectPtr<UBlackboardData> BlackboardAsset;

private:
	UPROPERTY()
	TWeakObjectPtr<AGS_AISeeker> ControlledSeeker;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTargetEnemy;

	UPROPERTY()
	TWeakObjectPtr<AGS_AIGoalTrigger> CurrentGoal;

	UPROPERTY()
	TArray<TWeakObjectPtr<AGS_TrapBase>> DetectedTraps;

	// Cache for visited locations (exploration)
	UPROPERTY()
	TArray<FVector> VisitedLocations;

	float VisitedLocationRadius = 200.0f;

	bool HasVisitedLocation(const FVector& Location) const;
	void MarkLocationVisited(const FVector& Location);

	// Stuck detection for trap forest/corners
	FVector LastPosition = FVector::ZeroVector;
	float StationaryTime = 0.0f;
	bool bIsEvasionSuppressed = false;
	float EvasionSuppressionTimer = 0.0f;

	// Exploration Markers
	float LastMarkerPlaceTime = 0.0f;
	const float MarkerCooldown = 30.0f; // Drop marker every 30 seconds of exploration

	// Revive state
	bool bIsHoldingReviveKey = false;
};
