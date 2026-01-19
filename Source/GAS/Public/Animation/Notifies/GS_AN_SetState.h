// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Character/GS_TpsController.h"
#include "Animation/Character/E_SeekerAnim.h"
#include "GS_AN_SetState.generated.h"

/**
 * @brief Animation notify to set various character states during an animation.
 * Allows fine-grained control over movement, gait, combo windows, and skill permissions.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Set Character State Notify"))
class GAS_API UGS_AN_SetState : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_SetState();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;

	/** Update the active montage slot for the seeker */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State|Animation")
	ESeekerMontageSlot TargetMontageSlot = ESeekerMontageSlot::End;

	/** Whether the character can switch their gait (Walk/Run/Sprint) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State|Movement")
	bool bAllowGaitChange = true;

	/** Whether the character can accept next combo inputs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State|Combat")
	bool bAllowComboInput = true;

	/** If true, explicitly set movement/look control values using ControlSettings */
	UPROPERTY(EditAnywhere, Category = "State|Control")
	bool bOverrideControlValues = false;

	/** New control values to apply if bOverrideControlValues is true */
	UPROPERTY(EditAnywhere,
			  BlueprintReadWrite,
			  Category = "State|Control",
			  meta = (EditCondition = "bOverrideControlValues"))
	FControlValue ControlSettings;

	/** If true, change the character's gait to NewGait */
	UPROPERTY(EditAnywhere, Category = "State|Movement")
	bool bApplyNewGait = false;

	/** The new gait to apply if bApplyNewGait is true */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State|Movement", meta = (EditCondition = "bApplyNewGait"))
	EGait NewGait = EGait::Walk;

	/** If true, reset the allowed skills mask to default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State|Skills")
	bool bTriggerSkillReset = false;
};
