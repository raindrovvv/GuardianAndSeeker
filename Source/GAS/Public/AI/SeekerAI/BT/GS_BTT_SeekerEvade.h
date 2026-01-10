// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GS_BTT_SeekerEvade.generated.h"

/**
 * BT Task: Evade traps and dangerous areas
 * Handles rolling, repositioning, and avoiding hazards
 */
UCLASS()
class GAS_API UGS_BTT_SeekerEvade : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UGS_BTT_SeekerEvade();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// Evasion Configuration
	UPROPERTY(EditAnywhere, Category = "Evasion")
	float EvadeDistance = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Evasion")
	bool bPreferRollOverWalk = true;

	UPROPERTY(EditAnywhere, Category = "Evasion")
	float RollCooldown = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Evasion")
	bool bUseEvadeSkillIfAvailable = true;

	UPROPERTY(EditAnywhere, Category = "Evasion|Blackboard")
	FBlackboardKeySelector TrapLocationKey;

protected:
	virtual FString GetStaticDescription() const override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FGS_BTTEvadeMemory); }

private:
	struct FGS_BTTEvadeMemory
	{
		float StartTime;
	};

	float LastRollTime = 0.0f;
	float LastEvadeTaskFinishTime = 0.0f;
	const float GlobalEvadeCooldown = 4.0f;

	FVector CalculateSafeEvadeDirection(const FVector& ThreatLocation, const FVector& CurrentLocation) const;
};
