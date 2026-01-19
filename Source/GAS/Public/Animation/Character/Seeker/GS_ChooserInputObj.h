// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Animation/Character/E_SeekerAnim.h"
#include "Character/E_Character.h"
#include "GS_ChooserInputObj.generated.h"

/**
 * @brief Input object used by the Unreal Engine Chooser system to determine motion matching animations.
 * Provides predicates for turning in place, pivoting, starting, and state-based transitions.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "GS Seeker Chooser Input"))
class GAS_API UGS_ChooserInputObj : public UObject
{
	GENERATED_BODY()

public:
	/** Returns true if the character orientation delta warrants a turn-in-place animation */
	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe = "true"), Category = "Movement")
	bool ShouldTurnInPlace() const;

	/** Returns true if the character has significant current and predicted future velocity */
	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe = "true"), Category = "Movement")
	bool IsMoving() const;

	/** Returns true if the character is starting to move from an idle state */
	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe = "true"), Category = "Movement")
	bool IsStarting() const;

	/** Returns true if the character's movement direction is changing significantly (pivoting) */
	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe = "true"), Category = "Movement")
	bool IsPivoting() const;

	/** Returns true if a spin transition is required based on rotation delta and speed */
	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe = "true"), Category = "Movement")
	bool ShouldSpinTransition() const;

	// Movement Vectors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Vectors")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Vectors")
	FVector FutureVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Context")
	FTransform CharacterTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Context")
	FTransform RootTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Context")
	float Speed2D = 0.0f;

	// State values
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	EMovementState MovementState = EMovementState::Idle;

	/** Movement state from the previous frame for transition detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	EMovementState LastMovementState = EMovementState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	EGait Gait = EGait::Walk;

	/** Gait from the previous frame for transition detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	EGait LastGait = EGait::Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	ERotationMode RotationMode = ERotationMode::OrientToMovement;

	/** External flag to force a turn-in-place logic check */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Control")
	bool bMustTurnInPlace = false;

	/** Tags currently active in the motion matching databases for context-aware logic */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Database")
	TArray<FName> CurrentDatabaseTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	ECharacterType CharacterType = ECharacterType::Seeker;

	/** Whether the character is currently in a blocking/defensive state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	bool bIsBlocking = false;
};
