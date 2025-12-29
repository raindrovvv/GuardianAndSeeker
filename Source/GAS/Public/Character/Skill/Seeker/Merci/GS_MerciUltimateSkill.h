// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/Skill/Seeker/GS_SeekerSkillBase.h"
#include "GS_MerciUltimateSkill.generated.h"

/**
 * 
 */
UCLASS()
class GAS_API UGS_MerciUltimateSkill : public UGS_SeekerSkillBase
{
	GENERATED_BODY()

public:
	UGS_MerciUltimateSkill();
	
	virtual void ActiveSkill() override;
	virtual void OnSkillAnimationEnd() override;
	virtual void InterruptSkill() override;

private:
	UFUNCTION()
	virtual void DeactiveSkill() override;

	// 스탠스 관리
	FTimerHandle AutoAimingHandle;
	FTimerHandle AutoAimTickHandle;
	float AutoAimTickInterval = 0.2f;

	void AutoAimingStart();
	float AutoAimingStateTime = 10.0f;

	AActor* FindCloseTarget();

	UFUNCTION()
	void TickAutoAimTarget();

	void UpdateMonsterList();

	// 캐싱된 Merci 소유자 (ActiveSkill에서 설정)
	UPROPERTY()
	TWeakObjectPtr<class AGS_Merci> CachedMerciOwner;

	UPROPERTY()
	TObjectPtr<AActor> CurrentTarget;
};
