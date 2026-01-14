// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Skill/Seeker/GS_SeekerRollSkill.h"

#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Skill/Seeker/GS_HealSkill.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sound/GS_SeekerAudioComponent.h"

UGS_SeekerRollSkill::UGS_SeekerRollSkill()
{
	CurrentSkillType = ESkillSlot::Rolling;
}

void UGS_SeekerRollSkill::ActiveSkill()
{
	Super::ActiveSkill();

	UE_LOG(LogTemp, Warning, TEXT("SeekerRollSkill")); // SJE
	
	CachedSeekerOwner = Cast<AGS_Seeker>(OwnerCharacter);

	if (CachedSeekerOwner.IsValid())
	{
		if (CachedSeekerOwner->HasAuthority())
		{
			CachedSeekerOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::FullBody);
			CachedSeekerOwner->CanChangeSeekerGait = false;

			const FName RollDirection = CalRollDirection();
			UAnimMontage* AM_Roll = GetCachedMontage(0);
			if (AM_Roll)
			{
				if (RollDirection == FName("00"))
				{
					CachedSeekerOwner->Multicast_PlaySkillMontage(AM_Roll, FName("F0"));
				}
				else
				{
					CachedSeekerOwner->Multicast_PlaySkillMontage(AM_Roll, RollDirection);
				}
			}
			
			EndDelegate.BindUObject(this, &UGS_SeekerRollSkill::OnRollMontageEnded);
			UGS_SeekerAnimInstance* SeekerAnimInstance = Cast<UGS_SeekerAnimInstance>(OwnerCharacter->GetMesh()->GetAnimInstance());
			if (SeekerAnimInstance)
			{
				SeekerAnimInstance->Montage_SetEndDelegate(EndDelegate, AM_Roll);
			}

			// Ignore collision with all pawns (Seekers, Monsters, etc.) during roll
			CachedSeekerOwner->Multicast_SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

			// Also disable collision in CharacterMovement to prevent physics-based collisions
			if (UCharacterMovementComponent* MoveComp = CachedSeekerOwner->GetCharacterMovement())
			{
				MoveComp->SetAvoidanceEnabled(false);
			}

			// 스킬 시작 사운드 재생 (멀티캐스트)
			if (UGS_SeekerAudioComponent* AudioComp = CachedSeekerOwner->SeekerAudioComponent)
			{
				AudioComp->RequestSkillAudio(CurrentSkillType, 0);
			}
		}
		StartCoolDown();
	}
}

void UGS_SeekerRollSkill::OnSkillCanceledByDebuff()
{
	Super::OnSkillCanceledByDebuff();
}

void UGS_SeekerRollSkill::OnSkillAnimationEnd()
{
	Super::OnSkillAnimationEnd();
}

void UGS_SeekerRollSkill::InterruptSkill()
{
	Super::InterruptSkill();
}

void UGS_SeekerRollSkill::OnRollMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (CachedSeekerOwner.IsValid())
	{
		UGS_HealSkill* HealSkill = Cast<UGS_HealSkill>(CachedSeekerOwner->GetSkillComp()->GetSkillFromSkillMap(ESkillSlot::HealPotion));
		if (HealSkill)
		{
			UAnimMontage* AM_Wielding = HealSkill->GetCachedMontage(2);

			CachedSeekerOwner->TransWeaponHandingState(
			EWeaponHandlingState::Sheathing,
			EWeaponHandlingState::Wielding,
			AM_Wielding,
			ESeekerMontageSlot::UpperBody);
		}

		// Restore collision after roll
		CachedSeekerOwner->Multicast_SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

		// Re-enable avoidance in CharacterMovement
		if (UCharacterMovementComponent* MoveComp = CachedSeekerOwner->GetCharacterMovement())
		{
			MoveComp->SetAvoidanceEnabled(true);
		}
	}

	DeactiveSkill();
}
