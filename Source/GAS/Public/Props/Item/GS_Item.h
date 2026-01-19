// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GS_Item.generated.h"

class UGS_ItemData;

/**
 * @brief Base class for all pickup items in the game.
 * Provides common functionality for item display, data management, and lifecycle.
 */
UCLASS(Abstract, BlueprintType)
class GAS_API AGS_Item : public AActor
{
	GENERATED_BODY()

public:
	AGS_Item();

protected:
	virtual void BeginPlay() override;

public:
	/**
	 * @brief Assigns item data to this item instance.
	 * @param InItemData The data asset containing item configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "Item")
	void AssignItemData(UGS_ItemData* InItemData);

	/**
	 * @brief Updates the visual mesh based on a named variant.
	 * @param VariantName The key in the MeshVariants map
	 */
	UFUNCTION(BlueprintCallable, Category = "Item|Visual")
	void ApplyMeshVariant(FName VariantName);

	/**
	 * @brief Gets the static mesh component for this item.
	 * @return The mesh component displaying this item
	 */
	UFUNCTION(BlueprintPure, Category = "Item|Visual")
	UStaticMeshComponent* GetVisualMesh() const;

	/**
	 * @brief Destroys this item with optional effects.
	 */
	UFUNCTION(BlueprintCallable, Category = "Item")
	void DestroyItem();

protected:
	/** Visual representation of this item */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Components")
	TObjectPtr<UStaticMeshComponent> VisualMeshComponent;

	/** Configuration data for this item */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Data")
	TObjectPtr<UGS_ItemData> ItemConfiguration;
};
