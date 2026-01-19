// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GS_Seeker.h"
#include "GS_Ares.generated.h"

class AGS_SwordAuraProjectile;
class UNiagaraSystem;
class UCurveFloat;

/**
 * @brief Ares character class - The heavy melee seeker.
 * Specializes in high-damage, large-scale sword attacks with built-in hit-stop and camera feedback.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Seeker Ares"))
class GAS_API AGS_Ares : public AGS_Seeker
{
	GENERATED_BODY()

public:
	AGS_Ares();

	// AActor / ACharacter interface
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount,
							 struct FDamageEvent const& DamageEvent,
							 class AController* EventInstigator,
							 AActor* DamageCauser) override;
	// ~AActor / ACharacter interface

	// AGS_Character interface
	virtual void ServerAttackMontage() override;
	virtual void MulticastPlayComboSection_Implementation(int32 ComboIndex) override;
	virtual void OnAttackHitSuccess(int32 ComboIndex, const FHitResult& HitResult) override;
	// ~AGS_Character interface

	/** Projectile class used by Ares' special sword aura skills */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ares|Skills")
	TSubclassOf<AGS_SwordAuraProjectile> AresProjectileClass;

	/** VFX played specifically on the final hit of the basic attack combo */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ares|VFX")
	TObjectPtr<UNiagaraSystem> FinisherHitVFX;

	/** Broadcasts hit effects (hit-stop, screenshake) to all relevant clients */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_HandleAttackHitEffects(int32 ComboIndex);

	/** Restores the camera zoom state after a dash skill move */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_RestoreDashCameraZoom();

	// Ares Moving Skill (Dash) Configuration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ares|Skills|Moving")
	float DashZoomDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ares|Skills|Moving")
	TObjectPtr<UCurveFloat> DashZoomCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ares|Skills|Moving|VFX")
	bool bEnableDashMotionBlur = false;

	UPROPERTY(EditAnywhere,
			  BlueprintReadOnly,
			  Category = "Ares|Skills|Moving|VFX",
			  meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DashMotionBlurIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ares|Skills|Moving|VFX")
	TObjectPtr<UCurveFloat> DashMotionBlurCurve;

	UPROPERTY(EditAnywhere,
			  BlueprintReadOnly,
			  Category = "Ares|Skills|Moving|VFX",
			  meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float DashMotionBlurExponent = 2.0f;

protected:
	virtual void BeginPlay() override;

private:
	/** Internal ID of the current playing sound for overlap management */
	int32 ActiveSoundID = -1;
};
