// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_AN_EndCombo.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"

UGS_AN_EndCombo::UGS_AN_EndCombo()
{
}

void UGS_AN_EndCombo::Notify(USkeletalMeshComponent* MeshComp,
							 UAnimSequenceBase* Animation,
							 const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	// Signalling the end of a combo sequence to the seeker character
	if (AGS_Seeker* SeekerCharacter = Cast<AGS_Seeker>(MeshComp->GetOwner()))
	{
		// Combo state cleanup must be synchronized on the server
		if (SeekerCharacter->HasAuthority())
		{
			// Reset active montage slot
			SeekerCharacter->Multicast_SetMontageSlot(ESeekerMontageSlot::None);

			// Clear skill permissions and reset movement state
			if (UGS_SkillComp* SkillComponent = SeekerCharacter->GetSkillComp())
			{
				SkillComponent->ResetAllowedSkillsMask();
			}

			// Restore character defaults for movement and combo tracking
			SeekerCharacter->CanChangeSeekerGait = true;
			SeekerCharacter->CurrentComboIndex = 0;

			// Re-enable combo input processing for the next sequence
			SeekerCharacter->ComboInputOpen();
		}
	}
}