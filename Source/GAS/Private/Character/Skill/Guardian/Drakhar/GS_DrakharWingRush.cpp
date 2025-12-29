#include "Character/Skill/Guardian/Drakhar/GS_DrakharWingRush.h"
#include "Character/Player/GS_Player.h"
#include "Character/Player/Guardian/GS_Guardian.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Character/Player/Guardian/GS_Drakhar.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Templates/SharedPointer.h"

UGS_DrakharWingRush::UGS_DrakharWingRush()
{
	CurrentSkillType = ESkillSlot::Moving;
}

void UGS_DrakharWingRush::ActiveSkill()
{
	Super::ActiveSkill();
	
	if (!CanActive())
	{
		return;
	}

	CachedDrakharOwner = Cast<AGS_Drakhar>(OwnerCharacter);
	
	ExecuteSkillEffect();
}

void UGS_DrakharWingRush::ExecuteSkillEffect()
{
	Super::ExecuteSkillEffect();

	if (!OwnerCharacter->HasAuthority())
	{
		return;
	}

	//server logic
	if (CachedDrakharOwner.IsValid())
	{
		CachedDrakharOwner->GuardianDoSkillState = EGuardianDoSkill::Moving;	
	}
	
	StartCoolDown();
	
	if (OwnerCharacter)
	{
		//play montage, except server
		OwnerCharacter->MulticastRPCPlaySkillMontage(SkillAnimMontages[0]);
	}
	
}

void UGS_DrakharWingRush::OnSkillAnimationEnd()
{
	Super::OnSkillAnimationEnd();
}

