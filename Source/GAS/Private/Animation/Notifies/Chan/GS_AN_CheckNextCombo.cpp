// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_AN_CheckNextCombo.h"
#include "Character/Player/Seeker/GS_Seeker.h"

UGS_AN_CheckNextCombo::UGS_AN_CheckNextCombo()
{
}

void UGS_AN_CheckNextCombo::Notify(USkeletalMeshComponent* MeshComp,
								   UAnimSequenceBase* Animation,
								   const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		if (AGS_Seeker* SeekerCharacter = Cast<AGS_Seeker>(MeshComp->GetOwner()))
		{
			// Close the window for accepting new combo inputs
			SeekerCharacter->ComboInputClose();
		}
	}
}
