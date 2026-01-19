// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GS_ANS_Aggro.generated.h"

/**
 * @brief Animation notify state to manage aggro-related state for Seeker characters.
 * Triggers skill animation end for movement when the state finishes.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Aggro Notify State"))
class GAS_API UGS_ANS_Aggro : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UGS_ANS_Aggro();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp,
							 UAnimSequenceBase* Animation,
							 float TotalDuration,
							 const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp,
						   UAnimSequenceBase* Animation,
						   const FAnimNotifyEventReference& EventReference) override;
};
