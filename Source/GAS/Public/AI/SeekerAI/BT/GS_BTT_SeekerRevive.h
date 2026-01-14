// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GS_BTT_SeekerRevive.generated.h"

/**
 * BT Task: Move to and revive a downed ally seeker
 */
UCLASS()
class GAS_API UGS_BTT_SeekerRevive : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UGS_BTT_SeekerRevive();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Revive")
	FBlackboardKeySelector DownedAllyKey;

	UPROPERTY(EditAnywhere, Category = "Revive")
	float ReviveRange = 250.0f;

	virtual FString GetStaticDescription() const override;
};
