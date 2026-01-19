// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "Engine/DamageEvents.h"
#include "Character/Component/E_HitReact.h"
#include "F_GS_DamageEvent.generated.h"

/**
 * @brief Custom damage event structure for the Guardian and Seeker project.
 * Extends Unreal's standard FDamageEvent to include project-specific hit reaction and critical hit data.
 */
USTRUCT(BlueprintType)
struct FGS_DamageEvent : public FDamageEvent
{
	GENERATED_BODY()

public:
	FGS_DamageEvent();

	/** The ID for this custom damage event type */
	static const int32 DamageEventType = 101;

	/** The type of hit reaction to trigger on the receiving character */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	EHitReactType HitReactType = EHitReactType::DamageOnly;

	/** Whether to suppress visual camera effects (shake, dither) for this damage event */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bSuppressCameraEffects = false;

	/** Whether this damage event represents a critical hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bIsCritical = false;

	/** Returns the type ID for this custom damage event */
	int32 GetType() const
	{
		return DamageEventType;
	}
};
