// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "GS_AresAnimInstance.generated.h"

/**
 * @brief Animation instance specific to the Ares Seeker character.
 * Handles Ares-specific animation states and transitions.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Ares Anim Instance"))
class GAS_API UGS_AresAnimInstance : public UGS_SeekerAnimInstance
{
	GENERATED_BODY()

public:
	UGS_AresAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
