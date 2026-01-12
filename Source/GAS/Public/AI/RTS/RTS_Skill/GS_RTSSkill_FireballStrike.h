// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GS_RTSSkillBase.h"
#include "GS_RTSSkill_FireballStrike.generated.h"

class UGS_RTSSkillData_Fireball;

/**
 * 스킬 4: 불덩이 투하
 * 하늘에서 불덩이를 떨어뜨려 지정한 위치에 범위 피해를 입힙니다.
 */
UCLASS(Blueprintable)
class GAS_API UGS_RTSSkill_FireballStrike : public UGS_RTSSkillBase
{
	GENERATED_BODY()

public:
	UGS_RTSSkill_FireballStrike();

	virtual FVector ActivateSkill(UGS_RTSSkillComponent* SkillComponent, const FVector& TargetLocation) override;
	virtual void PreloadAssets() override;

protected:
	const UGS_RTSSkillData_Fireball* GetFireballData() const;

	// 캐싱된 에셋들
	UPROPERTY()
	TSubclassOf<AActor> CachedProjectileClass;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> CachedWarningDecalMaterial;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> CachedTrailVFX;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> CachedExplosionVFX;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedFallSound_TPS;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedFallSound_RTS;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedExplosionSound_TPS;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedExplosionSound_RTS;

private:
	// 경고 표시 후 불덩이 생성
	void ShowWarningAndSpawnFireball(const FVector& TargetLocation);

	// 불덩이 생성
	void SpawnFireball();

	// 타이머 핸들
	FTimerHandle SpawnFireballTimerHandle;

	// 임시 저장용 타겟 위치
	FVector PendingTargetLocation;
};
