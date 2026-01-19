// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "E_HitReact.h"
#include "GS_HitReactComp.generated.h"

/** Forward declaration for hit reaction end event */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHitReactEnd, UAnimMontage*, Montage, bool, bInterrupted);

/**
 * @brief Component responsible for handling hit reactions and stagger logic for characters.
 * Manages cooldowns, hit direction calculation, and state resets during hit montages.
 */
UCLASS(BlueprintType, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GAS_API UGS_HitReactComp : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_HitReactComp();

	/** Array of hit reaction montages, indexed by EHitReactType */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hit Reaction|Animations")
	TArray<TObjectPtr<UAnimMontage>> HitReactMontages;

	/** Triggers a hit reaction animation based on type and incoming direction */
	UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
	void PlayHitReact(EHitReactType ReactType, FVector HitDirection);

	/** Forcefully stops any currently playing hit reaction montage */
	UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
	void StopHitReact(UAnimMontage* TargetMontage);

	/** Calculates the relative direction (Front/Back/Left/Right) for a hit */
	UFUNCTION(BlueprintPure, Category = "Hit Reaction")
	FName CalculateHitDirection(FVector HitDirection);

	/** Triggered when a hit reaction montage finishes naturally or is interrupted */
	UPROPERTY(BlueprintAssignable, Category = "Hit Reaction|Events")
	FOnHitReactEnd OnHitReactEnd;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Internal handler for the animation end delegate */
	UFUNCTION()
	void HandleHitReactEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Internal delegate for montage completion */
	FOnMontageEnded HitReactEndDelegate;

	/** Timestamp of the last significant hit reaction to manage cooldowns */
	float LastHitReactTimestamp = 0.0f;

	/** Duration during which subsequent minor hits only apply damage without interrupting animations */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Reaction|Settings")
	float HitReactCooldownSeconds = 1.5f;
};