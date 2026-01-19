// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GS_AttackInterface.generated.h"

/**
 * @brief Interface for attack input handling.
 * Implement this interface to receive attack input events from the input system.
 */
UINTERFACE(MinimalAPI, BlueprintType, meta = (DisplayName = "Attack Interface"))
class UGS_AttackInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * @brief Attack interface implementation class.
 * Classes implementing this interface can respond to left-click attack input.
 */
class GAS_API IGS_AttackInterface
{
	GENERATED_BODY()

public:
	/**
	 * @brief Called when the left mouse button is pressed.
	 * Implement to handle attack initiation.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Input")
	void OnAttackInputPressed();

	/**
	 * @brief Called when the left mouse button is released.
	 * Implement to handle attack release or charged attack completion.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Input")
	void OnAttackInputReleased();
};
