// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/Character/Seeker/GS_AresAnimInstance.h"
#include "Animation/Character/Seeker/GS_ChooserInputObj.h"
#include "Character/E_Character.h"

void UGS_AresAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (ChooserInputObj)
	{
		ChooserInputObj->CharacterType = ECharacterType::Chan; // [임시] Chan으로 해야 올바른 이동 모션이 실행됨
	}
}

void UGS_AresAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
}
