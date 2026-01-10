// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/BT/GS_BTD_SeekerCondition.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "AI/SeekerAI/GS_AISeeker.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Skill/GS_SkillComp.h"

UGS_BTD_SeekerCondition::UGS_BTD_SeekerCondition()
{
	NodeName = "Seeker Condition";

	// Default blackboard key setup
	TargetActorKey.SelectedKeyName = AGS_SeekerAIController::TargetEnemyKey;
}

bool UGS_BTD_SeekerCondition::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	switch (ConditionType)
	{
	case ESeekerConditionType::HasTargetEnemy:
		return CheckHasTargetEnemy(OwnerComp);
	case ESeekerConditionType::IsLowHealth:
		return CheckIsLowHealth(OwnerComp);
	case ESeekerConditionType::IsCriticalHealth:
		return CheckIsCriticalHealth(OwnerComp);
	case ESeekerConditionType::IsNearTrap:
		return CheckIsNearTrap(OwnerComp);
	case ESeekerConditionType::HasReachedGoal:
		return CheckHasReachedGoal(OwnerComp);
	case ESeekerConditionType::CanUseSkill:
		return CheckCanUseSkill(OwnerComp);
	case ESeekerConditionType::CanHeal:
		return CheckCanHeal(OwnerComp);
	case ESeekerConditionType::IsInCombat:
		return CheckIsInCombat(OwnerComp);
	case ESeekerConditionType::IsTargetInRange:
		return CheckIsTargetInRange(OwnerComp);
	case ESeekerConditionType::IsAlive:
		return CheckIsAlive(OwnerComp);
	case ESeekerConditionType::HasDownedAlly:
		return CheckHasDownedAlly(OwnerComp);
	case ESeekerConditionType::HasInteractiveItem:
		return CheckHasInteractiveItem(OwnerComp);
	default:
		return false;
	}
}

bool UGS_BTD_SeekerCondition::CheckHasTargetEnemy(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	UObject* Target = Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName);
	return Target != nullptr;
}

bool UGS_BTD_SeekerCondition::CheckIsLowHealth(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	float HealthPercent = Blackboard->GetValueAsFloat(AGS_SeekerAIController::CurrentHealthPercentKey);
	return HealthPercent <= CustomHealthThreshold;
}

bool UGS_BTD_SeekerCondition::CheckIsCriticalHealth(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	float HealthPercent = Blackboard->GetValueAsFloat(AGS_SeekerAIController::CurrentHealthPercentKey);
	return HealthPercent <= 0.15f; // Critical threshold
}

bool UGS_BTD_SeekerCondition::CheckIsNearTrap(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	UObject* Trap = Blackboard->GetValueAsObject(AGS_SeekerAIController::NearbyTrapKey);
	return Trap != nullptr;
}

bool UGS_BTD_SeekerCondition::CheckHasReachedGoal(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	return Blackboard->GetValueAsBool(AGS_SeekerAIController::HasReachedGoalKey);
}

bool UGS_BTD_SeekerCondition::CheckCanUseSkill(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return false;
	}

	APawn* Pawn = AIController->GetPawn();
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn);
	if (!Seeker)
	{
		return false;
	}

	// Check via AI Seeker wrapper
	if (AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(Pawn->GetOwner()))
	{
		return AISeeker->CanUseSkill(SkillIndex);
	}

	// Fallback: if seeker is alive and has skill comp, assume we can try
	if (UGS_SkillComp* SkillComp = Seeker->FindComponentByClass<UGS_SkillComp>())
	{
		return !Seeker->IsDead();
	}

	return false;
}

bool UGS_BTD_SeekerCondition::CheckCanHeal(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return false;
	}

	APawn* Pawn = AIController->GetPawn();

	// Check via AI Seeker wrapper
	if (AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(Pawn->GetOwner()))
	{
		return AISeeker->CanHeal();
	}

	// Fallback: check if seeker has heal potion skill
	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn))
	{
		if (UGS_SkillComp* SkillComp = Seeker->FindComponentByClass<UGS_SkillComp>())
		{
			// Assume seeker can heal if not dead
			return !Seeker->IsDead();
		}
	}

	return false;
}

bool UGS_BTD_SeekerCondition::CheckIsInCombat(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	return Blackboard->GetValueAsBool(AGS_SeekerAIController::IsInCombatKey);
}

bool UGS_BTD_SeekerCondition::CheckIsTargetInRange(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return false;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return false;
	}

	UObject* TargetObject = Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName);
	AActor* Target = Cast<AActor>(TargetObject);
	if (!Target)
	{
		return false;
	}

	float Distance = FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation());
	return Distance <= RangeThreshold;
}

bool UGS_BTD_SeekerCondition::CheckIsAlive(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return false;
	}

	APawn* Pawn = AIController->GetPawn();
	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn))
	{
		return !Seeker->IsDead();
	}

	return true; // Assume alive if not a Seeker
}

bool UGS_BTD_SeekerCondition::CheckHasDownedAlly(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	UObject* Ally = Blackboard->GetValueAsObject(AGS_SeekerAIController::DownedAllyKey);
	return Ally != nullptr;
}

bool UGS_BTD_SeekerCondition::CheckHasInteractiveItem(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	UObject* Item = Blackboard->GetValueAsObject(AGS_SeekerAIController::InteractiveItemKey);
	return Item != nullptr;
}

FString UGS_BTD_SeekerCondition::GetStaticDescription() const
{
	FString ConditionName;
	switch (ConditionType)
	{
	case ESeekerConditionType::HasTargetEnemy:
		ConditionName = TEXT("Has Target Enemy");
		break;
	case ESeekerConditionType::IsLowHealth:
		ConditionName = FString::Printf(TEXT("Health <= %.0f%%"), CustomHealthThreshold * 100.0f);
		break;
	case ESeekerConditionType::IsCriticalHealth:
		ConditionName = TEXT("Health <= 15%");
		break;
	case ESeekerConditionType::IsNearTrap:
		ConditionName = TEXT("Near Trap");
		break;
	case ESeekerConditionType::HasReachedGoal:
		ConditionName = TEXT("Reached Goal");
		break;
	case ESeekerConditionType::CanUseSkill:
		ConditionName = FString::Printf(TEXT("Can Use Skill %d"), SkillIndex);
		break;
	case ESeekerConditionType::CanHeal:
		ConditionName = TEXT("Can Heal");
		break;
	case ESeekerConditionType::IsInCombat:
		ConditionName = TEXT("In Combat");
		break;
	case ESeekerConditionType::IsTargetInRange:
		ConditionName = FString::Printf(TEXT("Target In Range (%.0f)"), RangeThreshold);
		break;
	case ESeekerConditionType::IsAlive:
		ConditionName = TEXT("Is Alive");
		break;
	case ESeekerConditionType::HasDownedAlly:
		ConditionName = TEXT("Has Downed Ally");
		break;
	case ESeekerConditionType::HasInteractiveItem:
		ConditionName = TEXT("Has Interactive Item");
		break;
	default:
		ConditionName = TEXT("Unknown");
		break;
	}

	return FString::Printf(TEXT("Condition: %s"), *ConditionName);
}
