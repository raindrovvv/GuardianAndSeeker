// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GS_ANS_ComboStartEnd.generated.h"

/**
 * @brief Animation notify state to toggle the 'IsAttacking' flag on the character.
 * Primarily used by Guardian (Drakhar) to manage combat state windows during combo animations.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Combo Window Notify State"))
class GAS_API UGS_ANS_ComboStartEnd : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UGS_ANS_ComboStartEnd();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp,
							 UAnimSequenceBase* Animation,
							 float TotalDuration,
							 const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp,
						   UAnimSequenceBase* Animation,
						   const FAnimNotifyEventReference& EventReference) override;
};
