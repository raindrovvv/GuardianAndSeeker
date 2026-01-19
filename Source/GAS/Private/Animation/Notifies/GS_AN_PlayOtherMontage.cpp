// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/GS_AN_PlayOtherMontage.h"
#include "Character/Player/GS_Player.h"
#include "Animation/Character/GS_CharacterAnimInstance.h"

UGS_AN_PlayOtherMontage::UGS_AN_PlayOtherMontage()
{
}

void UGS_AN_PlayOtherMontage::Notify(USkeletalMeshComponent* MeshComp,
									 UAnimSequenceBase* Animation,
									 const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	AGS_Player* Player = Cast<AGS_Player>(MeshComp->GetOwner());
	if (!Player)
	{
		return;
	}

	// Playing a montage usually requires an AnimInstance check or direct call on the player
	if (TargetMontage)
	{
		// Montage playback for skills/synced actions is handled via multicast for visibility
		if (Player->HasAuthority())
		{
			Player->Multicast_PlaySkillMontage(TargetMontage);
		}
	}
}
