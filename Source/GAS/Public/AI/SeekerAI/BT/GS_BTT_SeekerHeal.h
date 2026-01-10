// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GS_BTT_SeekerHeal.generated.h"

/**
 * BT Task: Use healing skill/potion when low health
 * Priority task when HP drops below threshold
 */
UCLASS()
class GAS_API UGS_BTT_SeekerHeal : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UGS_BTT_SeekerHeal();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// Healing Configuration
	UPROPERTY(EditAnywhere, Category = "Healing")
	float HealCooldown = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Healing")
	bool bFindSafePositionFirst = true;

	UPROPERTY(EditAnywhere, Category = "Healing")
	float SafePositionSearchRadius = 500.0f;

protected:
	virtual FString GetStaticDescription() const override;

private:
	float LastHealTime = 0.0f;

	// Find a safe position away from combat for healing
	FVector FindSafePosition(class AGS_SeekerAIController* AIController, const FVector& CurrentLocation) const;
};
