// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_AN_TEST.h"
#include "Character/Player/Seeker/GS_Seeker.h"

UGS_AN_TEST::UGS_AN_TEST()
{
}

void UGS_AN_TEST::Notify(USkeletalMeshComponent* MeshComp,
						 UAnimSequenceBase* Animation,
						 const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		if (AGS_Seeker* SeekerCharacter = Cast<AGS_Seeker>(MeshComp->GetOwner()))
		{
			// Log network role and character name for debugging purposes
			UE_LOG(LogTemp,
				   Warning,
				   TEXT("[AnimNotify_TEST] Role: %s | Character: %s"),
				   *UEnum::GetValueAsString(SeekerCharacter->GetLocalRole()),
				   *SeekerCharacter->GetName());
		}
	}
}
