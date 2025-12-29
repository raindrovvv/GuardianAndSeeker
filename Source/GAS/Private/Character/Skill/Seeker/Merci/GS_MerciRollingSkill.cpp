// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Skill/Seeker/Merci/GS_MerciRollingSkill.h"

#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Character/Player/Seeker/GS_Merci.h"
#include "Sound/GS_SeekerAudioComponent.h"
#include "Components/CapsuleComponent.h"

UGS_MerciRollingSkill::UGS_MerciRollingSkill()
{
	CurrentSkillType = ESkillSlot::Rolling;
}

void UGS_MerciRollingSkill::ActiveSkill()
{
	if (!OwnerCharacter)
	{
		return;
	}
	CachedMerciOwner = Cast<AGS_Merci>(OwnerCharacter);
	if (!CachedMerciOwner.IsValid())
	{
		return;
	}
	CachedMerciOwner->SetAimState(false);
	CachedMerciOwner->SetDrawState(false);
	Super::ActiveSkill();
}

void UGS_MerciRollingSkill::OnSkillAnimationEnd()
{
	Super::OnSkillAnimationEnd();

	if (CachedMerciOwner.IsValid())
	{
		if (CachedMerciOwner->HasAuthority())
		{
			CachedMerciOwner->Multicast_StopSkillMontage(SkillAnimMontages[0]);
			CachedMerciOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::None);
			CachedMerciOwner->CanChangeSeekerGait = true;

			// 스킬 종료 사운드 재생 (멀티캐스트)
			if (UGS_SeekerAudioComponent* AudioComp = CachedMerciOwner->SeekerAudioComponent)
			{
				AudioComp->RequestSkillAudio(CurrentSkillType, 1);
			}

			SetIsActive(false);

			CachedMerciOwner->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		}
	}
}

void UGS_MerciRollingSkill::InterruptSkill()
{
	Super::InterruptSkill();
	if (CachedMerciOwner.IsValid())
	{
		if (CachedMerciOwner->GetSkillComp())
		{
			CachedMerciOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::None);
			CachedMerciOwner->SetMoveControlValue(true, true);
		}
	}
	SetIsActive(false);
}
