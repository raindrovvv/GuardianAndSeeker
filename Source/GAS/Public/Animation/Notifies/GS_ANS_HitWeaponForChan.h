// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GS_ANS_HitWeaponForChan.generated.h"

/**
 * @brief Animation notify state to enable and disable hit detection specifically for Chan's primary weapon.
 * Functionally identical to UGS_ANS_HitWeapon but maintained for Asset compatibility.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Chan Weapon Hitbox Notify State"))
class GAS_API UGS_ANS_HitWeaponForChan : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UGS_ANS_HitWeaponForChan();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp,
							 UAnimSequenceBase* Animation,
							 float TotalDuration,
							 const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp,
						   UAnimSequenceBase* Animation,
						   const FAnimNotifyEventReference& EventReference) override;
};
