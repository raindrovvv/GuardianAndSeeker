// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "GS_Chan.generated.h"

class AGS_WeaponShield;
class AGS_WeaponAxe;
class UGS_ChanAimingSkillBar;
class UNiagaraSystem;
class UCapsuleComponent;

/** Delegate for stamina depletion events */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStaminaDepleted, bool, bByDamage);

/**
 * @brief Chan character class - The defensive tank seeker.
 * Specializes in axe-and-shield combat, stamina-based blocking, and area-of-effect ultimate skills.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Seeker Chan"))
class GAS_API AGS_Chan : public AGS_Seeker
{
	GENERATED_BODY()

public:
	AGS_Chan();

	// AActor / ACharacter interface
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount,
							 struct FDamageEvent const& DamageEvent,
							 class AController* EventInstigator,
							 AActor* DamageCauser) override;
	// ~AActor / ACharacter interface

	// AGS_Character interface
	virtual void MulticastPlayComboSection_Implementation(int32 ComboIndex) override;
	virtual void OnAttackHitSuccess(int32 ComboIndex, const FHitResult& HitResult) override;
	// ~AGS_Character interface

	/** Triggers jump attack specific state changes */
	void HandleJumpAttackSkillStart();
	void HandleJumpAttackSkillEnd();

	/** Forces the character back to an idle state and restores control */
	void TransitionToIdle();

	/** VFX played specifically on the final hit of Chan's basic combo */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chan|VFX")
	TObjectPtr<UNiagaraSystem> FinisherHitVFX;

	/** Debug tool to draw the radius of ground slam skills */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_DrawSkillRange(FVector Center, float Radius, FColor Color, float Duration);

	/** Synchronizes hit-stop and camera feedback for combos */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_HandleAttackHitEffects(int32 ComboIndex);

	/** Capsule used to detect targets during critical skill phases */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chan|Collision")
	TObjectPtr<UCapsuleComponent> UltimateCollision;

	/** Internal handler for ultimate skill collision events */
	UFUNCTION()
	void HandleUltimateOverlap(UPrimitiveComponent* OverlappedComp,
							   AActor* OtherActor,
							   UPrimitiveComponent* OtherComp,
							   int32 OtherBodyIndex,
							   bool bFromSweep,
							   const FHitResult& SweepResult);

	/** UI Synchronization for the stamina-based aiming/blocking bar */
	void SetChanAimingSkillBarWidget(UGS_ChanAimingSkillBar* Widget)
	{
		LinkedSkillBarWidget = Widget;
	}

	UFUNCTION(Client, Reliable)
	void Client_UpdateSkillBarProgress(float NormalizedValue);

	UFUNCTION(Client, Reliable)
	void Client_UpdateSkillBarDamageFlash(float NormalizedValue);

	UFUNCTION(Client, Reliable)
	void Client_SetSkillBarVisibility(bool bIsVisible);

	// Stamina Management
	UPROPERTY(BlueprintAssignable, Category = "Chan|Events")
	FOnStaminaDepleted OnStaminaDepleted;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Chan|Stats")
	float MaxStamina = 100.0f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Chan|Stats")
	float CurrentStamina = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chan|Stats")
	float StaminaDrainPerTick = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chan|Stats")
	float StaminaRegenPerTick = 1.0f;

	UFUNCTION(BlueprintPure, Category = "Chan|Stats")
	float GetCurrentStaminaValue() const
	{
		return CurrentStamina;
	}

	void RestoreStaminaFully();
	void AdjustStamina(float Delta, bool bTriggeredByDamage = false);
	void ProcessStaminaDrain();
	void ProcessStaminaRegen();

	// Defense / Blocking
	UPROPERTY(ReplicatedUsing = OnRep_IsDefending, BlueprintReadOnly, Category = "Chan|State")
	bool bIsDefending = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chan|Stats")
	float DefenseDamageMitigation = 0.7f;

	UFUNCTION(BlueprintCallable, Category = "Chan|Actions")
	void SetDefending(bool bEnabled);

	virtual bool IsDefending() const override
	{
		return bIsDefending;
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Callback for replicated defense state */
	UFUNCTION()
	void OnRep_IsDefending();

	/** Validates if an incoming hit direction is within the shield's block arc */
	bool ValidateBlockDetection(const FVector& ImpactLocation) const;

private:
	/** Linked UI widget for stamina visualization */
	UPROPERTY()
	TObjectPtr<UGS_ChanAimingSkillBar> LinkedSkillBarWidget;

	/** Handler for periodic stamina updates */
	FTimerHandle StaminaCycleTimerHandle;

	/** Cached health for damage calculations */
	float BaseMaxHealth = 100.0f;
};