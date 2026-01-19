// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GS_ANS_HitWeapon.generated.h"

/**
 * @brief Animation notify state to enable and disable the hit detection for a character's primary weapon.
 * Toggles the 'ServerEnableHit' state on the weapon actor attached to the first weapon slot.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Weapon Hitbox Notify State"))
class GAS_API UGS_ANS_HitWeapon : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UGS_ANS_HitWeapon();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp,
							 UAnimSequenceBase* Animation,
							 float TotalDuration,
							 const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp,
						   UAnimSequenceBase* Animation,
						   const FAnimNotifyEventReference& EventReference) override;
};
