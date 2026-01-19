// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/GS_ANS_HitWeapon.h"
#include "Character/GS_Character.h"
#include "Weapon/Equipable/GS_WeaponEquipable.h"

UGS_ANS_HitWeapon::UGS_ANS_HitWeapon()
{
}

void UGS_ANS_HitWeapon::NotifyBegin(USkeletalMeshComponent* MeshComp,
									UAnimSequenceBase* Animation,
									float TotalDuration,
									const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	if (AGS_Character* Character = Cast<AGS_Character>(MeshComp->GetOwner()))
	{
		// Weapon hitbox activation is only handled on the server
		if (Character->HasAuthority())
		{
			// Explicitly target the primary weapon slot (Index 0)
			if (AGS_WeaponEquipable* Weapon = Cast<AGS_WeaponEquipable>(Character->GetWeaponByIndex(0)))
			{
				Weapon->ServerEnableHit();
			}
		}
	}
}

void UGS_ANS_HitWeapon::NotifyEnd(USkeletalMeshComponent* MeshComp,
								  UAnimSequenceBase* Animation,
								  const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	if (AGS_Character* Character = Cast<AGS_Character>(MeshComp->GetOwner()))
	{
		if (Character->HasAuthority())
		{
			if (AGS_WeaponEquipable* Weapon = Cast<AGS_WeaponEquipable>(Character->GetWeaponByIndex(0)))
			{
				Weapon->ServerDisableHit();
			}
		}
	}
}
