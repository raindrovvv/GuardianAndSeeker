// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GS_BTT_SeekerCombat.generated.h"

/**
 * BT Task: Engage in combat with target enemy
 * Handles attacking, skill usage, and positioning
 */
UCLASS()
class GAS_API UGS_BTT_SeekerCombat : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UGS_BTT_SeekerCombat();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// Combat Configuration
	UPROPERTY(EditAnywhere, Category = "Combat")
	float AttackCooldown = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float SkillUsageChance = 0.3f; // 30% chance to use skill instead of basic attack

	UPROPERTY(EditAnywhere, Category = "Combat")
	bool bUseSkillsWhenAvailable = true;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float OptimalMeleeRange = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float OptimalRangedRange = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|Blackboard")
	FBlackboardKeySelector TargetActorKey;

protected:
	virtual FString GetStaticDescription() const override;

private:
	float LastAttackTime = 0.0f;
	float LastKiteTime = 0.0f;
};
