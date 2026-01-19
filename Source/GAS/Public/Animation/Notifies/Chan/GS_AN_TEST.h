// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_TEST.generated.h"

/**
 * @brief Debugging animation notify to log ownership and role information.
 * Only intended for temporary testing and debugging of network synchronization.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Network Test Notify"))
class GAS_API UGS_AN_TEST : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_TEST();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;
};
