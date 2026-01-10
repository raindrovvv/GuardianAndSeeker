// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/GS_SeekerAIBlackboard.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"

const FName UGS_SeekerAIBlackboard::KEY_TargetEnemy = TEXT("TargetEnemy");
const FName UGS_SeekerAIBlackboard::KEY_TargetLocation = TEXT("TargetLocation");
const FName UGS_SeekerAIBlackboard::KEY_GoalActor = TEXT("GoalActor");
const FName UGS_SeekerAIBlackboard::KEY_CurrentHealthPercent = TEXT("CurrentHealthPercent");
const FName UGS_SeekerAIBlackboard::KEY_IsInCombat = TEXT("IsInCombat");
const FName UGS_SeekerAIBlackboard::KEY_ShouldHeal = TEXT("ShouldHeal");
const FName UGS_SeekerAIBlackboard::KEY_ShouldEvade = TEXT("ShouldEvade");
const FName UGS_SeekerAIBlackboard::KEY_NearbyTrap = TEXT("NearbyTrap");
const FName UGS_SeekerAIBlackboard::KEY_ExplorationTarget = TEXT("ExplorationTarget");
const FName UGS_SeekerAIBlackboard::KEY_HasReachedGoal = TEXT("HasReachedGoal");
const FName UGS_SeekerAIBlackboard::KEY_LastKnownEnemyLocation = TEXT("LastKnownEnemyLocation");
const FName UGS_SeekerAIBlackboard::KEY_SelfActor = TEXT("SelfActor");

UGS_SeekerAIBlackboard::UGS_SeekerAIBlackboard()
{
	// Note: Keys are typically added via Blueprint or in PostInitProperties
	// This constructor sets up default values
	
	// The actual key setup should be done in the editor or via a factory
	// Here we provide documentation of the expected keys:
	/*
	Expected Blackboard Keys:
	
	1. TargetEnemy (Object) - Current enemy target (AGS_Monster)
	2. TargetLocation (Vector) - Target location for movement
	3. GoalActor (Object) - The goal trigger actor (AGS_AIGoalTrigger)
	4. CurrentHealthPercent (Float) - Current HP percentage (0.0-1.0)
	5. IsInCombat (Bool) - Whether currently in combat
	6. ShouldHeal (Bool) - Whether HP is low enough to heal
	7. ShouldEvade (Bool) - Whether there's a nearby trap to evade
	8. NearbyTrap (Object) - Nearest detected trap (AGS_TrapBase)
	9. ExplorationTarget (Vector) - Current exploration destination
	10. HasReachedGoal (Bool) - Whether the goal has been reached
	11. LastKnownEnemyLocation (Vector) - Last known position of lost enemy
	12. SelfActor (Object) - Reference to self (for EQS queries)
	*/
}
