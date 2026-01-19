// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GS_AnimInstance.generated.h"

/**
 * @brief Base animation instance class for all characters in the game.
 * Provides common animation functionality and serves as the root class
 * for the animation class hierarchy.
 */
UCLASS(Abstract, BlueprintType, meta = (DisplayName = "GS Base Anim Instance"))
class GAS_API UGS_AnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UGS_AnimInstance();

protected:
	/** Override to perform custom initialization */
	virtual void NativeInitializeAnimation() override;
};
