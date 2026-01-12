// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/BT/GS_BTT_SeekerCombat.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "AI/SeekerAI/GS_AISeeker.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Skill/GS_SkillComp.h"
#include "NavigationSystem.h"

UGS_BTT_SeekerCombat::UGS_BTT_SeekerCombat()
{
	NodeName = "Seeker Combat";
	bNotifyTick = true;
	bCreateNodeInstance = true; // Essential: Give each AI its own LastAttackTime variable

	// Reduced default cooldown for more aggressive AI
	// Note: Ranged characters (Merci) get 50% faster rate in combat logic
	AttackCooldown = 0.7f;
	SkillUsageChance = 0.25f; // 25% skill usage - more basic attacks, less spammy

	// Default blackboard key setup
	TargetActorKey.SelectedKeyName = AGS_SeekerAIController::TargetEnemyKey;
}

EBTNodeResult::Type UGS_BTT_SeekerCombat::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	// Get target
	UObject* TargetObject = Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName);
	AActor* TargetActor = Cast<AActor>(TargetObject);

	if (!TargetActor)
	{
		// No target - exit combat
		return EBTNodeResult::Failed;
	}

	// Check if target is dead
	if (AGS_Monster* Monster = Cast<AGS_Monster>(TargetActor))
	{
		if (Monster->IsDead())
		{
			AIController->ClearTargetEnemy();
			return EBTNodeResult::Succeeded;
		}
	}

	return EBTNodeResult::InProgress;
}

void UGS_BTT_SeekerCombat::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn);
	if (!Seeker || Seeker->IsDead())
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

	// Get target
	UObject* TargetObject = Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName);
	AActor* TargetActor = Cast<AActor>(TargetObject);

	if (!TargetActor)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Check if target is dead
	if (AGS_Monster* Monster = Cast<AGS_Monster>(TargetActor))
	{
		if (Monster->IsDead())
		{
			AIController->ClearTargetEnemy();
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
			return;
		}
	}

	float DistanceToTarget = AIController->GetDistanceToTarget(TargetActor);

	// Get AI Seeker wrapper early for type checking
	AGS_AISeeker* AISeeker = AIController->GetControlledSeeker();
	if (!AISeeker)
	{
		AActor* AIWrapper = AIController->GetOwner();
		AISeeker = Cast<AGS_AISeeker>(AIWrapper);
	}

	// Determine if ranged (for kiting logic)
	bool bIsRanged = (AISeeker != nullptr) ? AISeeker->IsRangedSeeker() : false;
	// Use AttackRange from controller (synced with Seeker type)
	float AttackRange = AIController->AttackRange;

	// Determine if we are currently mid-action (combo or drawing bow)
	bool bInAction = (!bIsRanged && Seeker->GetCurrentComboIndex() > 0) ||
	                 (bIsRanged && (Seeker->GetDrawState() || Seeker->GetAimState()));

	// Tolerance factor for range check (e.g., 20% extra)
	const float RangeTolerance = 1.2f;

	// Bypass range check if currently performing an action (e.g. drawing bow)
	if (bInAction || DistanceToTarget <= AttackRange * RangeTolerance)
	{
		// [Universal Evasion/Kiting Logic]
		// Ranged characters (Merci) maintain optimal distance
		// Melee characters (Ares/Chan) only reposition when health is low or surrounded
		float KitingThreshold = bIsRanged ? 0.4f : 0.25f; // Merci: 40%, Melee: 25%

		// Melee characters only kite when health is below 50%
		bool bShouldKite = bIsRanged || (Seeker->GetStatComp() && Seeker->GetStatComp()->GetCurrentHealth() / Seeker->GetStatComp()->GetMaxHealth() < 0.5f);

		if (bShouldKite && DistanceToTarget < AttackRange * KitingThreshold)
		{
			FVector AwayDirection = (Pawn->GetActorLocation() - TargetActor->GetActorLocation()).GetSafeNormal2D();

			// Smart Direction Finding (Avoid Walls)
			FVector BestDirection = AwayDirection;
			UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

			bool bFoundGoodSpot = false;
			if (NavSystem)
			{
				// Removed 135/-135 to prevent rolling towards the enemy when backed against a wall
				float Angles[] = {0.f, 45.f, -45.f, 90.f, -90.f};
				float TestDist = bIsRanged ? 400.f : 250.f;

				for (float Angle : Angles)
				{
					FVector TestDir = AwayDirection.RotateAngleAxis(Angle, FVector::UpVector);
					FVector TestPos = Pawn->GetActorLocation() + TestDir * TestDist;
					FNavLocation NavLocation;

					// Use a larger vertical extent for wall checks
					if (NavSystem->ProjectPointToNavigation(TestPos, NavLocation, FVector(200.f, 200.f, 200.f)))
					{
						BestDirection = TestDir;
						bFoundGoodSpot = true;
						break;
					}
				}
			}

			// Use Roll only in emergency situations
			// Merci: Only when VERY close (10% range) - emergency escape
			// Melee: Even more restrictive (8% range) - prefer to stand and fight
			float RollThreshold = bIsRanged ? 0.1f : 0.08f;
			if (AISeeker && DistanceToTarget < AttackRange * RollThreshold && AISeeker->CanUseSkill(static_cast<int32>(ESkillSlot::Rolling)))
			{
				AIController->ClearFocus(EAIFocusPriority::Gameplay);
				AISeeker->PerformRoll(BestDirection);
				LastAttackTime = GetWorld()->GetTimeSeconds();
				return;
			}
			else if (bShouldKite)
			{
				// Reposition/Kite by walking
				FVector EvadePos = Pawn->GetActorLocation() + BestDirection * (bIsRanged ? 400.f : 250.f);
				AIController->MoveToLocation(EvadePos, 100.0f);

				// Keep focus for ranged to allow shooting while kiting
				if (bIsRanged)
				{
					AIController->SetFocus(TargetActor);
				}
				else
				{
					AIController->ClearFocus(EAIFocusPriority::Gameplay);
				}

				// Ranged characters can still attack while kiting (no early return)
				// Only prevent attack if actively drawing bow (bInAction will be checked below)
			}
		}

		// Don't rotate or attack if playing a montage, UNLESS it's a special window
		if (Seeker->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying())
		{
			bool bIsComboWindowOpen = !bIsRanged && Seeker->CanAcceptComboInput;
			bool bIsMerciDrawing = bIsRanged && (Seeker->GetDrawState() || Seeker->GetAimState());

			if (!bIsComboWindowOpen && !bIsMerciDrawing)
			{
				return;
			}
		}

		AIController->StopMovement();

		// Face target only if in range or performing an action
		bool bHasLOS = AIController->LineOfSightTo(TargetActor);
		bool bShouldFocus = bHasLOS && (DistanceToTarget <= AttackRange * 1.5f || bInAction);

		if (bShouldFocus)
		{
			AIController->SetFocus(TargetActor);
		}
		else
		{
			AIController->ClearFocus(EAIFocusPriority::Gameplay);
		}

		// Attack logic
		float CurrentCombatTime = GetWorld()->GetTimeSeconds();
		float TimeSinceLastAttack = CurrentCombatTime - LastAttackTime;

		// Merci needs faster attack rate due to bow draw time
		float EffectiveCooldown = bIsRanged ? AttackCooldown * 0.5f : AttackCooldown;

		if (bInAction || (TimeSinceLastAttack >= EffectiveCooldown))
		{
			// 🟢 AI Skills & Attacks are now intelligently handled by AISeeker wrapper
			if (AISeeker)
			{
				// decision logic moved to PerformAttack and GetOptimalCombo
			}

			// Basic attack
			if (bHasLOS)
			{
				if (bHasLOS)
				{
					if (AISeeker)
					{
						// Merci: Before basic attack, decide on arrow type
						if (bIsRanged && !Seeker->GetDrawState() && !Seeker->GetAimState())
						{
							if (FMath::FRand() < 0.3f) // 30% chance to switch if ammo available
							{
								AISeeker->SwitchToRandomSpecialArrow();
							}
							else
							{
								AISeeker->ResetToNormalArrow();
							}
						}

						AISeeker->PerformAttack();
					}
					else
					{
						Seeker->Server_OnComboAttack();
					}
				}

				if (!bInAction)
				{
					LastAttackTime = CurrentCombatTime;
				}
			}
		}
	}
	else
	{
		if (AIController)
		{
			AIController->ClearFocus(EAIFocusPriority::Gameplay);
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (AIController->ShouldHeal())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	if (AIController->ShouldEvade())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}
}

EBTNodeResult::Type UGS_BTT_SeekerCombat::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner()))
	{
		AIController->StopMovement();
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}
	return Super::AbortTask(OwnerComp, NodeMemory);
}

FString UGS_BTT_SeekerCombat::GetStaticDescription() const
{
	return FString::Printf(TEXT("Combat\nMelee Range: %.0f\nRanged Range: %.0f\nSkill Chance: %.0f%%"),
	                       OptimalMeleeRange, OptimalRangedRange, SkillUsageChance * 100.0f);
}
