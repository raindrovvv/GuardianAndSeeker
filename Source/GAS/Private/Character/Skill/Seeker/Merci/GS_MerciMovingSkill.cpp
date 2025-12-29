// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Skill/Seeker/Merci/GS_MerciMovingSkill.h"
#include "Character/Player/Seeker/GS_Merci.h"
#include "Weapon/Projectile/Seeker/GS_SeekerMerciArrow.h"
#include "Sound/GS_SeekerAudioComponent.h"

UGS_MerciMovingSkill::UGS_MerciMovingSkill()
{
	CurrentSkillType = ESkillSlot::Moving;
}

void UGS_MerciMovingSkill::ActiveSkill()
{
	Super::ActiveSkill();

	CachedMerciOwner = Cast<AGS_Merci>(OwnerCharacter);
	if (CachedMerciOwner.IsValid())
	{
		// 스킬 시작 사운드 재생 (멀티캐스트)
		if (CachedMerciOwner->HasAuthority())
		{
			if (UGS_SeekerAudioComponent* AudioComp = CachedMerciOwner->SeekerAudioComponent)
			{
				AudioComp->RequestSkillAudio(CurrentSkillType, 0);
			}
		}

		CachedMerciOwner->SetDrawState(false);

		if (UAnimMontage* LoadedMontage = GetCachedMontage(0))
		{
			CachedMerciOwner->DrawBow(LoadedMontage);
		}
		CachedMerciOwner->Client_StartZoom();
	}
}

void UGS_MerciMovingSkill::OnSkillAnimationEnd()
{
}

void UGS_MerciMovingSkill::OnSkillCommand()
{
	if (!CanActive() || !GetIsActive())
	{
		UE_LOG(LogTemp, Warning, TEXT("Moving Can't Start Command"));
		return;
	}

	// 활 놓기
	if (CachedMerciOwner.IsValid())
	{
		bool IsFullyDrawn = CachedMerciOwner->GetIsFullyDrawn();
		
		if (CachedMerciOwner->SmokeArrowClass)
		{
			CachedMerciOwner->ReleaseArrow(CachedMerciOwner->SmokeArrowClass);
		}
		
		if (IsFullyDrawn)
		{
			// 쿨타임 측정 시작
			StartCoolDown();
		}
	}

	// 스킬 종료
	DeactiveSkill();
}

void UGS_MerciMovingSkill::InterruptSkill()
{
	Super::InterruptSkill();

	SetIsActive(false);
}

void UGS_MerciMovingSkill::DeactiveSkill()
{
	// 스킬 종료 사운드 재생 (멀티캐스트)
	if (OwnerCharacter->HasAuthority())
	{
		if (AGS_Seeker* OwnerSeeker = Cast<AGS_Seeker>(OwnerCharacter))
		{
			if (UGS_SeekerAudioComponent* AudioComp = OwnerSeeker->SeekerAudioComponent)
			{
				AudioComp->RequestSkillAudio(CurrentSkillType, 1);
			}
		}
	}

	Super::DeactiveSkill();
}

