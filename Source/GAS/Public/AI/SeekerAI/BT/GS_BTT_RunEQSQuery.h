#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "GS_BTT_RunEQSQuery.generated.h"

class UEnvQuery;

/**
 * BT Task that runs an EQS query and stores the best result in Blackboard
 * Supports different query types: Cover, Flanking, SafeRetreat, etc.
 */
UCLASS()
class GAS_API UGS_BTT_RunEQSQuery : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UGS_BTT_RunEQSQuery();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** The EQS Query asset to run */
	UPROPERTY(EditAnywhere, Category = "EQS")
	UEnvQuery* QueryTemplate;

	/** Blackboard key to store the result location */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector ResultLocationKey;

	/** Run mode for the query */
	UPROPERTY(EditAnywhere, Category = "EQS")
	TEnumAsByte<EEnvQueryRunMode::Type> RunMode;

	/** Timeout for query execution */
	UPROPERTY(EditAnywhere, Category = "EQS")
	float QueryTimeout = 2.0f;

private:
	/** Callback when EQS query finishes */
	void OnQueryFinished(TSharedPtr<FEnvQueryResult> Result);

	/** Cached reference to behavior tree component */
	UPROPERTY()
	UBehaviorTreeComponent* CachedOwnerComp;

	/** Query request ID for tracking */
	int32 QueryRequestID;
};
