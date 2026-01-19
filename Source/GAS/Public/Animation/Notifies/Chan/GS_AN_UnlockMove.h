// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_UnlockMove.generated.h"

/**
 * @brief Animation notify to unlock movement controls for the character.
 * Typically used at the end of an animation or when movement is allowed again.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Unlock Move Notify"))
class GAS_API UGS_AN_UnlockMove : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_UnlockMove();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;
};
