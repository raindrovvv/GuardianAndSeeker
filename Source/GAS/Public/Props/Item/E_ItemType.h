// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "E_ItemType.generated.h"

/**
 * @brief Enumeration defining the types of items in the game.
 * Used for categorizing items for inventory, pickup, and usage systems.
 */
UENUM(BlueprintType, meta = (DisplayName = "Item Type"))
enum class EItemType : uint8
{
	/** Health restoration potion */
	HP_Potion UMETA(DisplayName = "HP Potion"),

	/** Maximum enum value for iteration */
	MAX UMETA(Hidden)
};