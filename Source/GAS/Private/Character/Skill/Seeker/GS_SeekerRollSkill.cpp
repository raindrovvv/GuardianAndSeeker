// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Character/Skill/Seeker/GS_SeekerRollSkill.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Skill/Seeker/GS_HealSkill.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sound/GS_SeekerAudioComponent.h"

UGS_SeekerRollSkill::UGS_SeekerRollSkill()
{
	CurrentSkillType = ESkillSlot::Rolling;
}

void UGS_SeekerRollSkill::ActiveSkill()
{
	Super::ActiveSkill();

	CachedSeeker = Cast<AGS_Seeker>(OwnerCharacter);
	if (!CachedSeeker.IsValid())
	{
		return;
	}

	// Rolling logic is orchestrated by the server to ensure synchronized collision and movement
	if (CachedSeeker->HasAuthority())
	{
		// Force the character into the full-body montage slot and lock gait changes
		CachedSeeker->Multicast_SetMontageSlot(ESeekerMontageSlot::FullBody);
		CachedSeeker->CanChangeSeekerGait = false;

		// Select the appropriate roll animation segment based on current movement direction
		const FName TargetRollSection = CalRollDirection();
		UAnimMontage* RollMontage = GetCachedMontage(0);

		if (RollMontage)
		{
			// Default to forward ("F0") if no direction is specified/calculated
			const FName SectionToPlay = (TargetRollSection == FName("00")) ? FName("F0") : TargetRollSection;
			CachedSeeker->Multicast_PlaySkillMontage(RollMontage, SectionToPlay);

			// Subscribe to montage completion for cleanup
			RollEndDelegate.BindUObject(this, &UGS_SeekerRollSkill::HandleRollMontageEnded);
			if (UGS_SeekerAnimInstance* AnimInstance =
					Cast<UGS_SeekerAnimInstance>(CachedSeeker->GetMesh()->GetAnimInstance()))
			{
				AnimInstance->Montage_SetEndDelegate(RollEndDelegate, RollMontage);
			}
		}

		// Disable collision with other pawns to allow "rolling through" enemies/allies
		CachedSeeker->Multicast_SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

		// Disable RVO avoidance to prevent jitter during the scripted roll movement
		if (UCharacterMovementComponent* MoveComp = CachedSeeker->GetCharacterMovement())
		{
			MoveComp->SetAvoidanceEnabled(false);
		}

		// Trigger skill activation audio
		if (UGS_SeekerAudioComponent* AudioComponent = CachedSeeker->FindComponentByClass<UGS_SeekerAudioComponent>())
		{
			AudioComponent->Multicast_RequestSkillAudio(CurrentSkillType, 0, CachedSeeker->GetActorLocation());
		}
	}

	// Cooldown starts immediately upon activation attempt
	StartCoolDown();
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

void UGS_SeekerRollSkill::HandleRollMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!CachedSeeker.IsValid())
	{
		DeactiveSkill();
		return;
	}

	// Post-roll weapon handling logic (ensure weapon is correctly wielded/sheathed)
	if (UGS_SkillComp* SkillComponent = CachedSeeker->GetSkillComp())
	{
		if (UGS_HealSkill* HealSkill =
				Cast<UGS_HealSkill>(SkillComponent->GetSkillFromSkillMap(ESkillSlot::HealPotion)))
		{
			// Montage Index 2 corresponds to the wielding/unholstering animation
			UAnimMontage* WieldingMontage = HealSkill->GetCachedMontage(2);

			CachedSeeker->TransWeaponHandingState(EWeaponHandlingState::Sheathing,
												  EWeaponHandlingState::Wielding,
												  WieldingMontage,
												  ESeekerMontageSlot::UpperBody);
		}
	}

	// Restore pawn-to-pawn collision settings
	CachedSeeker->Multicast_SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	// Re-enable RVO avoidance
	if (UCharacterMovementComponent* MovementComponent = CachedSeeker->GetCharacterMovement())
	{
		MovementComponent->SetAvoidanceEnabled(true);
	}

	// Terminate active skill state
	DeactiveSkill();
}
