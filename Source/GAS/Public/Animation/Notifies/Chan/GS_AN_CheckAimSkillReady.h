// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_CheckAimSkillReady.generated.h"

/**
 * @brief Animation notify for Chan to check if the aiming skill is ready to be transitioned into.
 * Updates the allowed skills mask to include the aiming skill.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Check Aim Skill Ready Notify"))
class GAS_API UGS_AN_CheckAimSkillReady : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_CheckAimSkillReady();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;
};
