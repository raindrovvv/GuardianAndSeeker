// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/Skill/Seeker/GS_SeekerSkillBase.h"
#include "GS_MerciAimingSkill.generated.h"

class AGS_SeekerMerciArrow;
/**
 * 
 */
UCLASS()
class GAS_API UGS_MerciAimingSkill : public UGS_SeekerSkillBase
{
	GENERATED_BODY()
	
public:
	UGS_MerciAimingSkill();
	virtual void InitializeDelegate() override;
	virtual void ActiveSkill() override;
	virtual void OnSkillCommand() override;
	virtual void OnSkillAnimationEnd() override;
	virtual void InterruptSkill() override;

	TSubclassOf<AGS_SeekerMerciArrow> ArrowClass;
private:
	virtual void DeactiveSkill() override;

	/** OnSkillActivated 델리게이트에 바인딩할 함수 */
	UFUNCTION()
	void HandleSkillActivated(ESkillSlot ActivatedSkillSlot);

	/** 캐싱된 Merci 소유자 */
	UPROPERTY()
	TWeakObjectPtr<class AGS_Merci> CachedMerciOwner;

	/** 5초 후 자동 해제를 위한 타이머 */
	FTimerHandle AimTimeoutTimerHandle;
};
