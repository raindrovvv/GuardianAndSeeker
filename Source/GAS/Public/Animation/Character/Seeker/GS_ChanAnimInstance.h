// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "GS_ChanAnimInstance.generated.h"

/**
 * @brief Animation instance specific to the Chan Seeker character.
 * Handles Chan-specific animation states and transitions.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Chan Anim Instance"))
class GAS_API UGS_ChanAnimInstance : public UGS_SeekerAnimInstance
{
	GENERATED_BODY()

public:
	UGS_ChanAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
