// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_AN_EndRollingSkill.h"

UGS_AN_EndRollingSkill::UGS_AN_EndRollingSkill()
{
}

void UGS_AN_EndRollingSkill::Notify(USkeletalMeshComponent* MeshComp,
									UAnimSequenceBase* Animation,
									const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// Implementation placeholder - logic managed via SkillComponent in Seeker classes
}
