#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GS_BTT_SeekerInteract.generated.h"

/**
 * BT Task: Interact with objects (Chests, Doors, etc.)
 */
UCLASS()
class GAS_API UGS_BTT_SeekerInteract : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UGS_BTT_SeekerInteract();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Interaction")
	float AcceptableDistance = 150.0f;

	// Timer for interaction progress
	float InteractionTimer = 0.0f;
	float TotalInteractionDuration = 0.0f;

	TWeakObjectPtr<AActor> CurrentTarget;
};
