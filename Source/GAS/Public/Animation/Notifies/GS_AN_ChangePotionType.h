// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_ChangePotionType.generated.h"

/**
 * @brief Animation notify to change the visual mesh variant of a potion.
 * Used during drinking or handling animations where the potion's appearance changes (e.g., full to empty).
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Change Potion Type Notify"))
class GAS_API UGS_AN_ChangePotionType : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_ChangePotionType();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;

	/** The name of the mesh variant to apply to the potion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Potion")
	FName TargetMeshVariantName = NAME_None;
};
