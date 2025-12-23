// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Skill/Seeker/Chan/GS_ChanRollingSkill.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Character/GS_TpsController.h"
#include "Components/CapsuleComponent.h"
#include "Sound/GS_SeekerAudioComponent.h"

UGS_ChanRollingSkill::UGS_ChanRollingSkill()
{
}

void UGS_ChanRollingSkill::ActiveSkill()
{
	Super::ActiveSkill();

	CachedChanOwner = Cast<AGS_Chan>(OwnerCharacter);
}

void UGS_ChanRollingSkill::OnSkillCanceledByDebuff()
{
	Super::OnSkillCanceledByDebuff();
}

void UGS_ChanRollingSkill::OnSkillAnimationEnd()
{
	Super::OnSkillAnimationEnd();

	if (CachedChanOwner.IsValid())
	{
		if (CachedChanOwner->HasAuthority())
		{
			CachedChanOwner->Multicast_StopSkillMontage(SkillAnimMontages[0]);
			CachedChanOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::None);
			CachedChanOwner->CanChangeSeekerGait = true;

			// 스킬 종료 사운드 재생 (멀티캐스트)
			if (UGS_SeekerAudioComponent* AudioComp = CachedChanOwner->SeekerAudioComponent)
			{
				AudioComp->RequestSkillAudio(CurrentSkillType, 1);
			}

			SetIsActive(false);

			CachedChanOwner->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		}
	}
}

void UGS_ChanRollingSkill::InterruptSkill()
{
	Super::InterruptSkill();
	if (CachedChanOwner.IsValid())
	{
		if (CachedChanOwner->GetSkillComp())
		{
			CachedChanOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::None);
			CachedChanOwner->SetMoveControlValue(true, true);
		}
	}
	SetIsActive(false);
}
