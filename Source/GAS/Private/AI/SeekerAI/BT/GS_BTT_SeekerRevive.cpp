// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/BT/GS_BTT_SeekerRevive.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "AI/SeekerAI/GS_AISeeker.h"
#include "AIController.h"

UGS_BTT_SeekerRevive::UGS_BTT_SeekerRevive()
{
	NodeName = "Seeker Revive";
	bNotifyTick = true;
	DownedAllyKey.SelectedKeyName = AGS_SeekerAIController::DownedAllyKey;
}

EBTNodeResult::Type UGS_BTT_SeekerRevive::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
		return EBTNodeResult::Failed;

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
		return EBTNodeResult::Failed;

	AActor* AllyActor = Cast<AActor>(Blackboard->GetValueAsObject(DownedAllyKey.SelectedKeyName));
	if (!AllyActor)
		return EBTNodeResult::Failed;

	AGS_Seeker* DownedAlly = Cast<AGS_Seeker>(AllyActor);
	if (!DownedAlly || !DownedAlly->IsInDyingState())
	{
		Blackboard->ClearValue(DownedAllyKey.SelectedKeyName);
		return EBTNodeResult::Failed;
	}

	return EBTNodeResult::InProgress;
}

void UGS_BTT_SeekerRevive::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AActor* AllyActor = Cast<AActor>(Blackboard->GetValueAsObject(DownedAllyKey.SelectedKeyName));
	AGS_Seeker* DownedAlly = Cast<AGS_Seeker>(AllyActor);

	// If ally is no longer in dying state (revived or dead), finish task
	if (!DownedAlly || !DownedAlly->IsInDyingState() || DownedAlly->IsDead())
	{
		AIController->SetHoldingReviveKey(false);
		Blackboard->ClearValue(DownedAllyKey.SelectedKeyName);
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	float Distance = FVector::Dist(Pawn->GetActorLocation(), DownedAlly->GetActorLocation());

	if (Distance <= ReviveRange)
	{
		// Stop moving if close enough
		AIController->StopMovement();

		// Face the ally
		FVector Direction = DownedAlly->GetActorLocation() - Pawn->GetActorLocation();
		Direction.Z = 0;
		if (!Direction.IsNearlyZero())
		{
			Pawn->SetActorRotation(Direction.Rotation());
		}

		// Start/Continue revive process
		if (!DownedAlly->IsBeingRevived())
		{
			AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn);
			if (Seeker)
			{
				UE_LOG(LogTemp, Warning, TEXT("[AI Revive] %s starting revive on %s"), *Seeker->GetName(), *DownedAlly->GetName());
				DownedAlly->Server_StartRevive(Seeker);
			}
		}

		// Simulate holding the E key
		AIController->SetHoldingReviveKey(true);
		UE_LOG(LogTemp, Log, TEXT("[AI Revive] %s holding revive key, IsBeingRevived: %s, ReviveProgress: %.2f"),
		       *Pawn->GetName(),
		       DownedAlly->IsBeingRevived() ? TEXT("true") : TEXT("false"),
		       DownedAlly->GetReviveProgress());

		// Check if revive is completed (state changed)
		if (!DownedAlly->IsInDyingState())
		{
			AIController->SetHoldingReviveKey(false);
			Blackboard->ClearValue(DownedAllyKey.SelectedKeyName);
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
	}
	else
	{
		// Too far, move to ally
		AIController->SetHoldingReviveKey(false);

		if (DownedAlly->IsBeingRevived() && DownedAlly->CurrentReviver.Get() == Pawn)
		{
			DownedAlly->Server_CancelRevive();
		}

		AIController->MoveToActor(DownedAlly, ReviveRange * 0.8f);
	}
}

EBTNodeResult::Type UGS_BTT_SeekerRevive::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	if (AIController)
	{
		AIController->SetHoldingReviveKey(false);

		UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
		if (Blackboard)
		{
			AActor* AllyActor = Cast<AActor>(Blackboard->GetValueAsObject(DownedAllyKey.SelectedKeyName));
			AGS_Seeker* DownedAlly = Cast<AGS_Seeker>(AllyActor);
			if (DownedAlly && DownedAlly->IsBeingRevived() && DownedAlly->CurrentReviver.Get() == AIController->GetPawn())
			{
				DownedAlly->Server_CancelRevive();
			}
		}
	}
	return Super::AbortTask(OwnerComp, NodeMemory);
}

FString UGS_BTT_SeekerRevive::GetStaticDescription() const
{
	return FString::Printf(TEXT("Move to and Revive Downed Ally\nRange: %.0f"), ReviveRange);
}
