// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_EndCombo.generated.h"

/**
 * @brief Animation notify that formally ends a combo sequence.
 * Resets montage slots, combo indices, and gait settings.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS End Combo Notify"))
class GAS_API UGS_AN_EndCombo : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_EndCombo();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;
};
