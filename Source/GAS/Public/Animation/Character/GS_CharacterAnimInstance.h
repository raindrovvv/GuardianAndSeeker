// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/GS_AnimInstance.h"
#include "GS_CharacterAnimInstance.generated.h"

class UCharacterMovementComponent;
class AGS_Character;

/**
 * @brief Animation instance for all playable characters.
 * Provides character-specific animation state and movement data binding.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Character Anim Instance"))
class GAS_API UGS_CharacterAnimInstance : public UGS_AnimInstance
{
	GENERATED_BODY()

public:
	UGS_CharacterAnimInstance();

	/**
	 * @brief Binds the owner character reference for animation logic.
	 * @param InCharacter The character that owns this animation instance
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|Setup")
	void BindOwnerCharacter(AGS_Character* InCharacter);

	/**
	 * @brief Binds the movement component for velocity and state queries.
	 * @param InMovement The character movement component to bind
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|Setup")
	void BindMovementComponent(UCharacterMovementComponent* InMovement);

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/** Reference to the owning character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation|References")
	TObjectPtr<AGS_Character> CachedOwnerCharacter;

	/** Reference to the character's movement component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation|References")
	TObjectPtr<UCharacterMovementComponent> CachedMovementComponent;

	/** Current ground movement speed (calculated each frame) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation|Movement")
	float CurrentGroundSpeed;
};
