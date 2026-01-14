// Fill out your copyright notice in the Description page of Project Settings.

#include "Animation/Notifies/GS_AN_ShieldAttack.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Weapon/Equipable/GS_WeaponShield.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UGS_AN_ShieldAttack::UGS_AN_ShieldAttack()
{
	// 노티파이 이름은 기본값 사용 (ShieldAttack)
}

void UGS_AN_ShieldAttack::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// 메시 컴포넌트의 소유자에서 찬 캐릭터 찾기
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	AGS_Chan* Chan = Cast<AGS_Chan>(Owner);
	if (!Chan)
	{
		return;
	}

	// 방패를 찾아서 공격 콜리전 활성화
	// Server RPC는 로컬에서 조종하는 캐릭터에서만 호출
	if (!Chan->IsLocallyControlled())
	{
		return;
	}

	for (int32 i = 0; i < 5; ++i)
	{
		if (AGS_WeaponShield* Shield = Cast<AGS_WeaponShield>(Chan->GetWeaponByIndex(i)))
		{
			// 서버에서 콜리전 활성화 및 자동 비활성화 타이머 처리
			// (ServerEnableAttackHit_Implementation 내부에서 0.3초 타이머 설정)
			Shield->ServerEnableAttackHit();
			break;
		}
	}
}
