// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GS_CharacterAnimInstance.h"
#include "E_SeekerAnim.h"
#include "GS_SeekerAnimInstance.generated.h"

class UChooserTable;
class UGS_ChooserInputObj;
struct UPoseSearchDatabase;

UENUM(BlueprintType)
enum class ESeekerMontageSlot : uint8
{
	None UMETA(DisplayName = "None"),
	FullBody UMETA(DisplayName = "Full Body"),
	UpperBody UMETA(DisplayName = "Upper Body"),
	End UMETA(DisplayName = "End"),
};

UCLASS()
class GAS_API UGS_SeekerAnimInstance : public UGS_CharacterAnimInstance
{
	GENERATED_BODY()
public:
	UGS_SeekerAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// Anim Update
	UFUNCTION(BlueprintNativeEvent, Category = "Update")
	void UpdateEssentialValue(float DeltaSeconds);
	UFUNCTION(BlueprintImplementableEvent, Category = "Update")
	void UpdateTrajectory();
	UFUNCTION(BlueprintNativeEvent, Category = "Update")
	void UpdateState();

	UFUNCTION()
	bool GetMustTurnInPlace();

	UFUNCTION()
	void SetMustTurnInPlace(bool MustTurn);

	/** Gait 전환 중인지 확인 (블루프린트에서 사용 가능) */
	UFUNCTION(BlueprintPure, Category = "Animation|State")
	bool IsTransitioningGait() const
	{
		return bIsTransitioningGait;
	}

	// Offset Root Bone
	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe = "true"), Category = "OffsetRootBone")
	float GetOffsetRootTranslationHalfLife();

	// Lean
	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe = "true"), Category = "Lean")
	FVector CalculateRelativeAccelerationAmount();

	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe = "true"), Category = "Lean")
	float Get_LeanAmount();

	// Steering
	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe = "true"), Category = "Steering")
	bool EnableSteering();

	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe = "true"), Category = "AimOffset")
	FVector2D Get_AOValue();

	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe = "true"), Category = "AimOffset")
	bool Enable_AO();

	UFUNCTION(BlueprintCallable, Category = "Montage")
	void SetCurMontageSlot(ESeekerMontageSlot InputMontageSlot);

	// Slot Change Control Value
	/*UPROPERTY(Replicated, BlueprintReadWrite, Category = "Montage", meta = (BlueprintThreadSafe))
	bool IsPlayingUpperBodyMontage = false;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "Montage", meta = (BlueprintThreadSafe))
	bool IsPlayingFullBodyMontage = false;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "Montage", meta = (BlueprintThreadSafe))
	bool IsPlayingLeftArmMontage = false;*/

	UPROPERTY(Replicated, BlueprintReadWrite, Category = "Montage", meta = (BlueprintThreadSafe))
	uint8 CurMontageSlot = 0;

	// Chooser
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Motion Matching")
	TObjectPtr<UGS_ChooserInputObj> ChooserInputObj;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EssentialValue")
	FVector Acceleration;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EssentialValue")
	FVector VelocityLastFrame;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EssentialValue")
	FVector VelocityAcceleration;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EssentialValue")
	FVector LastNonZeroVelocity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateValue")
	ERotationMode LastRotationMode;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateValue")
	EGait LastGait;

	/** Gait 전환 중인지 확인 */
	UPROPERTY(BlueprintReadOnly, Category = "StateValue")
	bool bIsTransitioningGait = false;

	/** Gait 전환 타이머 */
	float GaitTransitionTimer = 0.0f;

	/** Gait 전환 대기 시간 (초) - 애니메이션 전환 안정화 */
	static constexpr float GaitTransitionDelay = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OffsetRootBone")
	bool bUseOffsetRootBone = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bIsMoving = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	float PreviousDesiredController;

	FVector2D Get_AOValue_Internal();
	bool Enable_AO_Internal();

	UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
	FVector2D CachedAOValue;

	UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
	bool bCachedEnableAO = false;

	// ========================================
	// 빈사 상태
	// ========================================

	/** 빈사 상태인지 확인 (Animation Blueprint에서 사용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsDying = false;

	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe = "true"), Category = "Montage")
	bool IsMontageSlotActive(ESeekerMontageSlot InputMontageSlot);
};
