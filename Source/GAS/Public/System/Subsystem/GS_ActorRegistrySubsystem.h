// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GS_ActorRegistrySubsystem.generated.h"

class AGS_Monster;
class AGS_Seeker;
class AGS_Guardian;
class AGS_RTSController;
class AGS_LavaTrap;
class AGS_TrapManager;
class AGS_AIGoalTrigger;
class UGS_CompassIndicatorComponent;
class AGS_TrapBase;

/**
 * WorldSubsystem for managing and quickly accessing key game actors
 * Replacing repeated GetAllActorsOfClass and TActorIterator calls
 */
UCLASS()
class GAS_API UGS_ActorRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void CleanupInvalidEntries();

	// --- Registration ---
	void RegisterMonster(AGS_Monster* Monster);
	void UnregisterMonster(AGS_Monster* Monster);

	void RegisterSeeker(AGS_Seeker* Seeker);
	void UnregisterSeeker(AGS_Seeker* Seeker);

	void RegisterGuardian(AGS_Guardian* Guardian);
	void UnregisterGuardian(AGS_Guardian* Guardian);

	void RegisterRTSController(AGS_RTSController* RTSController);
	void UnregisterRTSController(AGS_RTSController* RTSController);

	void RegisterLavaTrap(AGS_LavaTrap* LavaTrap);
	void UnregisterLavaTrap(AGS_LavaTrap* LavaTrap);

	void RegisterCompassIndicator(UGS_CompassIndicatorComponent* CompassIndicator);
	void UnregisterCompassIndicator(UGS_CompassIndicatorComponent* CompassIndicator);

	void RegisterTrapManager(AGS_TrapManager* TrapManager);
	void UnregisterTrapManager(AGS_TrapManager* TrapManager);

	void RegisterGoalTrigger(AGS_AIGoalTrigger* GoalTrigger);
	void UnregisterGoalTrigger(AGS_AIGoalTrigger* GoalTrigger);

	void RegisterTrap(AGS_TrapBase* Trap);
	void UnregisterTrap(AGS_TrapBase* Trap);
	const TArray<TWeakObjectPtr<AGS_TrapBase>>& GetTraps() const { return RegisteredTraps; }

	// --- Accessors ---
	const TArray<TWeakObjectPtr<AGS_Monster>>& GetMonsters() const { return RegisteredMonsters; }
	const TArray<TWeakObjectPtr<AGS_Seeker>>& GetSeekers() const { return RegisteredSeekers; }
	AGS_Guardian* GetGuardian() const { return RegisteredGuardian.Get(); }
	AGS_RTSController* GetRTSController() const { return RegisteredRTSController.Get(); }
	const TArray<TWeakObjectPtr<AGS_LavaTrap>>& GetLavaTraps() const { return RegisteredLavaTraps; }
	const TArray<TWeakObjectPtr<UGS_CompassIndicatorComponent>>& GetCompassIndicators() const { return RegisteredCompassIndicators; }
	AGS_TrapManager* GetTrapManager() const { return RegisteredTrapManager.Get(); }
	const TArray<TWeakObjectPtr<AGS_AIGoalTrigger>>& GetGoalTriggers() const { return RegisteredGoalTriggers; }

	/** Combines Monsters and Guardians for Merci's ultimate or similar logic */
	void GetAllHostileActors(TArray<AActor*>& OutActors) const;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<AGS_Monster>> RegisteredMonsters;

	UPROPERTY()
	TArray<TWeakObjectPtr<AGS_Seeker>> RegisteredSeekers;

	UPROPERTY()
	TWeakObjectPtr<AGS_Guardian> RegisteredGuardian;

	UPROPERTY()
	TWeakObjectPtr<AGS_RTSController> RegisteredRTSController;

	UPROPERTY()
	TArray<TWeakObjectPtr<AGS_LavaTrap>> RegisteredLavaTraps;

	UPROPERTY()
	TArray<TWeakObjectPtr<UGS_CompassIndicatorComponent>> RegisteredCompassIndicators;

	UPROPERTY()
	TWeakObjectPtr<AGS_TrapManager> RegisteredTrapManager;

	UPROPERTY()
	TArray<TWeakObjectPtr<AGS_AIGoalTrigger>> RegisteredGoalTriggers;

	UPROPERTY()
	TArray<TWeakObjectPtr<class AGS_TrapBase>> RegisteredTraps;

	FTimerHandle CleanupTimerHandle;
};
