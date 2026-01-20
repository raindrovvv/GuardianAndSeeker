// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Character/Seeker/GS_ChooserInputObject.h"
#include "Kismet/KismetMathLibrary.h"

bool UGS_ChooserInputObject::ShouldTurnInPlace() const
{
	// Check turn-in-place criteria: Transitioning from moving to idle, or explicitly requested
	if (bMustTurnInPlace || (MovementState == EMovementState::Idle && LastMovementState == EMovementState::Moving))
	{
		const FRotator CharacterRot = CharacterTransform.GetRotation().Rotator();
		const FRotator RootRot = RootTransform.GetRotation().Rotator();

		const FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRot, RootRot);

		// If orientation delta exceeds 50 degrees, trigger a turning animation
		if (FMath::Abs(DeltaRot.Yaw) >= 50.0f)
		{
			return true;
		}
	}
	return false;
}

bool UGS_ChooserInputObject::IsMoving() const
{
	// Increased threshold to 10.0f to avoid jittering in idle.
	// Only current velocity is required to be "moving" for core state detection.
	return Velocity.Size2D() > 10.0f;
}

bool UGS_ChooserInputObject::IsStarting() const
{
	// Check if already in a "Pivots" context (to avoid double-starting) and ensure we are moving correctly
	const bool bAlreadyPivoting = CurrentDatabaseTags.Contains(TEXT("Pivots"));

	// Consider a start if future predicted speed is significantly higher than current speed
	const bool bHasFutureVelocity = FutureVelocity.Size2D() > 10.0f;
	if (FutureVelocity.Size2D() > (Velocity.Size2D() + 100.0f))
	{
		if (!bAlreadyPivoting && bHasFutureVelocity)
		{
			return true;
		}
	}
	return false;
}

bool UGS_ChooserInputObject::IsPivoting() const
{
	// Calculate rotation delta between current velocity and predicted future velocity
	const FRotator CurrentVelocityRot = Velocity.Rotation();
	const FRotator FutureVelocityRot = FutureVelocity.Rotation();

	const FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(CurrentVelocityRot, FutureVelocityRot);
	const float AbsYawDelta = FMath::Abs(DeltaRot.Yaw);

	// Thresholds depend on whether we are oriented to movement or strafing
	float PivotThreshold = 0.0f;
	switch (RotationMode)
	{
		case ERotationMode::OrientToMovement:
			PivotThreshold = 60.0f;
			break;
		case ERotationMode::Strafe:
			PivotThreshold = 40.0f;
			break;
	}

	return (AbsYawDelta > PivotThreshold);
}

bool UGS_ChooserInputObject::ShouldSpinTransition() const
{
	// Spins are specific types of high-rotation transitions used when already pivoting
	if (!CurrentDatabaseTags.Contains(TEXT("Pivots")))
	{
		return false;
	}

	const FRotator CharacterRot = CharacterTransform.GetRotation().Rotator();
	const FRotator RootRot = RootTransform.GetRotation().Rotator();
	const FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRot, RootRot);

	// Fast moving characters with high orientation divergence trigger a spin
	return (FMath::Abs(DeltaRot.Yaw) >= 130.0f && Speed2D >= 150.0f);
}
