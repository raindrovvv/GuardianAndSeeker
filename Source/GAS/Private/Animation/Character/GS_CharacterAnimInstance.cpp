// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Character/GS_CharacterAnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"

UGS_CharacterAnimInstance::UGS_CharacterAnimInstance()
	: CurrentGroundSpeed(0.0f)
{
}

void UGS_CharacterAnimInstance::BindOwnerCharacter(AGS_Character* InCharacter)
{
	CachedOwnerCharacter = InCharacter;
}

void UGS_CharacterAnimInstance::BindMovementComponent(UCharacterMovementComponent* InMovement)
{
	CachedMovementComponent = InMovement;
}

void UGS_CharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
}

void UGS_CharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (CachedOwnerCharacter && CachedMovementComponent)
	{
		const FVector Velocity = CachedMovementComponent->Velocity;
		CurrentGroundSpeed = Velocity.Size2D();
	}
}
