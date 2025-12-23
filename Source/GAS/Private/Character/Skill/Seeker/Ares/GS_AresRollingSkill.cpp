// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Skill/Seeker/Ares/GS_AresRollingSkill.h"

#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Character/Player/Seeker/GS_Ares.h"
#include "Sound/GS_SeekerAudioComponent.h"

UGS_AresRollingSkill::UGS_AresRollingSkill()
{
	CurrentSkillType = ESkillSlot::Rolling;
}

void UGS_AresRollingSkill::ActiveSkill()
{	
	Super::ActiveSkill();

	CachedAresOwner = Cast<AGS_Ares>(OwnerCharacter);
}

void UGS_AresRollingSkill::OnSkillCanceledByDebuff()
{
	Super::OnSkillCanceledByDebuff();
}

void UGS_AresRollingSkill::OnSkillAnimationEnd()
{
	Super::OnSkillAnimationEnd();

	if (CachedAresOwner.IsValid())
	{
		if (CachedAresOwner->HasAuthority())
		{
			CachedAresOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::None);
			CachedAresOwner->SetMoveControlValue(true, true);
			CachedAresOwner->CanChangeSeekerGait = true;

			// 스킬 종료 사운드 재생 (멀티캐스트)
			if (UGS_SeekerAudioComponent* AudioComp = CachedAresOwner->SeekerAudioComponent)
			{
				AudioComp->RequestSkillAudio(CurrentSkillType, 1);
			}

			SetIsActive(false);
		}
	}
}

void UGS_AresRollingSkill::InterruptSkill()
{
	Super::InterruptSkill();
	if (CachedAresOwner.IsValid() && CachedAresOwner->GetSkillComp())
	{
		CachedAresOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::None);
		CachedAresOwner->CanChangeSeekerGait = true;
	}
	SetIsActive(false);
}
