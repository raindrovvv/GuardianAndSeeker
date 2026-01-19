// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GS_ANS_SetPotion.generated.h"

/**
 * @brief Animation notify state to manage the visual presence of a potion during a drinking animation.
 * Spawns the potion on begin, attaches it, and releases/destroys it on end.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Set Potion State Notify"))
class GAS_API UGS_ANS_SetPotion : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UGS_ANS_SetPotion();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp,
							 UAnimSequenceBase* Animation,
							 float TotalDuration,
							 const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp,
						   UAnimSequenceBase* Animation,
						   const FAnimNotifyEventReference& EventReference) override;
};
