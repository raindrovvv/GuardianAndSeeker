// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GS_ActorRegistrySubsystem.generated.h"

class AGS_Monster;
class AGS_Seeker;
class AGS_Guardian;

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

	// --- Accessors ---
	const TArray<TWeakObjectPtr<AGS_Monster>>& GetMonsters() const { return RegisteredMonsters; }
	const TArray<TWeakObjectPtr<AGS_Seeker>>& GetSeekers() const { return RegisteredSeekers; }
	AGS_Guardian* GetGuardian() const { return RegisteredGuardian.Get(); }

	/** Combines Monsters and Guardians for Merci's ultimate or similar logic */
	void GetAllHostileActors(TArray<AActor*>& OutActors) const;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<AGS_Monster>> RegisteredMonsters;

	UPROPERTY()
	TArray<TWeakObjectPtr<AGS_Seeker>> RegisteredSeekers;

	UPROPERTY()
	TWeakObjectPtr<AGS_Guardian> RegisteredGuardian;

	FTimerHandle CleanupTimerHandle;
};
