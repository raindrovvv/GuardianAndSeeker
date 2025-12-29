// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Skill/Seeker/Chan/GS_ChanReadySkill.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Sound/GS_SeekerAudioComponent.h"

UGS_ChanReadySkill::UGS_ChanReadySkill()
{
	CurrentSkillType = ESkillSlot::Ready;
}

void UGS_ChanReadySkill::ActiveSkill()
{
	Super::ActiveSkill();

	CachedChanOwner = Cast<AGS_Chan>(OwnerCharacter);

	if (CachedChanOwner.IsValid())
	{
		// Change Slot
		CachedChanOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::UpperBody);

		CachedChanOwner->Multicast_SetMustTurnInPlace(true);
		CachedChanOwner->SetSeekerGait(EGait::Walk);

		if (UAnimMontage* LoadedMontage = GetCachedMontage(0))
		{
			CachedChanOwner->Multicast_PlaySkillMontage(LoadedMontage);
		}
		CachedChanOwner->CanChangeSeekerGait = false;

		// 스킬 시작 사운드 재생 (멀티캐스트)
		if (UGS_SeekerAudioComponent* AudioComp = CachedChanOwner->SeekerAudioComponent)
		{
			AudioComp->RequestSkillAudio(CurrentSkillType, 0); // 0 = 스킬 시작
		}

		if (OwningComp)
		{
			FVector SkillLocation = OwnerCharacter->GetActorLocation();
			FRotator SkillRotation = OwnerCharacter->GetActorRotation();

			// 스킬 시전 VFX 재생
			OwningComp->Multicast_PlayCastVFX(CurrentSkillType, SkillLocation, SkillRotation);
		}

		// 방어 상태 활성화
		CachedChanOwner->SetDefending(true);

		if (UAnimInstance* AnimInstance = CachedChanOwner->GetMesh()->GetAnimInstance())
		{
			AnimInstance->OnMontageEnded.AddUniqueDynamic(this, &UGS_ChanReadySkill::OnMontageEnded);
			ActiveMontage = TargetMontage; // 추적용 변수
		}

		// 스테미나 이벤트 구독
		CachedChanOwner->OnStaminaDepleted.AddUniqueDynamic(this, &UGS_ChanReadySkill::HandleStaminaDepleted);
	}

	// DeactiveMontageIndex 초기화
	DeactiveMontageIndex = 0;
}

void UGS_ChanReadySkill::OnSkillCanceledByDebuff()
{
	Super::OnSkillCanceledByDebuff();

	// 방어 상태 비활성화
	if (CachedChanOwner.IsValid())
	{
		CachedChanOwner->SetDefending(false);
	}
}

void UGS_ChanReadySkill::OnSkillAnimationEnd()
{
	UE_LOG(LogTemp, Error, TEXT("OnSkillAnimationEnd - ChanReadySkill"));
	Super::OnSkillAnimationEnd();

	if (CachedChanOwner.IsValid())
	{
		// Change Slot
		CachedChanOwner->Multicast_SetMustTurnInPlace(false);
		CachedChanOwner->SetSeekerGait(EGait::Run);
		// Change Slot
		CachedChanOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::None);

		CachedChanOwner->CanChangeSeekerGait = true;

		CachedChanOwner->SetMoveControlValue(true, true);
		CachedChanOwner->SetLookControlValue(true, true);

		// =======================
		// 스킬 종료 VFX 재생
		// =======================

		if (OwningComp)
		{
			FVector SkillLocation = OwnerCharacter->GetActorLocation();
			FRotator SkillRotation = OwnerCharacter->GetActorRotation();

			// 스킬 종료 VFX 재생
			OwningComp->Multicast_PlayEndVFX(CurrentSkillType, SkillLocation, SkillRotation);
		}
	}
}

void UGS_ChanReadySkill::InterruptSkill()
{
	Super::InterruptSkill();

	if (CachedChanOwner.IsValid())
	{
		CachedChanOwner->SetLookControlValue(true, true);
		// 방어 상태 비활성화 (스킬이 중단될 때)
		CachedChanOwner->SetDefending(false);
	}
	
	SetIsActive(false);
}

void UGS_ChanReadySkill::HandleStaminaDepleted(bool bByDamage)
{
	if (bByDamage)
	{
		UE_LOG(LogTemp, Error, TEXT("Stamina 0 - Cause Damage"));
		// 기본 애니메이션
		DeactiveMontageIndex = 1;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Stamina 0 - Cause Drain"));
		// 기본 애니메이션
		DeactiveMontageIndex = 0;
	}
}

void UGS_ChanReadySkill::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!OwnerCharacter) return;

	if (SkillAnimMontages.Contains(Montage))
	{
		FName CurrentMontageName = NAME_None;
		if (UAnimMontage* CurrentMontage = OwnerCharacter->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage())
		{
			CurrentMontageName = CurrentMontage->GetFName();
		}
		UE_LOG(LogTemp, Warning, TEXT("AnimationEnded 현재 애니메이션 몽타주: %s"), *CurrentMontageName.ToString());


		// 애니메이션 종료 처리 (Notify가 빠졌을 경우에도 안전하게)
		UAnimMontage* LoadedMontage1 = GetCachedMontage(1);
		if (Montage == LoadedMontage1)
		{
			OnSkillAnimationEnd();
			if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
			{
				AnimInstance->OnMontageEnded.RemoveDynamic(this, &UGS_ChanReadySkill::OnMontageEnded);
			}
		}

		
	}
}

void UGS_ChanReadySkill::DeactiveSkill()
{
	if (CachedChanOwner.IsValid())
	{
		// Set HitReact
		CachedChanOwner->SetCanHitReact(true);
		CachedChanOwner->CanChangeSeekerGait = true;
		CachedChanOwner->SetSeekerGait(EGait::Run);

		// 애니메이션 재생
		FName SectionName = NAME_None;
		if (DeactiveMontageIndex == 0)
		{
			SectionName = FName("LoopEnd");
		}
		else if (DeactiveMontageIndex == 1)
		{
			CachedChanOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::FullBody);
		}


		if (UAnimMontage* LoadedMontage = GetCachedMontage(DeactiveMontageIndex))
		{
			CachedChanOwner->Multicast_PlaySkillMontage(LoadedMontage, SectionName);
		}

		// 현재 재생 중인 몽타주가 있으면
		FName CurrentMontageName = NAME_None;
		if (UAnimInstance* AnimInstance = CachedChanOwner->GetMesh()->GetAnimInstance())
		{
			if (UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage())
			{
				CurrentMontageName = CurrentMontage->GetFName();
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("현재 애니메이션 몽타주: %s"), *CurrentMontageName.ToString());

		// 방어 상태 비활성화 (스킬 완전 종료 시)
		CachedChanOwner->SetDefending(false);
	}

	// 스킬 상태 업데이트
	Super::DeactiveSkill();
}

