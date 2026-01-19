// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/GS_ANS_ComboStartEnd.h"
#include "Character/Player/Guardian/GS_Drakhar.h"

UGS_ANS_ComboStartEnd::UGS_ANS_ComboStartEnd()
{
}

void UGS_ANS_ComboStartEnd::NotifyBegin(USkeletalMeshComponent* MeshComp,
										UAnimSequenceBase* Animation,
										float TotalDuration,
										const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		// Toggle attacking flag for Drakhar
		if (AGS_Drakhar* Guardian = Cast<AGS_Drakhar>(MeshComp->GetOwner()))
		{
			Guardian->bIsAttacking = true;
		}
	}
}

void UGS_ANS_ComboStartEnd::NotifyEnd(USkeletalMeshComponent* MeshComp,
									  UAnimSequenceBase* Animation,
									  const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		// Reset attacking flag for Drakhar
		if (AGS_Drakhar* Guardian = Cast<AGS_Drakhar>(MeshComp->GetOwner()))
		{
			Guardian->bIsAttacking = false;
		}
	}
}
