#include "Animation/Notifies/GS_ANS_CancelWindow.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Skill/GS_SkillComp.h"

void UGS_ANS_CancelWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(MeshComp->GetOwner()))
	{
		if (UGS_SkillComp* SkillComp = Seeker->GetSkillComp())
		{
			for (ESkillSlot Slot : CancellableSkills)
			{
				SkillComp->AddAllowedSkill(Slot);
			}
		}
	}
}

void UGS_ANS_CancelWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(MeshComp->GetOwner()))
	{
		if (UGS_SkillComp* SkillComp = Seeker->GetSkillComp())
		{
			for (ESkillSlot Slot : CancellableSkills)
			{
				SkillComp->RemoveAllowedSkill(Slot);
			}
		}
	}
}
