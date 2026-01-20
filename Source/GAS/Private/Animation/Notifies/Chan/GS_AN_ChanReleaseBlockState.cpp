// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_AN_ChanReleaseBlockState.h"

#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Animation/Character/Seeker/GS_ChooserInputObject.h"

void UGS_AN_ChanReleaseBlockState::Notify(USkeletalMeshComponent* MeshComp,
										  UAnimSequenceBase* Animation,
										  const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(MeshComp->GetOwner()))
	{
		if (UGS_SeekerAnimInstance* SeekerAnim = Cast<UGS_SeekerAnimInstance>(MeshComp->GetAnimInstance()))
		{
			// SeekerAnim->ChooserInputObject->bIsBlocking = false;
		}
		if (Seeker->HasAuthority())
		{
			Seeker->Multicast_SetMustTurnInPlace(false);
		}
	}
}
