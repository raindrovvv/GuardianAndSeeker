// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_CheckNextCombo.generated.h"

/**
 * @brief Animation notify that closes the window for accepting consecutive combo inputs.
 * Typically used at the end of an attack's active window to prevent late combo chaining.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Check Next Combo Notify"))
class GAS_API UGS_AN_CheckNextCombo : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_CheckNextCombo();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;
};
