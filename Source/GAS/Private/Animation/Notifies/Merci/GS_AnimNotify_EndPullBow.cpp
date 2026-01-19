// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Merci/GS_AnimNotify_EndPullBow.h"
#include "Character/Player/Seeker/GS_Merci.h"

void UGS_AnimNotify_EndPullBow::Notify(USkeletalMeshComponent* MeshComp,
									   UAnimSequenceBase* Animation,
									   const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	// Signalling Merci character that the draw montage has completed
	if (AGS_Merci* MerciCharacter = Cast<AGS_Merci>(MeshComp->GetOwner()))
	{
		// This should be called on the client where the animation is playing
		MerciCharacter->OnDrawMontageEnded();
	}
}
