// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_CheckStaminaEnd.generated.h"

/**
 * @brief Animation notify to check if the character has run out of stamina.
 * Forces the character into an idle state if stamina is depleted during certain actions.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Check Stamina End Notify"))
class GAS_API UGS_AN_CheckStaminaEnd : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_CheckStaminaEnd();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;
};
