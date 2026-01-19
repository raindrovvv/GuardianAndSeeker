// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GS_AN_ChanSwitchingAxeSlot.generated.h"

/**
 * @brief Enum defining the target socket state for Chan's axe switching.
 */
UENUM(BlueprintType)
enum class ESwitchingAxeSocket : uint8
{
	Wielding,  ///< Switch to the hand socket
	Sheathing, ///< Switch to the holster/back socket
	MAX		   ///< Max sentinel value
};

/**
 * @brief Animation notify for Chan to switch her axe between different sockets (wielded vs sheathed).
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Chan Switch Axe Socket"))
class GAS_API UGS_AN_ChanSwitchingAxeSlot : public UAnimNotify
{
	GENERATED_BODY()

public:
	UGS_AN_ChanSwitchingAxeSlot();

	virtual void Notify(USkeletalMeshComponent* MeshComp,
						UAnimSequenceBase* Animation,
						const FAnimNotifyEventReference& EventReference) override;

	/** The socket state to transition to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switching")
	ESwitchingAxeSocket TargetSocketState = ESwitchingAxeSocket::MAX;
};
