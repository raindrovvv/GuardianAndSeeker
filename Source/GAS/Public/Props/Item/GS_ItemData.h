// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Props/Item/E_ItemType.h"
#include "GS_ItemData.generated.h"

/**
 * @brief Data container for item properties.
 * Stores configuration and state information for inventory items.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Item Data"))
class GAS_API UGS_ItemData : public UObject
{
	GENERATED_BODY()

public:
	/** The category/type of this item */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Config")
	EItemType ItemCategory;

	/** Display name for this item */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Config")
	FName DisplayName;

	/** Map of mesh variants for this item (e.g., full/empty states) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Visual")
	TMap<FName, UStaticMesh*> MeshVariants;

	/** Maximum stack size for this item */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Stack", meta = (ClampMin = "1"))
	int32 StackLimit = 1;

	/** Current quantity in stack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Stack", meta = (ClampMin = "0"))
	int32 CurrentQuantity = 0;
};
