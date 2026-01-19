// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Props/Item/GS_Item.h"
#include "GS_HP_Potion.generated.h"

/**
 * @brief Health Potion item that can be consumed by Seeker characters.
 * Handles pickup, consumption, and physics simulation when dropped.
 */
UCLASS(meta = (DisplayName = "HP Potion"))
class GAS_API AGS_HP_Potion : public AGS_Item
{
	GENERATED_BODY()

public:
	AGS_HP_Potion();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	/**
	 * @brief Releases the potion from its socket and enables physics.
	 * Called when the potion is discarded or the holder dies.
	 */
	UFUNCTION(BlueprintCallable, Category = "Item|Potion")
	void ReleaseFromHolder();

protected:
	/** Timer handle for delayed destruction after drop */
	FTimerHandle DestructionTimerHandle;

	/** Time in seconds before the dropped potion is destroyed */
	UPROPERTY(EditDefaultsOnly, Category = "Item|Potion", meta = (ClampMin = "1.0"))
	float DestructionDelay = 5.0f;

	/** Impulse force applied when potion is dropped */
	UPROPERTY(EditDefaultsOnly, Category = "Item|Potion")
	float DropImpulseStrength = 250.0f;

private:
	/** Applies physics impulse and torque when dropped */
	void ApplyDropPhysics();
};
