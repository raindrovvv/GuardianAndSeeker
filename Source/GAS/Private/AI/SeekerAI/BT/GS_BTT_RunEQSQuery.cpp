#include "AI/SeekerAI/BT/GS_BTT_RunEQSQuery.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"

UGS_BTT_RunEQSQuery::UGS_BTT_RunEQSQuery()
{
	NodeName = TEXT("Run EQS Query");
	bNotifyTick = false;
	bCreateNodeInstance = true; // Need instance for async callback

	RunMode = EEnvQueryRunMode::SingleResult;
	QueryTimeout = 2.0f;
	QueryRequestID = INDEX_NONE;
	CachedOwnerComp = nullptr;
}

EBTNodeResult::Type UGS_BTT_RunEQSQuery::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (!QueryTemplate)
	{
		return EBTNodeResult::Failed;
	}

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	// Cache the owner component for the callback
	CachedOwnerComp = &OwnerComp;

	// Run the EQS query
	UEnvQueryManager* QueryManager = UEnvQueryManager::GetCurrent(Pawn->GetWorld());
	if (!QueryManager)
	{
		return EBTNodeResult::Failed;
	}

	FEnvQueryRequest QueryRequest(QueryTemplate, Pawn);
	QueryRequestID = QueryManager->RunQuery(
		QueryRequest, RunMode, FQueryFinishedSignature::CreateUObject(this, &UGS_BTT_RunEQSQuery::OnQueryFinished));

	if (QueryRequestID == INDEX_NONE)
	{
		return EBTNodeResult::Failed;
	}

	// Query is running asynchronously
	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UGS_BTT_RunEQSQuery::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// Cancel the query if it's still running
	if (QueryRequestID != INDEX_NONE)
	{
		APawn* Pawn = OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr;
		if (Pawn)
		{
			UEnvQueryManager* QueryManager = UEnvQueryManager::GetCurrent(Pawn->GetWorld());
			if (QueryManager)
			{
				QueryManager->AbortQuery(QueryRequestID);
			}
		}
		QueryRequestID = INDEX_NONE;
	}

	return EBTNodeResult::Aborted;
}

void UGS_BTT_RunEQSQuery::OnQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	QueryRequestID = INDEX_NONE;

	if (!CachedOwnerComp)
	{
		return;
	}

	// Check if the query was successful
	if (!Result.IsValid() || !Result->IsSuccessful())
	{
		FinishLatentTask(*CachedOwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Get the best result location
	FVector BestLocation = Result->GetItemAsLocation(0);

	// Store in blackboard
	UBlackboardComponent* Blackboard = CachedOwnerComp->GetBlackboardComponent();
	if (Blackboard && ResultLocationKey.IsSet())
	{
		Blackboard->SetValueAsVector(ResultLocationKey.SelectedKeyName, BestLocation);

		FinishLatentTask(*CachedOwnerComp, EBTNodeResult::Succeeded);
	}
	else
	{
		FinishLatentTask(*CachedOwnerComp, EBTNodeResult::Failed);
	}
}

FString UGS_BTT_RunEQSQuery::GetStaticDescription() const
{
	FString QueryName = QueryTemplate ? QueryTemplate->GetName() : TEXT("None");
	FString KeyName = ResultLocationKey.SelectedKeyName.ToString();

	return FString::Printf(TEXT("Query: %s\nStore in: %s"), *QueryName, *KeyName);
}
