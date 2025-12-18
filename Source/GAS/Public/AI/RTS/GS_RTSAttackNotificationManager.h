// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GS_RTSAttackNotificationManager.generated.h"

class AGS_Monster;
class UGS_RTSAttackWarningWidget;
class UGS_MinimapWidget;
class USoundBase;

/**
 * RTS Attack Notification Manager Component
 * Manages attack notifications for RTS units including:
 * - HUD text notifications
 * - Minimap warning icons
 * - Audio alerts
 * - Last attack location tracking for camera jump
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAS_API UGS_RTSAttackNotificationManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_RTSAttackNotificationManager();

	/**
	 * Called when a unit is attacked
	 * Coordinates all notification systems (HUD, minimap, sound)
	 * @param Unit - The monster that was attacked
	 * @param Location - World location of the attack
	 */
	UFUNCTION()
	void OnUnitAttacked(AGS_Monster* Unit, FVector Location);

	/**
	 * Get the location of the last registered attack
	 * Used for camera jump functionality
	 * @return Last attack world location (FVector::ZeroVector if none)
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|Notification")
	FVector GetLastAttackLocation() const { return LastAttackLocation; }

	/**
	 * Set the HUD warning widget reference
	 * @param InWarningWidget - The warning widget to use for notifications
	 */
	void SetWarningWidget(UGS_RTSAttackWarningWidget* InWarningWidget);

	/**
	 * Set the minimap widget reference
	 * @param InMinimapWidget - The minimap widget to use for warning icons
	 */
	void SetMinimapWidget(UGS_MinimapWidget* InMinimapWidget);

protected:
	virtual void BeginPlay() override;

private:
	/** Last attack location in world space */
	FVector LastAttackLocation;

	/** Timestamp of last notification (for cooldown) */
	float LastNotificationTime;

	/** Cooldown between notifications to prevent spam */
	UPROPERTY(EditAnywhere, Category="RTS|Notification", meta=(ClampMin="0.5", ClampMax="5.0"))
	float NotificationCooldown = 5.0f;

	/** Reference to the HUD warning widget */
	UPROPERTY()
	TObjectPtr<UGS_RTSAttackWarningWidget> WarningWidget;

	/** Reference to the minimap widget */
	UPROPERTY()
	TObjectPtr<UGS_MinimapWidget> MinimapWidget;

	/** Audio for attack alert sound (Unreal native sound) */
	UPROPERTY(EditDefaultsOnly, Category="Sound")
	TObjectPtr<USoundBase> AttackAlertSound;

	/** Last sound play time (for sound cooldown) */
	float LastSoundPlayTime;

	/** Sound cooldown (separate from visual notification cooldown) */
	UPROPERTY(EditAnywhere, Category="Sound", meta=(ClampMin="0.5", ClampMax="5.0"))
	float SoundCooldown = 5.0f;
};
