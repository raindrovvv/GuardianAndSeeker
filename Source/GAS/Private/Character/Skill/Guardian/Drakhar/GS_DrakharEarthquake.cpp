#include "Character/Skill/Guardian/Drakhar/GS_DrakharEarthquake.h"
#include "Character/Player/Guardian/GS_Drakhar.h"
#include "Character/Player/Guardian/GS_DrakharAnimInstance.h"
#include "Character/Skill/GS_SkillComp.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"

UGS_DrakharEarthquake::UGS_DrakharEarthquake()
{
	CurrentSkillType = ESkillSlot::Aiming;
}

void UGS_DrakharEarthquake::ActiveSkill()
{
	Super::ActiveSkill();

	//cool time check
	if (!CanActive())
	{
		return;
	}

	CachedDrakharOwner = Cast<AGS_Drakhar>(OwnerCharacter);
	
	ExecuteSkillEffect();
}

void UGS_DrakharEarthquake::ExecuteSkillEffect()
{
	Super::ExecuteSkillEffect();
	
	if (!OwnerCharacter->HasAuthority())
	{
		return;
	}

	//server logic
	if (CachedDrakharOwner.IsValid())
	{
		CachedDrakharOwner->GuardianDoSkillState = EGuardianDoSkill::Aiming;
	}
	
	StartCoolDown();
	
	if (OwnerCharacter)
	{
		if (UAnimMontage* LoadedMontage = GetCachedMontage(0))
		{
			OwnerCharacter->MulticastRPCPlaySkillMontage(LoadedMontage);
		}
	}
}

void UGS_DrakharEarthquake::OnSkillAnimationEnd()
{
	Super::OnSkillAnimationEnd();
}
