// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AnimNotify_EndPullBow.generated.h"

/**
 * @brief Animation notify triggered when the bow-pulling animation sequence ends.
 * Signals the character (Merci) to finish the draw montage and potentially fire an arrow.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS End Pull Bow Notify"))
class GAS_API UGS_AnimNotify_EndPullBow : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;
};
