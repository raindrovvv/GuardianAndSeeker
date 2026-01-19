// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_AN_CheckStaminaEnd.h"
#include "Character/Player/Seeker/GS_Chan.h"

UGS_AN_CheckStaminaEnd::UGS_AN_CheckStaminaEnd()
{
}

void UGS_AN_CheckStaminaEnd::Notify(USkeletalMeshComponent* MeshComp,
									UAnimSequenceBase* Animation,
									const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		if (AGS_Chan* ChanCharacter = Cast<AGS_Chan>(MeshComp->GetOwner()))
		{
			// Check if stamina is fully depleted and reset state if necessary
			if (ChanCharacter->GetCurrentStaminaValue() <= 0.0f)
			{
				ChanCharacter->TransitionToIdle();
			}
		}
	}
}
