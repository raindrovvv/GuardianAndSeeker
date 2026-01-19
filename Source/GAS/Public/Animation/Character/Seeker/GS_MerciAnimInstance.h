// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "GS_MerciAnimInstance.generated.h"

/**
 * @brief Animation instance specific to the Merci Seeker character.
 * Handles Merci-specific animation states and transitions.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Merci Anim Instance"))
class GAS_API UGS_MerciAnimInstance : public UGS_SeekerAnimInstance
{
	GENERATED_BODY()

public:
	UGS_MerciAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
