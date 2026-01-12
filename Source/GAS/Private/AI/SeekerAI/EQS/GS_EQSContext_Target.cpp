#include "AI/SeekerAI/EQS/GS_EQSContext_Target.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"

void UGS_EQSContext_Target::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	AActor* QueryOwner = Cast<AActor>(QueryInstance.Owner.Get());
	if (!QueryOwner)
	{
		return;
	}

	// Get the AI Controller
	APawn* Pawn = Cast<APawn>(QueryOwner);
	if (!Pawn)
	{
		return;
	}

	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(Pawn->GetController());
	if (!AIController)
	{
		return;
	}

	// Get target from blackboard
	UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent();
	if (!Blackboard)
	{
		return;
	}

	AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(AGS_SeekerAIController::TargetEnemyKey));
	if (TargetActor)
	{
		// Provide actor as context
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, TargetActor);
	}
	else
	{
		// Fallback: try to get last known enemy location
		FVector LastKnownLocation = Blackboard->GetValueAsVector(AGS_SeekerAIController::LastKnownEnemyLocationKey);
		if (!LastKnownLocation.IsNearlyZero())
		{
			UEnvQueryItemType_Point::SetContextHelper(ContextData, LastKnownLocation);
		}
	}
}
