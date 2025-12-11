#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GS_RTSSkillTypes.h"
#include "GS_RTSSkillData.generated.h"

class UGS_RTSSkillBase;
class UParticleSystem;
class USoundBase;
class UTexture2D;
class AGS_Monster;
class UMaterialInterface;
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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Info")
    TObjectPtr<UTexture2D> SkillIcon;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Cost", meta=(ClampMin="0.0"))
    float EtherCost = 0.f;

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
    TSubclassOf<UGS_RTSSkillBase> SkillClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|VFX")
    TObjectPtr<UParticleSystem> ActivationVFX;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Sound")
    TObjectPtr<USoundBase> CastSound;
};

/**
 * 소환 스킬용 데이터
 */
UCLASS(BlueprintType)
class GAS_API UGS_RTSSkillData_Summon : public UGS_RTSSkillData
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Summon")
    TArray<TSubclassOf<AGS_Monster>> MonsterClasses;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Summon")
    float SpawnHeightOffset = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Summon", meta=(ClampMin="0.0"))
    float StatMultiplier = 1.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Summon")
    TObjectPtr<UParticleSystem> SummonVFX;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Summon")
    TObjectPtr<USoundBase> SummonSound;
};

/**
 * 불덩이 스킬용 데이터
 */
UCLASS(BlueprintType)
class GAS_API UGS_RTSSkillData_Fireball : public UGS_RTSSkillData
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Projectile")
    TSubclassOf<AActor> ProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Projectile")
    float FallStartHeight = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Projectile")
    float FallSpeed = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Warning", meta=(ClampMin="0.0"))
    float WarningDuration = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Warning")
    TObjectPtr<UMaterialInterface> WarningDecalMaterial;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|VFX")
    TObjectPtr<UParticleSystem> TrailVFX;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|VFX")
    TObjectPtr<UParticleSystem> ExplosionVFX;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Sound")
    TObjectPtr<USoundBase> FallSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Sound")
    TObjectPtr<USoundBase> ExplosionSound;
};

/**
 * 시야 차단 스킬용 데이터
 */
UCLASS(BlueprintType)
class GAS_API UGS_RTSSkillData_ObscureVision : public UGS_RTSSkillData
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Sound")
    TObjectPtr<USoundBase> ObscureActivateSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Debuff")
    TSubclassOf<UGS_DebuffObscure> ObscureDebuffClass;
};
