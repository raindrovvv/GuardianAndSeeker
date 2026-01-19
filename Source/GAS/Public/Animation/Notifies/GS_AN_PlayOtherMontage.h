// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_PlayOtherMontage.generated.h"

class UAnimMontage;

/**
 * @brief Animation notify that triggers the playback of another montage on the character.
 * Often used to chain montages or trigger reaction/skill sequents.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Play Other Montage Notify"))
class GAS_API UGS_AN_PlayOtherMontage : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_PlayOtherMontage();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;

	/** The next montage to play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimMontage> TargetMontage = nullptr;
};
