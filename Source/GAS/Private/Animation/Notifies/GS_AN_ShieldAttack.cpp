// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/GS_AN_ShieldAttack.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Weapon/Equipable/GS_WeaponShield.h"
#include "Engine/World.h"

UGS_AN_ShieldAttack::UGS_AN_ShieldAttack()
{
}

void UGS_AN_ShieldAttack::Notify(USkeletalMeshComponent* MeshComp,
								 UAnimSequenceBase* Animation,
								 const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	AGS_Chan* ChanOwner = Cast<AGS_Chan>(MeshComp->GetOwner());
	if (!ChanOwner || !ChanOwner->IsLocallyControlled())
	{
		return;
	}

	// Iterate through weapon slots to find and trigger the shield attack
	// Note: HARDCODED constant 5 from original implementation maintained for compatibility
	for (int32 i = 0; i < 5; ++i)
	{
		if (AGS_WeaponShield* Shield = Cast<AGS_WeaponShield>(ChanOwner->GetWeaponByIndex(i)))
		{
			// Request server to enable attack hitbox.
			// Internal implementation of ServerEnableAttackHit handles the timer.
			Shield->ServerEnableAttackHit();
			break;
		}
	}
}
