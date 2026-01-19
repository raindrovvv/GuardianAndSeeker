// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GS_ANS_JumpAttack.generated.h"

/**
 * @brief Animation notify state used during jump attack animations for Chan.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Jump Attack Notify State"))
class GAS_API UGS_ANS_JumpAttack : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UGS_ANS_JumpAttack();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp,
							 UAnimSequenceBase* Animation,
							 float TotalDuration,
							 const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp,
						   UAnimSequenceBase* Animation,
						   const FAnimNotifyEventReference& EventReference) override;
};
