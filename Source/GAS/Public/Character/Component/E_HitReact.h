// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "E_HitReact.generated.h"

/**
 * @brief Enumeration defining the types of hit reactions for characters.
 * Determines how a character responds to taking damage.
 */
UENUM(BlueprintType, meta = (DisplayName = "Hit Reaction Type"))
enum class EHitReactType : uint8
{
	/** Full interrupt - stops current action and plays hit animation */
	Interrupt UMETA(DisplayName = "Interrupt"),

	/** Additive - plays hit reaction on top of current animation */
	Additive UMETA(DisplayName = "Additive"),

	/** Damage only - applies damage without visual reaction */
	DamageOnly UMETA(DisplayName = "Damage Only"),

	/** Total number of hit reaction types */
	TypeNum UMETA(Hidden)
};