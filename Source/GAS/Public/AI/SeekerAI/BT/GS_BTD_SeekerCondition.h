// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "GS_BTD_SeekerCondition.generated.h"

/**
 * Condition type for Seeker AI decisions
 */
UENUM(BlueprintType)
enum class ESeekerConditionType : uint8
{
	HasTargetEnemy UMETA(DisplayName = "Has Target Enemy"),
	IsLowHealth UMETA(DisplayName = "Is Low Health"),
	IsCriticalHealth UMETA(DisplayName = "Is Critical Health"),
	IsNearTrap UMETA(DisplayName = "Is Near Trap"),
	HasReachedGoal UMETA(DisplayName = "Has Reached Goal"),
	CanUseSkill UMETA(DisplayName = "Can Use Skill"),
	CanHeal UMETA(DisplayName = "Can Heal"),
	IsInCombat UMETA(DisplayName = "Is In Combat"),
	IsTargetInRange UMETA(DisplayName = "Is Target In Attack Range"),
	IsAlive UMETA(DisplayName = "Is Alive"),
	HasDownedAlly UMETA(DisplayName = "Has Downed Ally"),
	HasInteractiveItem UMETA(DisplayName = "Has Interactive Item")
};

/**
 * BT Decorator: Various conditions for Seeker AI behavior
 */
UCLASS()
class GAS_API UGS_BTD_SeekerCondition : public UBTDecorator
{
	GENERATED_BODY()

public:
	UGS_BTD_SeekerCondition();

	// Condition to check
	UPROPERTY(EditAnywhere, Category = "Condition")
	ESeekerConditionType ConditionType = ESeekerConditionType::HasTargetEnemy;

	// Optional: Skill index for CanUseSkill condition
	UPROPERTY(EditAnywhere, Category = "Condition", meta = (EditCondition = "ConditionType == ESeekerConditionType::CanUseSkill"))
	int32 SkillIndex = 0;

	// Optional: Health threshold for custom low health check
	UPROPERTY(EditAnywhere, Category = "Condition", meta = (EditCondition = "ConditionType == ESeekerConditionType::IsLowHealth", ClampMin = "0.0", ClampMax = "1.0"))
	float CustomHealthThreshold = 0.3f;

	// Optional: Distance threshold for range checks
	UPROPERTY(EditAnywhere, Category = "Condition", meta = (EditCondition = "ConditionType == ESeekerConditionType::IsTargetInRange"))
	float RangeThreshold = 200.0f;

	// Blackboard Keys
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;

private:
	bool CheckHasTargetEnemy(UBehaviorTreeComponent& OwnerComp) const;
	bool CheckIsLowHealth(UBehaviorTreeComponent& OwnerComp) const;
	bool CheckIsCriticalHealth(UBehaviorTreeComponent& OwnerComp) const;
	bool CheckIsNearTrap(UBehaviorTreeComponent& OwnerComp) const;
	bool CheckHasReachedGoal(UBehaviorTreeComponent& OwnerComp) const;
	bool CheckCanUseSkill(UBehaviorTreeComponent& OwnerComp) const;
	bool CheckCanHeal(UBehaviorTreeComponent& OwnerComp) const;
	bool CheckIsInCombat(UBehaviorTreeComponent& OwnerComp) const;
	bool CheckIsTargetInRange(UBehaviorTreeComponent& OwnerComp) const;
	bool CheckIsAlive(UBehaviorTreeComponent& OwnerComp) const;
	bool CheckHasDownedAlly(UBehaviorTreeComponent& OwnerComp) const;
	bool CheckHasInteractiveItem(UBehaviorTreeComponent& OwnerComp) const;
};
