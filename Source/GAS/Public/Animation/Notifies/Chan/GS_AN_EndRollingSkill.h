// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_EndRollingSkill.generated.h"

/**
 * @brief Animation notify triggered at the end of the rolling skill animation.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS End Rolling Skill Notify"))
class GAS_API UGS_AN_EndRollingSkill : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_EndRollingSkill();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;
};
