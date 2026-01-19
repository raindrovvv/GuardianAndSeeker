// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_AN_StateReset.h"
#include "Character/Player/Seeker/GS_Seeker.h"

UGS_AN_StateReset::UGS_AN_StateReset()
{
}

void UGS_AN_StateReset::Notify(USkeletalMeshComponent* MeshComp,
							   UAnimSequenceBase* Animation,
							   const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	// Triggering a formal state reset on the seeker character
	if (AGS_Seeker* SeekerCharacter = Cast<AGS_Seeker>(MeshComp->GetOwner()))
	{
		// Resetting character state flags is typically a server-side responsibility
		if (SeekerCharacter->HasAuthority())
		{
			SeekerCharacter->StateReset();
		}
	}
}
