// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Character/E_Character.h"
#include "GS_AISeeker.generated.h"

class AGS_Seeker;
class AGS_Ares;
class AGS_Chan;
class AGS_Merci;
class AGS_SeekerAIController;
class UGS_SkillComp;
class UGS_StatComp;

/**
 * Seeker Type for AI Control
 */
UENUM(BlueprintType)
enum class ESeekerAIType : uint8
{
	Ares UMETA(DisplayName = "Ares (Sword)"),
	Chan UMETA(DisplayName = "Chan (Axe & Shield)"),
	Merci UMETA(DisplayName = "Merci (Bow)")
};

/**
 * AI Seeker wrapper - Spawns and controls an AI-driven Seeker
 * Place this in the level and select the Seeker type in the editor
 */
UCLASS(Blueprintable, BlueprintType)
class GAS_API AGS_AISeeker : public AActor
{
	GENERATED_BODY()

public:
	AGS_AISeeker();

	// Seeker Type Selection (Editable in Editor)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Seeker|Setup", meta = (DisplayPriority = 1))
	ESeekerAIType SeekerType = ESeekerAIType::Ares;

	// Seeker Class References (Set in Blueprint or Data Asset)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Seeker|Setup")
	TSubclassOf<AGS_Ares> AresClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Seeker|Setup")
	TSubclassOf<AGS_Chan> ChanClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Seeker|Setup")
	TSubclassOf<AGS_Merci> MerciClass;

	// AI Controller Class
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Seeker|Setup")
	TSubclassOf<AGS_SeekerAIController> AIControllerClass;

	// AI Behavior Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Seeker|Behavior")
	float HealThreshold = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Seeker|Behavior")
	float AttackRange = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Seeker|Behavior")
	float TrapDetectionRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Seeker|Behavior")
	bool bAutoStartExploration = true;

	// Debug Visualization
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Seeker|Debug")
	bool bShowDebugInfo = false;

	// Getters
	UFUNCTION(BlueprintPure, Category = "AI Seeker")
	AGS_Seeker* GetControlledSeeker() const { return SpawnedSeeker; }

	UFUNCTION(BlueprintPure, Category = "AI Seeker")
	AGS_SeekerAIController* GetSeekerAIController() const;

	UFUNCTION(BlueprintPure, Category = "AI Seeker")
	UGS_SkillComp* GetSkillComp() const;

	UFUNCTION(BlueprintPure, Category = "AI Seeker")
	UGS_StatComp* GetStatComp() const;

	UFUNCTION(BlueprintPure, Category = "AI Seeker")
	ESeekerAIType GetSeekerType() const { return SeekerType; }

	UFUNCTION(BlueprintPure, Category = "AI Seeker")
	bool IsMeleeSeeker() const { return SeekerType == ESeekerAIType::Ares || SeekerType == ESeekerAIType::Chan; }

	UFUNCTION(BlueprintPure, Category = "AI Seeker")
	bool IsRangedSeeker() const { return SeekerType == ESeekerAIType::Merci; }

	// AI Control Functions
	UFUNCTION(BlueprintCallable, Category = "AI Seeker|Combat")
	void PerformAttack();

	UFUNCTION(BlueprintCallable, Category = "AI Seeker|Combat")
	void PerformSkill(int32 SkillIndex);

	UFUNCTION(BlueprintCallable, Category = "AI Seeker|Combat")
	void PerformHeal();

	UFUNCTION(BlueprintCallable, Category = "AI Seeker|Combat")
	void PerformRoll(FVector Direction);

	UFUNCTION(BlueprintCallable, Category = "AI Seeker|Combat")
	void StopAllActions();

	// Health & Status
	UFUNCTION(BlueprintPure, Category = "AI Seeker|Status")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "AI Seeker|Status")
	bool IsAlive() const;

	UFUNCTION(BlueprintPure, Category = "AI Seeker|Status")
	bool CanUseSkill(int32 SkillIndex) const;

	UFUNCTION(BlueprintPure, Category = "AI Seeker|Status")
	bool CanHeal() const;

	// Events
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAISeekerSpawned, AGS_Seeker*, Seeker);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAISeekerDeath);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAISeekerReachedGoal);

	UPROPERTY(BlueprintAssignable, Category = "AI Seeker|Events")
	FOnAISeekerSpawned OnSeekerSpawned;

	UPROPERTY(BlueprintAssignable, Category = "AI Seeker|Events")
	FOnAISeekerDeath OnSeekerDeath;

	UPROPERTY(BlueprintAssignable, Category = "AI Seeker|Events")
	FOnAISeekerReachedGoal OnReachedGoal;

	// Goal Handling
	UFUNCTION(BlueprintCallable, Category = "AI Seeker")
	void NotifyGoalReached();

	// Merci Arrow Control
	UFUNCTION(BlueprintCallable, Category = "AI Seeker|Combat")
	void SwitchToRandomSpecialArrow();

	UFUNCTION(BlueprintCallable, Category = "AI Seeker|Combat")
	void ResetToNormalArrow();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY()
	AGS_Seeker* SpawnedSeeker;

	UPROPERTY()
	AGS_SeekerAIController* SeekerController;

	void SpawnSeeker();
	void SetupAIController();
	TSubclassOf<AGS_Seeker> GetSeekerClassByType() const;

	UFUNCTION()
	void HandleSeekerDeath();

	// Merci Bow Holding Logic (Safety Fallback for AI)
	float DrawStartTime = 0.0f;
	const float MerciMaxHoldDuration = 1.5f;

	// Timed Skill Logic (Ares Dash, Chan Shield, Merci Fog)
	FTimerHandle AresDashTimer;
	FTimerHandle ChanShieldTimer;
	FTimerHandle MerciMovingSkillTimer;

	void ExecuteAresDash();
	void StopChanShield();
	void ExecuteMerciMovingSkill();

	// Debug
	void DrawDebugInfo() const;
};
