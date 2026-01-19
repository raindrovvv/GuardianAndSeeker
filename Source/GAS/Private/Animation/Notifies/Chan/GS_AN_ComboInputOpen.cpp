// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_AN_ComboInputOpen.h"
#include "Character/Player/Seeker/GS_Seeker.h"

UGS_AN_ComboInputOpen::UGS_AN_ComboInputOpen()
{
}

void UGS_AN_ComboInputOpen::Notify(USkeletalMeshComponent* MeshComp,
								   UAnimSequenceBase* Animation,
								   const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		if (AGS_Seeker* SeekerCharacter = Cast<AGS_Seeker>(MeshComp->GetOwner()))
		{
			// Triggers the base seeker logic for opening the combo input window
			SeekerCharacter->ComboInputOpen();
		}
	}
}
