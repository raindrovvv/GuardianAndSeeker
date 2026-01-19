// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_AN_UnlockMove.h"
#include "Character/Player/Seeker/GS_Chan.h"

UGS_AN_UnlockMove::UGS_AN_UnlockMove()
{
}

void UGS_AN_UnlockMove::Notify(USkeletalMeshComponent* MeshComp,
							   UAnimSequenceBase* Animation,
							   const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	// This notify is specifically for Chan but could be generalized in the future
	if (AGS_Chan* ChanCharacter = Cast<AGS_Chan>(MeshComp->GetOwner()))
	{
		// Unlocking movement is usually a server-side decision for sync consistency
		if (ChanCharacter->HasAuthority())
		{
			ChanCharacter->SetMoveControlValue(true, true);
		}
	}
}