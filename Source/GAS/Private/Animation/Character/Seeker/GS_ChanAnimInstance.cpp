// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Character/Seeker/GS_ChanAnimInstance.h"
#include "Animation/Character/Seeker/GS_ChooserInputObj.h"
#include "Character/E_Character.h"

UGS_ChanAnimInstance::UGS_ChanAnimInstance()
{
}

void UGS_ChanAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (ChooserInputObj)
	{
		ChooserInputObj->CharacterType = ECharacterType::Chan;
	}
}

void UGS_ChanAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
}
