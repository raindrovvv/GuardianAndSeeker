// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Character/GS_Character.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Animation/Character/Seeker/GS_ChooserInputObject.h"

UGS_SeekerAnimInstance::UGS_SeekerAnimInstance()
{
	ChooserInputObject = CreateDefaultSubobject<UGS_ChooserInputObject>(TEXT("ChooserInputObject"));
}

void UGS_SeekerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// Cache the owner and movement component for performance and thread-safety
	if (AGS_Seeker* SeekerOwner = Cast<AGS_Seeker>(TryGetPawnOwner()))
	{
		CachedOwnerCharacter = SeekerOwner;
		CachedMovementComponent = SeekerOwner->GetCharacterMovement();

		// Initialize chooser data from character state
		if (ChooserInputObject)
		{
			ChooserInputObject->CharacterType = SeekerOwner->GetCharacterType();
			ChooserInputObject->Gait = SeekerOwner->GetSeekerGait();
			ChooserInputObject->LastGait = ChooserInputObject->Gait;
			LastGait = ChooserInputObject->Gait;
		}

		// Root bone offsetting logic for authority
		if (SeekerOwner->HasAuthority())
		{
			bUseOffsetRootBone = true;
		}
	}
}

void UGS_SeekerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Ensure components are cached (retry if necessary, though NativeInitialize handles most cases)
	if (!CachedOwnerCharacter)
	{
		CachedOwnerCharacter = Cast<AGS_Character>(TryGetPawnOwner());
		if (CachedOwnerCharacter)
		{
			CachedMovementComponent = CachedOwnerCharacter->GetCharacterMovement();
		}
	}

	if (CachedOwnerCharacter && CachedMovementComponent)
	{
		// Execute core animation update logic
		UpdateEssentialValue(DeltaSeconds);
		UpdateTrajectory();
		UpdateState();

		// Synchronize dying state for death animations
		if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(CachedOwnerCharacter))
		{
			bIsDying = Seeker->IsInDyingState();
		}

		// Cache values for thread-safe access in the AnimGraph
		CachedAOValue = Get_AOValue_Internal();
		bCachedEnableAO = Enable_AO_Internal();

		// Handle gait transition timing for smooth blend logic
		if (bIsTransitioningGait)
		{
			GaitTransitionTimer -= DeltaSeconds;
			if (GaitTransitionTimer <= 0.0f)
			{
				bIsTransitioningGait = false;
				GaitTransitionTimer = 0.0f;
			}
		}
	}
}

void UGS_SeekerAnimInstance::UpdateEssentialValue_Implementation(float DeltaSeconds)
{
	if (!ChooserInputObject || !CachedOwnerCharacter || !CachedMovementComponent)
	{
		return;
	}

	// Synchronize world transform
	ChooserInputObject->CharacterTransform = CachedOwnerCharacter->GetActorTransform();

	// Update movement vectors
	Acceleration = CachedMovementComponent->GetCurrentAcceleration();
	VelocityLastFrame = ChooserInputObject->Velocity;
	ChooserInputObject->Velocity = CachedMovementComponent->Velocity;
	ChooserInputObject->Speed2D = ChooserInputObject->Velocity.Size2D();

	// Calculate velocity acceleration for leaning effects
	const float SafeDeltaTime = FMath::Max(DeltaSeconds, SMALL_NUMBER);
	VelocityAcceleration = (ChooserInputObject->Velocity - VelocityLastFrame) / SafeDeltaTime;

	// Keep track of the last direction of movement for orienting idles
	if (ChooserInputObject->Speed2D > 5.0f)
	{
		LastNonZeroVelocity = ChooserInputObject->Velocity;
	}

	ChooserInputObject->bIsMoving = ChooserInputObject->IsMoving();
	bIsMoving = ChooserInputObject->bIsMoving;
}

void UGS_SeekerAnimInstance::UpdateState_Implementation()
{
	if (!ChooserInputObject || !CachedOwnerCharacter || !CachedMovementComponent)
	{
		return;
	}

	// Previous state caching for transition detection
	LastRotationMode = ChooserInputObject->RotationMode;

	// Determine rotation mode from movement component settings
	ChooserInputObject->RotationMode =
		CachedMovementComponent->bOrientRotationToMovement ? ERotationMode::OrientToMovement : ERotationMode::Strafe;

	// Handle movement state transitions and controller rotation dependencies
	ChooserInputObject->LastMovementState = ChooserInputObject->MovementState;

	if (ChooserInputObject->IsMoving())
	{
		// Strafe mode requires controller sync, OrientToMovement handles it internally/smoothly
		CachedOwnerCharacter->bUseControllerRotationYaw = (ChooserInputObject->RotationMode == ERotationMode::Strafe);
		ChooserInputObject->MovementState = EMovementState::Moving;
	}
	else
	{
		// Idle state rotation locking
		CachedOwnerCharacter->bUseControllerRotationYaw = CachedOwnerCharacter->GetIsLockedRotationToController();
		ChooserInputObject->MovementState = EMovementState::Idle;
	}

	// Manage gait (Walk/Run/Sprint) transitions
	if (AGS_Seeker* SeekerCharacter = Cast<AGS_Seeker>(CachedOwnerCharacter))
	{
		EGait CurrentGait = SeekerCharacter->GetSeekerGait();
		if (LastGait != CurrentGait)
		{
			ChooserInputObject->LastGait = LastGait;
			ChooserInputObject->Gait = CurrentGait;

			bIsTransitioningGait = true;
			GaitTransitionTimer = GaitTransitionDelay;

			LastGait = CurrentGait;
		}
	}

	bIsMoving = ChooserInputObject->IsMoving();
}

bool UGS_SeekerAnimInstance::GetMustTurnInPlace()
{
	return ChooserInputObject ? ChooserInputObject->bMustTurnInPlace : false;
}

void UGS_SeekerAnimInstance::SetMustTurnInPlace(bool MustTurn)
{
	if (ChooserInputObject)
	{
		ChooserInputObject->bMustTurnInPlace = MustTurn;
	}
}

float UGS_SeekerAnimInstance::GetOffsetRootTranslationHalfLife()
{
	if (!ChooserInputObject)
		return 0.0f;

	// Use specific half-lives based on movement state for stable root offsetting
	switch (ChooserInputObject->MovementState)
	{
		case EMovementState::Idle:
			return 0.15f;
		case EMovementState::Moving:
			return 0.40f;
		default:
			return 0.00f;
	}
}

FVector UGS_SeekerAnimInstance::CalculateRelativeAccelerationAmount()
{
	if (!CachedMovementComponent || !ChooserInputObject)
	{
		return FVector::ZeroVector;
	}

	const float MaxAcceleration = CachedMovementComponent->GetMaxAcceleration();
	const float MaxDeceleration = CachedMovementComponent->GetMaxBrakingDeceleration();

	if (MaxAcceleration > 0.0f && MaxDeceleration > 0.0f)
	{
		// Accelerating or Decelerating relative to current velocity
		const bool bIsAccelerating = FVector::DotProduct(Acceleration, ChooserInputObject->Velocity) > 0.0f;
		const float ClampValue = bIsAccelerating ? MaxAcceleration : MaxDeceleration;

		FVector ClampedAcc = VelocityAcceleration.GetClampedToMaxSize(ClampValue);
		return ChooserInputObject->CharacterTransform.GetRotation().UnrotateVector(ClampedAcc / ClampValue);
	}

	return FVector::ZeroVector;
}

float UGS_SeekerAnimInstance::Get_LeanAmount()
{
	if (ChooserInputObject && CachedMovementComponent)
	{
		// Scale leaning based on 2D speed
		const float LeanIntensity = FMath::GetMappedRangeValueClamped(
			FVector2D(200.0f, 500.0f), FVector2D(0.5f, 1.0f), ChooserInputObject->Speed2D);

		return CalculateRelativeAccelerationAmount().Y * LeanIntensity;
	}
	return 0.0f;
}

bool UGS_SeekerAnimInstance::EnableSteering()
{
	return ChooserInputObject && ChooserInputObject->MovementState == EMovementState::Moving;
}

FVector2D UGS_SeekerAnimInstance::Get_AOValue()
{
	return CachedAOValue;
}

FVector2D UGS_SeekerAnimInstance::Get_AOValue_Internal()
{
	FVector2D AO = FVector2D::ZeroVector;

	if (CachedOwnerCharacter && ChooserInputObject)
	{
		if (AController* Controller = CachedOwnerCharacter->GetController())
		{
			const FRotator ControllerRot = Controller->GetControlRotation();
			const FRotator RootRot = ChooserInputObject->RootTransform.Rotator();

			const FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(ControllerRot, RootRot);

			// Map Pitch to normalized range for AnimGraph
			const float PitchMin = -80.0f;
			const float PitchMax = 60.0f;

			AO.X = FMath::GetMappedRangeValueClamped(
				FVector2D(PitchMin, PitchMax), FVector2D(-100.0f, 100.0f), DeltaRot.Pitch);
			AO.Y = DeltaRot.Yaw;
		}
	}
	return AO;
}

bool UGS_SeekerAnimInstance::Enable_AO()
{
	return bCachedEnableAO;
}

bool UGS_SeekerAnimInstance::Enable_AO_Internal()
{
	// Only enable Aim Offset when strafing and within reasonable pitch limits
	if (!ChooserInputObject || ChooserInputObject->RotationMode != ERotationMode::Strafe)
	{
		return false;
	}

	return FMath::Abs(Get_AOValue_Internal().X) < 90.0f;
}

void UGS_SeekerAnimInstance::SetCurMontageSlot(ESeekerMontageSlot InputMontageSlot)
{
	// Convert enum to bitmask for networked replication
	CurMontageSlot = (1 << static_cast<uint8>(InputMontageSlot));
}

bool UGS_SeekerAnimInstance::IsMontageSlotActive(ESeekerMontageSlot InputMontageSlot)
{
	return (CurMontageSlot & (1 << static_cast<uint8>(InputMontageSlot))) != 0;
}

void UGS_SeekerAnimInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate current active montage slot for remote visibility
	DOREPLIFETIME(UGS_SeekerAnimInstance, CurMontageSlot);
}
