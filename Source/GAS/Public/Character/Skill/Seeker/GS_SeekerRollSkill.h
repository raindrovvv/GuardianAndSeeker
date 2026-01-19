// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GS_SeekerSkillBase.h"
#include "GS_SeekerRollSkill.generated.h"

/**
 * @brief Seeker rolling skill that provides mobility and temporary invulnerability.
 * Manages montage playback based on movement direction and handles collision switching.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Seeker Roll Skill"))
class GAS_API UGS_SeekerRollSkill : public UGS_SeekerSkillBase
{
	GENERATED_BODY()

public:
	UGS_SeekerRollSkill();

	// UGS_SkillBase interface
	virtual void ActiveSkill() override;
	virtual void OnSkillCanceledByDebuff() override;
	virtual void OnSkillAnimationEnd() override;
	virtual void InterruptSkill() override;
	// ~UGS_SkillBase interface

protected:
	/** Internal delegate to handle logic when the roll montage concludes */
	FOnMontageEnded RollEndDelegate;

	/** Handles cleanup and state restoration once the roll animation finishes */
	UFUNCTION()
	void HandleRollMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Weak pointer to the seeker character owning this skill instance */
	UPROPERTY()
	TWeakObjectPtr<class AGS_Seeker> CachedSeeker;
};