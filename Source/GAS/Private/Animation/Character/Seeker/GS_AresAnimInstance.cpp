// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Character/Seeker/GS_AresAnimInstance.h"
#include "Animation/Character/Seeker/GS_ChooserInputObj.h"
#include "Character/E_Character.h"

UGS_AresAnimInstance::UGS_AresAnimInstance()
{
}

void UGS_AresAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (ChooserInputObj)
	{
		// Ares uses Chan's movement logic for now as a placeholder/shared logic
		ChooserInputObj->CharacterType = ECharacterType::Chan;
	}
}

void UGS_AresAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
}
