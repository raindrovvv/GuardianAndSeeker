#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "System/GS_PlayerRole.h"
#include "GS_SpawnSlot.generated.h"

UCLASS()
class GAS_API AGS_SpawnSlot : public AActor
{
	GENERATED_BODY()
	
public:	
	AGS_SpawnSlot();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Slot")
	EPlayerRole ForRole = EPlayerRole::PR_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Slot")
	int32 SlotIndex = 0;

	FORCEINLINE EPlayerRole GetRole() const { return ForRole; }
	FORCEINLINE int32 GetSlotIndex() const { return SlotIndex; }
};
