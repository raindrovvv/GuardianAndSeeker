// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/GS_ANS_HitWeaponForChan.h"
#include "Character/GS_Character.h"
#include "Weapon/Equipable/GS_WeaponEquipable.h"

UGS_ANS_HitWeaponForChan::UGS_ANS_HitWeaponForChan()
{
}

void UGS_ANS_HitWeaponForChan::NotifyBegin(USkeletalMeshComponent* MeshComp,
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
		if (Character->HasAuthority())
		{
			// Target Chan's primary weapon slot
			if (AGS_WeaponEquipable* Weapon = Cast<AGS_WeaponEquipable>(Character->GetWeaponByIndex(0)))
			{
				Weapon->ServerEnableHit();
			}
		}
	}
}

void UGS_ANS_HitWeaponForChan::NotifyEnd(USkeletalMeshComponent* MeshComp,
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