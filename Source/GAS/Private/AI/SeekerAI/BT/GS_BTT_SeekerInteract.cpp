#include "AI/SeekerAI/BT/GS_BTT_SeekerInteract.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Interface/GS_InteractableInterface.h"
#include "GameFramework/Pawn.h"

UGS_BTT_SeekerInteract::UGS_BTT_SeekerInteract()
{
	NodeName = "Seeker Interact";
	bNotifyTick = true;
	// AbortTask override handles abort notification
}

EBTNodeResult::Type UGS_BTT_SeekerInteract::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
		return EBTNodeResult::Failed;

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
		return EBTNodeResult::Failed;

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
		return EBTNodeResult::Failed;

	AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(AGS_SeekerAIController::InteractiveItemKey));
	if (!TargetActor)
		return EBTNodeResult::Failed;

	IGS_InteractableInterface* Interactable = Cast<IGS_InteractableInterface>(TargetActor);
	if (!Interactable || !Interactable->Execute_CanInteract(TargetActor, Pawn))
	{
		return EBTNodeResult::Failed;
	}

	CurrentTarget = TargetActor;

	// Check distance
	float Distance = FVector::Dist(Pawn->GetActorLocation(), TargetActor->GetActorLocation());
	if (Distance > AcceptableDistance)
	{
		// Should have moved closer before this task
		return EBTNodeResult::Failed;
	}

	// Start interaction
	TotalInteractionDuration = Interactable->Execute_GetInteractionDuration(TargetActor);
	InteractionTimer = 0.0f;

	Interactable->Execute_BeginInteract(TargetActor, Pawn);

	if (TotalInteractionDuration <= 0.0f)
	{
		Interactable->Execute_EndInteract(TargetActor, Pawn, true);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::InProgress;
}

void UGS_BTT_SeekerInteract::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (!CurrentTarget.IsValid())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	if (!Pawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	InteractionTimer += DeltaSeconds;

	// Check if still in range
	if (FVector::Dist(Pawn->GetActorLocation(), CurrentTarget->GetActorLocation()) > AcceptableDistance + 50.0f)
	{
		if (IGS_InteractableInterface* Interactable = Cast<IGS_InteractableInterface>(CurrentTarget.Get()))
		{
			Interactable->Execute_EndInteract(CurrentTarget.Get(), Pawn, false);
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (InteractionTimer >= TotalInteractionDuration)
	{
		if (IGS_InteractableInterface* Interactable = Cast<IGS_InteractableInterface>(CurrentTarget.Get()))
		{
			Interactable->Execute_EndInteract(CurrentTarget.Get(), Pawn, true);
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UGS_BTT_SeekerInteract::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (CurrentTarget.IsValid())
	{
		AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
		APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;

		if (Pawn)
		{
			if (IGS_InteractableInterface* Interactable = Cast<IGS_InteractableInterface>(CurrentTarget.Get()))
			{
				Interactable->Execute_EndInteract(CurrentTarget.Get(), Pawn, false);
			}
		}
	}
	return EBTNodeResult::Aborted;
}
