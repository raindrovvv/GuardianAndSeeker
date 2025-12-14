// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GS_RTSSkillBase.h"
#include "GS_RTSSkill_ObscureVision.generated.h"

class AGS_Seeker;
class UGS_DebuffObscure;
class UGS_RTSSkillData_ObscureVision;

/**
 * 스킬 3: 시야 차단
 * 모든 시커의 시야를 일정 시간 동안 가립니다.
 * 기존에 구현된 GS_DebuffObscure를 활용합니다.
 */
UCLASS(Blueprintable)
class GAS_API UGS_RTSSkill_ObscureVision : public UGS_RTSSkillBase
{
	GENERATED_BODY()

public:
	UGS_RTSSkill_ObscureVision();

	// 즉시 발동 스킬이므로 타겟 위치 불필요
	virtual FVector ActivateSkill(UGS_RTSSkillComponent* SkillComponent, const FVector& TargetLocation) override;
	virtual bool CanActivate(UGS_RTSSkillComponent* SkillComponent) const override;

protected:
	const UGS_RTSSkillData_ObscureVision* GetObscureVisionData() const;

private:
	// 모든 시커에게 디버프 적용
	void ApplyObscureToAllSeekers();
};
