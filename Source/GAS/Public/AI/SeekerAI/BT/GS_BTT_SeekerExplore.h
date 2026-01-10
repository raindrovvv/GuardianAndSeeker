// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GS_BTT_SeekerExplore.generated.h"

/**
 * BT Task: Find and move to new exploration point
 * Used when AI Seeker is exploring the dungeon
 */
UCLASS()
class GAS_API UGS_BTT_SeekerExplore : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UGS_BTT_SeekerExplore();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// Configuration
	UPROPERTY(EditAnywhere, Category = "Exploration")
	float ExplorationRadius = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "Exploration")
	float AcceptableRadius = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Exploration")
	bool bPreferUnvisitedAreas = true;

protected:
	virtual FString GetStaticDescription() const override;
};
