// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_ANS_Aggro.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Character/Skill/GS_SkillComp.h"

UGS_ANS_Aggro::UGS_ANS_Aggro()
{
}

void UGS_ANS_Aggro::NotifyBegin(USkeletalMeshComponent* MeshComp,
								UAnimSequenceBase* Animation,
								float TotalDuration,
								const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
}

void UGS_ANS_Aggro::NotifyEnd(USkeletalMeshComponent* MeshComp,
							  UAnimSequenceBase* Animation,
							  const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		// Specifically handling Chan's aggro state termination
		if (AGS_Chan* ChanCharacter = Cast<AGS_Chan>(MeshComp->GetOwner()))
		{
			// Skill state cleanup should only be requested by the local owner to avoid redundancy
			if (ChanCharacter->IsLocallyControlled())
			{
				if (UGS_SkillComp* SkillComponent = ChanCharacter->GetSkillComp())
				{
					SkillComponent->Server_TrySkillAnimationEnd(ESkillSlot::Moving);
				}
			}
		}
	}
}
