// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_Heal.generated.h"

/**
 * @brief Animation notify that triggers a healing effect on the character.
 * Consumes a potion from the character's inventory and applies health restoration.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Heal Notify"))
class GAS_API UGS_AN_Heal : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_Heal();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;
};
