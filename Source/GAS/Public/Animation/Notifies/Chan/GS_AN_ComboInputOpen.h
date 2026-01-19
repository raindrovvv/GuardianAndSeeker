// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_ComboInputOpen.generated.h"

/**
 * @brief Animation notify that opens the window for accepting consecutive combo inputs during an attack.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Combo Input Open Notify"))
class GAS_API UGS_AN_ComboInputOpen : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_ComboInputOpen();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;
};
