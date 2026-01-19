// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_StateReset.generated.h"

/**
 * @brief Animation notify to reset the internal state of the seeker character.
 * Used at the end of complex animations to ensure all state flags are back to default.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Character State Reset Notify"))
class GAS_API UGS_AN_StateReset : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_StateReset();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;
};
