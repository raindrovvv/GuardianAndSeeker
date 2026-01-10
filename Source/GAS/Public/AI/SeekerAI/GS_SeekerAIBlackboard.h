// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BlackboardData.h"
#include "GS_SeekerAIBlackboard.generated.h"

/**
 * Custom Blackboard Data for Seeker AI
 * Contains all necessary keys for dungeon exploration AI
 */
UCLASS(BlueprintType, Blueprintable)
class GAS_API UGS_SeekerAIBlackboard : public UBlackboardData
{
	GENERATED_BODY()

public:
	UGS_SeekerAIBlackboard();

	// Static key names for easy access
	static const FName KEY_TargetEnemy;
	static const FName KEY_TargetLocation;
	static const FName KEY_GoalActor;
	static const FName KEY_CurrentHealthPercent;
	static const FName KEY_IsInCombat;
	static const FName KEY_ShouldHeal;
	static const FName KEY_ShouldEvade;
	static const FName KEY_NearbyTrap;
	static const FName KEY_ExplorationTarget;
	static const FName KEY_HasReachedGoal;
	static const FName KEY_LastKnownEnemyLocation;
	static const FName KEY_SelfActor;
};
