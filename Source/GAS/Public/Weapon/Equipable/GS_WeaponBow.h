// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GS_WeaponEquipable.h"
#include "GS_WeaponBow.generated.h"

/**
 * @brief Seeker's bow weapon class.
 * Manages the bow's visual mesh and the child actor component for arrows.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Seeker Bow"))
class GAS_API AGS_WeaponBow : public AGS_WeaponEquipable
{
	GENERATED_BODY()

public:
	AGS_WeaponBow();

protected:
	virtual void BeginPlay() override;

	/** Skeletal mesh representing the bow itself (includes string/limbs) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Mesh")
	TObjectPtr<USkeletalMeshComponent> BowMesh;

	/** Child actor component that represents the arrow currently notched or prepared */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Attachment")
	TObjectPtr<UChildActorComponent> ArrowActorComponent;
};
