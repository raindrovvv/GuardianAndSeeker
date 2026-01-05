#include "Character/Skill/Guardian/Drakhar/GS_DrakharFly.h"
#include "Character/Player/GS_Player.h"
#include "Character/Player/Guardian/GS_Drakhar.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Kismet/KismetSystemLibrary.h"

UGS_DrakharFly::UGS_DrakharFly()
{
	bIsFlying = false;

	CurrentSkillType = ESkillSlot::Ready;
}

void UGS_DrakharFly::ActiveSkill()
{
	Super::ActiveSkill();
	
	if (!CanActive())
	{
		return;
	}
	if (bIsFlying)
	{
		return;
	}

	bIsFlying = true;
	
	CachedDrakharOwner = Cast<AGS_Drakhar>(OwnerCharacter);

	// 멀티플레이어 환경에서 안전성 체크 추가
	if (CachedDrakharOwner.IsValid())
	{
		CachedDrakharOwner->MulticastRPC_OnFlyStart();
	}
	
	ExecuteSkillEffect();
}

void UGS_DrakharFly::OnSkillCanceledByDebuff()
{
	bIsFlying = false;
	
	if (CachedDrakharOwner.IsValid())
	{
		CachedDrakharOwner->MulticastRPC_OnFlyEnd();
		CachedDrakharOwner->GuardianDoSkillState = EGuardianDoSkill::None;
		CachedDrakharOwner->GuardianState = EGuardianCtrlState::CtrlEnd;
	}
	ExecuteSkillEffect();
}


void UGS_DrakharFly::ExecuteSkillEffect()
{
	// 멀티플레이어 환경에서 안전성 체크 추가
	if (!IsValid(OwnerCharacter))
	{
		return;
	}

	UAnimMontage* LoadedMontage = GetCachedMontage(0);
	if (!LoadedMontage)
	{
		return;
	}

	if (bIsFlying)
	{
		OwnerCharacter->MulticastRPCPlaySkillMontage(LoadedMontage);
	}
	else
	{
		OwnerCharacter->MulicastRPCStopCurrentSkillMontage(LoadedMontage);
	}
}
