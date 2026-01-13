// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GS_RTSSkillBase.h"
#include "GS_RTSSkill_SummonNormal.generated.h"

class AGS_Monster;
class UGS_RTSSkillData_Summon;

/**
 * 스킬 1: 노멀 몬스터 소환
 * 지정한 위치에 일반 몬스터를 소환합니다.
 */
UCLASS(Blueprintable)
class GAS_API UGS_RTSSkill_SummonNormal : public UGS_RTSSkillBase
{
	GENERATED_BODY()

public:
	UGS_RTSSkill_SummonNormal();

	virtual FVector ActivateSkill(UGS_RTSSkillComponent* SkillComponent, const FVector& TargetLocation) override;
	virtual void PlayCastEffects(const FVector& TargetLocation) override;
	virtual void PreloadAssets() override;

protected:
	const UGS_RTSSkillData_Summon* GetSummonData() const;

	// 캐싱된 에셋들
	UPROPERTY()
	TObjectPtr<UNiagaraSystem> CachedSummonVFX;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedSummonSound_TPS;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedSummonSound_RTS;

	UPROPERTY()
	TArray<TSubclassOf<AGS_Monster>> CachedMonsterClasses;

private:
	// 소환 실행 (서버에서만)
	FVector SpawnMonsterAtLocation(const FVector& Location);

	// 유효한 소환 위치 찾기
	bool FindValidSpawnLocation(const FVector& DesiredLocation, FVector& OutValidLocation) const;
};
