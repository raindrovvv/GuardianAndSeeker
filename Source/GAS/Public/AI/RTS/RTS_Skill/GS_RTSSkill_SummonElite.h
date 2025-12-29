// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GS_RTSSkillBase.h"
#include "GS_RTSSkill_SummonElite.generated.h"

class AGS_Monster;
class UGS_RTSSkillData_Summon;

/**
 * 스킬 2: 엘리트 몬스터 소환
 * 지정한 위치에 강력한 엘리트 몬스터를 소환합니다.
 * 노멀 몬스터보다 에테르 비용이 높고 쿨다운이 깁니다.
 */
UCLASS(Blueprintable)
class GAS_API UGS_RTSSkill_SummonElite : public UGS_RTSSkillBase
{
	GENERATED_BODY()

public:
	UGS_RTSSkill_SummonElite();

	virtual FVector ActivateSkill(UGS_RTSSkillComponent* SkillComponent, const FVector& TargetLocation) override;
	virtual void PlayCastEffects(const FVector& TargetLocation) override;

protected:
	const UGS_RTSSkillData_Summon* GetSummonData() const;

private:
	FVector SpawnEliteMonsterAtLocation(const FVector& Location);
	bool FindValidSpawnLocation(const FVector& DesiredLocation, FVector& OutValidLocation) const;
};
