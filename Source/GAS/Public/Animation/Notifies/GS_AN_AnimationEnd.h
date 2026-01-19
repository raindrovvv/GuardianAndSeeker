// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Character/Skill/ESkill.h"
#include "GS_AN_AnimationEnd.generated.h"

/**
 * @brief Animation notify that signals the end of a skill-related animation.
 * Used to clean up skill state and potentially trigger follow-up logic in the skill component.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Animation End Notify"))
class GAS_API UGS_AN_AnimationEnd : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_AnimationEnd();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;

	/** The skill slot associated with this animation end event */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	ESkillSlot TargetSkillSlot = ESkillSlot::None;
};
