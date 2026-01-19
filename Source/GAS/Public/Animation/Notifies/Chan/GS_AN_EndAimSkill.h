// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_EndAimSkill.generated.h"

/**
 * @brief Animation notify triggered at the end of the aiming skill animation.
 * Currently deprecated in favor of more robust skill component management.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS End Aim Skill Notify (Deprecated)"))
class GAS_API UGS_AN_EndAimSkill : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_EndAimSkill();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;
};
