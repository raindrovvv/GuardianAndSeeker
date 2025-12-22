// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Skill/Seeker/Merci/GS_MerciAimingSkill.h"
#include "Character/Player/Seeker/GS_Merci.h"
#include "Weapon/Projectile/Seeker/GS_SeekerMerciArrow.h"
#include "Sound/GS_SeekerAudioComponent.h"

UGS_MerciAimingSkill::UGS_MerciAimingSkill()
{
	CurrentSkillType = ESkillSlot::Aiming;
}

void UGS_MerciAimingSkill::InitializeDelegate()
{
	Super::InitializeDelegate();

	if (OwningComp)
	{
		OwningComp->OnSkillActivated.AddDynamic(this, &UGS_MerciAimingSkill::HandleSkillActivated);
	}
}

void UGS_MerciAimingSkill::HandleSkillActivated(ESkillSlot ActivatedSkillSlot)
{
	if (ActivatedSkillSlot == CurrentSkillType)
	{
		if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
		{
			if (!CachedMerciOwner.IsValid())
			{
				CachedMerciOwner = Cast<AGS_Merci>(OwnerCharacter);
			}

			if (CachedMerciOwner.IsValid())
			{
				CachedMerciOwner->Client_StartZoom();
			}
		}
	}
}

void UGS_MerciAimingSkill::ActiveSkill()
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

		// 활 당기기
		CachedMerciOwner->DrawBow(SkillAnimMontages[0]);

		// 5초 후 자동 조준 해제 타이머 시작 (서버에서 실행)
		if (OwnerCharacter->HasAuthority())
		{
			OwnerCharacter->GetWorldTimerManager().SetTimer(
				AimTimeoutTimerHandle,
				this,
				&UGS_MerciAimingSkill::DeactiveSkill,
				5.0f,
				false
			);
		}
	}
}

void UGS_MerciAimingSkill::OnSkillCommand()
{
	if (!CanActive() || !GetIsActive())
	{
		return;
	}

	// 활 놓기
	if (CachedMerciOwner.IsValid())
	{
		bool IsFullyDrawn = CachedMerciOwner->GetIsFullyDrawn();
		if(CachedMerciOwner->NormalArrowClass)
		{
			CachedMerciOwner->ReleaseArrow(CachedMerciOwner->NormalArrowClass, 15.0f, 4);
		}
		if(IsFullyDrawn)
		{
			// 쿨타임 측정 시작
			StartCoolDown();
		}
	}

	// 타이머 정리
	if (OwnerCharacter)
	{
		OwnerCharacter->GetWorldTimerManager().ClearTimer(AimTimeoutTimerHandle);
	}

	// 스킬 종료
	DeactiveSkill();
}

void UGS_MerciAimingSkill::OnSkillAnimationEnd()
{
}

void UGS_MerciAimingSkill::InterruptSkill()
{
	Super::InterruptSkill();

	if (CachedMerciOwner.IsValid())
	{
		CachedMerciOwner->Client_StopZoom(0.0f);
		
		// 타이머 해제 추가 (인터럽트 시에도 정리)
		CachedMerciOwner->GetWorldTimerManager().ClearTimer(AimTimeoutTimerHandle);
	}

	SetIsActive(false);
}

void UGS_MerciAimingSkill::DeactiveSkill()
{
	if (CachedMerciOwner.IsValid())
	{
		CachedMerciOwner->Client_StopZoom(0.0f);
	}

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

	// 타이머 정리
	if (OwnerCharacter)
	{
		OwnerCharacter->GetWorldTimerManager().ClearTimer(AimTimeoutTimerHandle);
	}

	Super::DeactiveSkill();
}
