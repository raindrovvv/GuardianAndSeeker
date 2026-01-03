// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/Notifies/GS_ANS_HitWeaponForChan.h"
#include "Character/GS_Character.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Weapon/Equipable/GS_WeaponAxe.h"
#include "Weapon/Equipable/GS_WeaponSword.h"

void UGS_ANS_HitWeaponForChan::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                           float TotalDuration)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);

	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(MeshComp->GetOwner()))
	{
		if (!Seeker->HasAuthority())
			return;

		ECharacterType CharacterType = Seeker->GetCharacterType();
		if (CharacterType == ECharacterType::Chan)
		{
			if (AGS_WeaponAxe* Weapon = Cast<AGS_WeaponAxe>(Seeker->GetWeaponByIndex(0)))
			{
				// 재활용되는 노티파이를 위해 히트 목록 강제 초기화 후 활성화
				Weapon->ServerEnableHit();
			}
		}
		else if (CharacterType == ECharacterType::Ares)
		{
			if (AGS_WeaponSword* Weapon = Cast<AGS_WeaponSword>(Seeker->GetWeaponByIndex(0)))
			{
				// 아레스 3타처럼 연달아 휘두르는 경우를 위해 강제 초기화 보강
				Weapon->ServerEnableHit();
			}
		}
	}
}

void UGS_ANS_HitWeaponForChan::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::NotifyEnd(MeshComp, Animation);

	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(MeshComp->GetOwner()))
	{
		ECharacterType CharacterType = Seeker->GetCharacterType();
		if (CharacterType == ECharacterType::Chan && Seeker->HasAuthority())
		{
			if (AGS_WeaponAxe* Weapon = Cast<AGS_WeaponAxe>(Seeker->GetWeaponByIndex(0)))
			{
				Weapon->ServerDisableHit();
			}
		}
		else if (CharacterType == ECharacterType::Ares && Seeker->HasAuthority())
		{
			if (AGS_WeaponSword* Weapon = Cast<AGS_WeaponSword>(Seeker->GetWeaponByIndex(0)))
			{
				Weapon->ServerDisableHit();
			}
		}
	}
}