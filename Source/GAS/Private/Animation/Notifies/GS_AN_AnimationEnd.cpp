// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/GS_AN_AnimationEnd.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Skill/GS_SkillComp.h"

UGS_AN_AnimationEnd::UGS_AN_AnimationEnd()
{
}

void UGS_AN_AnimationEnd::Notify(USkeletalMeshComponent* MeshComp,
								 UAnimSequenceBase* Animation,
								 const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	AGS_Seeker* Seeker = Cast<AGS_Seeker>(MeshComp->GetOwner());
	if (!Seeker)
	{
		return;
	}

	// Skill animation end handling is a server-side responsibility for state consistency
	if (Seeker->HasAuthority())
	{
		if (UGS_SkillComp* SkillComponent = Seeker->GetSkillComp())
		{
			SkillComponent->TrySkillAnimationEnd(TargetSkillSlot);
		}
	}
}
