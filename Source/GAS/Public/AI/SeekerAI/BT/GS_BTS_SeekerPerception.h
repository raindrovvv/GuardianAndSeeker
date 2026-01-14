// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "GS_BTS_SeekerPerception.generated.h"

/**
 * BT Service: Continuously update perception data
 * Monitors enemies, traps, and health status
 */
UCLASS()
class GAS_API UGS_BTS_SeekerPerception : public UBTService
{
	GENERATED_BODY()

public:
	UGS_BTS_SeekerPerception();

	// Perception Configuration
	UPROPERTY(EditAnywhere, Category = "Perception")
	float EnemyDetectionRadius = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "Perception")
	float TrapDetectionRadius = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Perception")
	float GoalDetectionRadius = 5000.0f;

	UPROPERTY(EditAnywhere, Category = "Perception|Health")
	float HealThreshold = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Perception|Health")
	float CriticalHealthThreshold = 0.15f;

	// Blackboard Keys
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetEnemyKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector NearbyTrapKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector ShouldHealKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector IsInCombatKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector GoalActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector LastKnownLocationKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector DownedAllyKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector InteractiveItemKey;

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;

private:
	void UpdateEnemyPerception(UBehaviorTreeComponent& OwnerComp);
	void UpdateTrapPerception(UBehaviorTreeComponent& OwnerComp);
	void UpdateHealthStatus(UBehaviorTreeComponent& OwnerComp);
	void UpdateGoalPerception(UBehaviorTreeComponent& OwnerComp);
	void UpdateAllyPerception(UBehaviorTreeComponent& OwnerComp);
	void UpdateInteractivePerception(UBehaviorTreeComponent& OwnerComp);

	AActor* FindNearestEnemy(AActor* Seeker) const;
	AActor* FindNearestTrap(AActor* Seeker) const;
	AActor* FindNearestGoal(AActor* Seeker) const;
	AActor* FindNearestInteractiveItem(AActor* Seeker) const;
};
