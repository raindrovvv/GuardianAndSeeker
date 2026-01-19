// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Character/Seeker/GS_MerciAnimInstance.h"
#include "Animation/Character/Seeker/GS_ChooserInputObj.h"
#include "Character/E_Character.h"

UGS_MerciAnimInstance::UGS_MerciAnimInstance()
{
}

void UGS_MerciAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (ChooserInputObj)
	{
		ChooserInputObj->CharacterType = ECharacterType::Merci;
	}
}

void UGS_MerciAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
}
