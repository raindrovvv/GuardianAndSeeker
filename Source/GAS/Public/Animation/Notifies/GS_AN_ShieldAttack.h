// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_ShieldAttack.generated.h"

/**
 * @brief Animation notify to enable the collision of a shield for an attack.
 * Specifically designed for Chan's shield attack mechanics.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Shield Attack Notify"))
class GAS_API UGS_AN_ShieldAttack : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_ShieldAttack();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;

protected:
	/** Duration for which the shield attack collision remains active (in seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Shield", meta = (AllowPrivateAccess = "true"))
	float AttackHitDuration = 0.5f;
};
