#pragma once

#include "CoreMinimal.h"
#include "Character/GS_Character.h"
//#include "Character/Skill/GS_SkillBase.h"
#include "ESkill.h"
#include "NiagaraSystem.h"
#include "GS_SkillSet.generated.h"

class UGS_SkillBase;
class UImage;
class UAkAudioEvent;

USTRUCT(BlueprintType)
struct FSkillAllow
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Skill")
	ESkillSlot Slot = ESkillSlot();

	UPROPERTY(EditAnywhere, Category = "Skill")
	bool bAllow = false;
};


USTRUCT(BlueprintType)
struct GAS_API FSkillInfo
{
	GENERATED_BODY()

	FSkillInfo()
	{
		AllowSkillList.Empty();
		for (int32 i = 0; i < static_cast<int32>(ESkillSlot::End); i++)
		{
			FSkillAllow AllowSkill;
			AllowSkill.Slot = static_cast<ESkillSlot>(i);
			AllowSkillList.Add(AllowSkill);
		}
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGS_SkillBase> SkillClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Cooltime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Damage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSoftObjectPtr<UAnimMontage>> Montages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UTexture2D> Image = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSkillAllow> AllowSkillList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FControlValue AllowControlValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllowHitReacct = false;


	// 스킬 사운드 이벤트들
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	TSoftObjectPtr<UAkAudioEvent> SkillStartSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	TSoftObjectPtr<UAkAudioEvent> SkillEndSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	TSoftObjectPtr<UAkAudioEvent> SkillLoopSound; // 스킬 가동 중 루프 사운드 (궁극기용)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	TSoftObjectPtr<UAkAudioEvent> SkillLoopStopSound; // 스킬 루프 사운드 정지 이벤트 (궁극기용)

	// RTS 모드 전용 스킬 사운드 (존재하지 않으면 TPS 사운드로 폴백)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound|RTS")
	TSoftObjectPtr<UAkAudioEvent> RTSSkillStartSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound|RTS")
	TSoftObjectPtr<UAkAudioEvent> RTSSkillEndSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound|RTS")
	TSoftObjectPtr<UAkAudioEvent> RTSSkillLoopSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound|RTS")
	TSoftObjectPtr<UAkAudioEvent> RTSSkillLoopStopSound;

	// 충돌별 특수 사운드 (주로 궁극기 스킬용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound|Collision")
	TSoftObjectPtr<UAkAudioEvent> WallCollisionSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound|Collision")
	TSoftObjectPtr<UAkAudioEvent> MonsterCollisionSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound|Collision")
	TSoftObjectPtr<UAkAudioEvent> GuardianCollisionSound;

	// RTS 모드 전용 충돌 사운드 (존재하지 않으면 TPS 사운드로 폴백)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound|Collision|RTS")
	TSoftObjectPtr<UAkAudioEvent> RTSWallCollisionSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound|Collision|RTS")
	TSoftObjectPtr<UAkAudioEvent> RTSMonsterCollisionSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound|Collision|RTS")
	TSoftObjectPtr<UAkAudioEvent> RTSGuardianCollisionSound;

	// 스킬 VFX 이벤트들
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> SkillCastVFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> SkillRangeVFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> SkillImpactVFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> SkillEnvImpactVFX; // 환경(벽, 바닥) 충돌용 VFX

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> SkillEndVFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> SkillLoopVFX; // 스킬 지속 중 루프 VFX (궁극기 아우라 등)

	// VFX 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	FVector SkillVFXScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	float SkillVFXDuration = 3.0f;

	// VFX 위치 오프셋
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX|Offset")
	FVector CastVFXOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX|Offset")
	FVector RangeVFXOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX|Offset")
	FVector ImpactVFXOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX|Offset")
	FVector EnvImpactVFXOffset = FVector::ZeroVector; // 환경 충돌용 VFX 오프셋

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX|Offset")
	FVector EndVFXOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX|Offset")
	FVector LoopVFXOffset = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct GAS_API FGS_SkillSet : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECharacterType CharacterType = ECharacterType::Chan;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSkillInfo ReadySkill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSkillInfo AimingSkill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSkillInfo MovingSkill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSkillInfo UltimateSkill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSkillInfo RollingSkill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSkillInfo ComboSkill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSkillInfo HealPotionSkill;

	FGS_SkillSet()
	{
	}
};
