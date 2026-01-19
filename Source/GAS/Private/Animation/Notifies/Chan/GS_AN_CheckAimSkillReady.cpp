// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_AN_CheckAimSkillReady.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Character/Skill/GS_SkillComp.h"

UGS_AN_CheckAimSkillReady::UGS_AN_CheckAimSkillReady()
{
}

void UGS_AN_CheckAimSkillReady::Notify(USkeletalMeshComponent* MeshComp,
									   UAnimSequenceBase* Animation,
									   const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		if (AGS_Chan* ChanCharacter = Cast<AGS_Chan>(MeshComp->GetOwner()))
		{
			// Explicitly allow the aiming skill in the bitmask
			uint8 AllowedSkillsBitmask = 0;
			AllowedSkillsBitmask |= (1 << static_cast<int32>(ESkillSlot::Aiming));

			if (UGS_SkillComp* SkillComponent = ChanCharacter->GetSkillComp())
			{
				SkillComponent->SetCurAllowedSkillsMask(AllowedSkillsBitmask);
			}
		}
	}
}
