// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/Notifies/GS_ANS_HitWeaponForChan.h"
#include "Character/GS_Character.h"
#include "Weapon/Equipable/GS_WeaponEquipable.h"

void UGS_ANS_HitWeaponForChan::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                           float TotalDuration)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);

	if (!MeshComp || !MeshComp->GetOwner())
		return;

	if (AGS_Character* Character = Cast<AGS_Character>(MeshComp->GetOwner()))
	{
		if (Character->HasAuthority())
		{
			// 첫 번째 무기를 소환된 ChildActor에서 가져와서 AGS_WeaponEquipable로 캐스팅
			if (AGS_WeaponEquipable* Weapon = Cast<AGS_WeaponEquipable>(Character->GetWeaponByIndex(0)))
			{
				Weapon->ServerEnableHit();
			}
		}
	}
}

void UGS_ANS_HitWeaponForChan::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::NotifyEnd(MeshComp, Animation);

	if (!MeshComp || !MeshComp->GetOwner())
		return;

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