#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GS_TrapData.generated.h"

// Forward declarations
class UAkAudioEvent;
class UNiagaraSystem;

//함정 배치 위치 분류
UENUM(BlueprintType)
enum class ETrapPlacement : uint8
{
	Floor UMETA(DisplayName = "Floor"),
	Wall UMETA(DisplayName = "Wall"),
	Ceiling UMETA(DisplayName = "Ceiling")
};


//함정 효과 분류
USTRUCT(BlueprintType)
struct FTrapEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Damage = 0.f;

	//stun
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bStun = false;

	//slow
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSlow = false;

	//Burn
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bBurn = false;

	//Lava
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bLava = false;
};

USTRUCT(BlueprintType)
struct FTrapData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TrapID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETrapPlacement Placement = ETrapPlacement::Wall;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTrapEffect Effect;

	// ===================
	// Audio Events (TPS/RTS Unified)
	// ===================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<UAkAudioEvent> ActivationSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<UAkAudioEvent> AlertSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<UAkAudioEvent> HitSound; // 함정 히트 사운드

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<UAkAudioEvent> DeactivationSound;

	// ===================
	// VFX Systems
	// ===================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> TrapHitBloodEffect; // 함정 타격 시 혈흔 이펙트
};
