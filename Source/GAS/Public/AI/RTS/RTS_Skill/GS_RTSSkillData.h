#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GS_RTSSkillTypes.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInterface.h"
#include "AkAudioEvent.h"
#include "GS_RTSSkillData.generated.h"

class UGS_RTSSkillBase;
class USoundBase;
class UTexture2D;
class AGS_Monster;
class UGS_DebuffObscure;
class AActor;

/**
 * 데이터 에셋 기반 RTS 스킬 정보
 */
UCLASS(BlueprintType)
class GAS_API UGS_RTSSkillData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Info")
    FText SkillName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Info", meta=(MultiLine=true))
    FText SkillDescription;

    // Soft Reference로 메모리 최적화
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Info")
    TSoftObjectPtr<UTexture2D> SkillIcon;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Cost", meta=(ClampMin="0.0"))
    float AetherCost = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Cost", meta=(ClampMin="0.0"))
    float CooldownTime = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Targeting")
    ERTSSkillTargetType TargetType = ERTSSkillTargetType::None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Targeting", meta=(ClampMin="0.0"))
    float SkillRange = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Effect", meta=(ClampMin="0.0"))
    float EffectRadius = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Effect")
    float SkillPower = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Effect", meta=(ClampMin="0.0"))
    float EffectDuration = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Class")
    TSoftClassPtr<UGS_RTSSkillBase> SkillClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|VFX")
    TSoftObjectPtr<UNiagaraSystem> ActivationVFX;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Sound")
    TSoftObjectPtr<UAkAudioEvent> CastSound_TPS;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Sound")
    TSoftObjectPtr<UAkAudioEvent> CastSound_RTS;
};

/**
 * 소환 스킬용 데이터
 */
UCLASS(BlueprintType)
class GAS_API UGS_RTSSkillData_Summon : public UGS_RTSSkillData
{
    GENERATED_BODY()

public:
    // Soft Reference로 메모리 최적화
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Summon")
    TArray<TSoftClassPtr<AGS_Monster>> MonsterClasses;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Summon")
    float SpawnHeightOffset = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Summon", meta=(ClampMin="0.0"))
    float StatMultiplier = 1.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Summon")
    TSoftObjectPtr<UNiagaraSystem> SummonVFX;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Summon")
    TSoftObjectPtr<UAkAudioEvent> SummonSound_TPS;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Summon")
    TSoftObjectPtr<UAkAudioEvent> SummonSound_RTS;
};

/**
 * 불덩이 스킬용 데이터
 */
UCLASS(BlueprintType)
class GAS_API UGS_RTSSkillData_Fireball : public UGS_RTSSkillData
{
    GENERATED_BODY()

public:
    // Soft Reference로 메모리 최적화
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Projectile")
    TSoftClassPtr<AActor> ProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Projectile")
    float FallStartHeight = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Projectile")
    float FallSpeed = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Warning", meta=(ClampMin="0.0"))
    float WarningDuration = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Warning")
    TSoftObjectPtr<UMaterialInterface> WarningDecalMaterial;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|VFX")
    TSoftObjectPtr<UNiagaraSystem> TrailVFX;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|VFX")
    TSoftObjectPtr<UNiagaraSystem> ExplosionVFX;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Sound")
    TSoftObjectPtr<UAkAudioEvent> FallSound_TPS;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Sound")
    TSoftObjectPtr<UAkAudioEvent> FallSound_RTS;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Sound")
    TSoftObjectPtr<UAkAudioEvent> ExplosionSound_TPS;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Sound")
    TSoftObjectPtr<UAkAudioEvent> ExplosionSound_RTS;
};

/**
 * 시야 차단 스킬용 데이터
 */
UCLASS(BlueprintType)
class GAS_API UGS_RTSSkillData_ObscureVision : public UGS_RTSSkillData
{
    GENERATED_BODY()

public:
    // Soft Reference로 메모리 최적화
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Sound")
    TSoftObjectPtr<UAkAudioEvent> ObscureActivateSound_TPS;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Sound")
    TSoftObjectPtr<UAkAudioEvent> ObscureActivateSound_RTS;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Debuff")
    TSoftClassPtr<UGS_DebuffObscure> ObscureDebuffClass;
};
