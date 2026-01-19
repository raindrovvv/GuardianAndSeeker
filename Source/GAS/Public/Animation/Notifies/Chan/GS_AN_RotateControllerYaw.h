// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_RotateControllerYaw.generated.h"

/**
 * @brief Animation notify that rotates the controller's yaw to face a target or the view direction.
 * Primarily used for aim assisting during attacks.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Rotate Controller Yaw"))
class GAS_API UGS_AN_RotateControllerYaw : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_RotateControllerYaw();

	/**
	 * @brief Triggered when the notify is executed in an animation.
	 */
	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;

protected:
	/** Maximum range to search for targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim Assist")
	float MaxAssistRange = 1800.0f;

	/** Radius of the sweep sphere for target detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim Assist")
	float TargetSearchRadius = 45.0f;
};
