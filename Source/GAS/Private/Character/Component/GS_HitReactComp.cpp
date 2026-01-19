// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Character/Component/GS_HitReactComp.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Character/Player/GS_Player.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Weapon/Equipable/GS_WeaponEquipable.h"
#include "Engine/World.h"

UGS_HitReactComp::UGS_HitReactComp()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Pre-initialize the hit reaction montage array
	HitReactMontages.Init(nullptr, static_cast<int32>(EHitReactType::TypeNum));
}

void UGS_HitReactComp::PlayHitReact(EHitReactType ReactType, FVector HitDirection)
{
	AGS_Player* OwnerCharacter = Cast<AGS_Player>(GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	AGS_Seeker* OwnerSeeker = Cast<AGS_Seeker>(OwnerCharacter);
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const FName HitSection = CalculateHitDirection(HitDirection);

	// Implement Hit Reaction Cooldown for 'Interrupt' types
	// If hit again too soon, downgrade to 'DamageOnly' to prevent infinite stun-locking
	if (ReactType == EHitReactType::Interrupt && (CurrentTime - LastHitReactTimestamp) < HitReactCooldownSeconds)
	{
		ReactType = EHitReactType::DamageOnly;
	}

	// Check for Super Armor (e.g., during Ultimate skills)
	bool bIsSuperArmorActive = false;
	if (OwnerSeeker && OwnerSeeker->GetSkillComp())
	{
		if (OwnerSeeker->GetSkillComp()->IsSkillActive(ESkillSlot::Ultimate))
		{
			bIsSuperArmorActive = true;
		}
	}

	// Handle different reaction behaviors
	switch (ReactType)
	{
		case EHitReactType::Interrupt:
		{
			if (OwnerSeeker && !bIsSuperArmorActive)
			{
				// Cancel current skills and force into the full-body reaction slot
				OwnerSeeker->GetSkillComp()->SkillsInterrupt();
				OwnerSeeker->Multicast_SetMontageSlot(ESeekerMontageSlot::FullBody);

				if (UAnimMontage* HitMontage = HitReactMontages[static_cast<int32>(EHitReactType::Interrupt)])
				{
					OwnerCharacter->Multicast_PlaySkillMontage(HitMontage, HitSection);

					// Setup end delegate to restore state after the stagger
					if (UGS_SeekerAnimInstance* AnimInstance =
							Cast<UGS_SeekerAnimInstance>(OwnerSeeker->GetMesh()->GetAnimInstance()))
					{
						HitReactEndDelegate.BindUObject(this, &UGS_HitReactComp::HandleHitReactEnded);
						AnimInstance->Montage_SetEndDelegate(HitReactEndDelegate, HitMontage);
					}
				}

				LastHitReactTimestamp = CurrentTime;
			}

			// Globally disable hit reaction for a duration to allow a 'breathing' window
			OwnerCharacter->DisableHitReact(3.0f);
			break;
		}

		case EHitReactType::Additive:
		{
			// Light reactions that reset the gait but don't cancel skills
			if (OwnerSeeker && !bIsSuperArmorActive)
			{
				OwnerSeeker->StateReset();
				OwnerSeeker->SetSeekerGait(EGait::Run);
			}
			break;
		}

		case EHitReactType::DamageOnly:
		default:
			// No animation change for purely damage-based events
			break;
	}

	// Automatic draw/aim cancellation for bow users, unless in super armor or minor hit
	if (OwnerSeeker && !bIsSuperArmorActive && ReactType != EHitReactType::DamageOnly)
	{
		OwnerSeeker->SetAimState(false);
		OwnerSeeker->SetDrawState(false);
	}
}

void UGS_HitReactComp::StopHitReact(UAnimMontage* TargetMontage)
{
	if (AGS_Player* OwnerCharacter = Cast<AGS_Player>(GetOwner()))
	{
		OwnerCharacter->Multicast_StopSkillMontage(TargetMontage);
	}
}

FName UGS_HitReactComp::CalculateHitDirection(FVector HitDirection)
{
	AGS_Player* OwnerCharacter = Cast<AGS_Player>(GetOwner());
	if (!OwnerCharacter || HitDirection.IsNearlyZero())
	{
		return FName("Front");
	}

	// Calculate hit quadrant based on actor's forward and right vectors
	const FVector Forward = OwnerCharacter->GetActorForwardVector();
	const FVector Right = OwnerCharacter->GetActorRightVector();

	const float ForwardDot = FVector::DotProduct(Forward, HitDirection);
	const float RightDot = FVector::DotProduct(Right, HitDirection);

	if (ForwardDot > 0.7f)
		return FName("Front");
	if (ForwardDot < -0.7f)
		return FName("Back");

	return (RightDot > 0.0f) ? FName("Right") : FName("Left");
}

void UGS_HitReactComp::HandleHitReactEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// Interrupted hit reactions (by skills like Roll) shouldn't force-reset the character state
	if (bInterrupted)
	{
		return;
	}

	AGS_Character* OwnerCharacter = Cast<AGS_Character>(GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	// 1. Safety: Disable hitboxes for all weapon slots to prevent active hitboxes during recovery
	for (int32 i = 0; i < 5; ++i)
	{
		if (AGS_WeaponEquipable* Weapon = Cast<AGS_WeaponEquipable>(OwnerCharacter->GetWeaponByIndex(i)))
		{
			Weapon->ForceDisableHit();
		}
	}

	// 2. Specialized seeker recovery
	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(OwnerCharacter))
	{
		Seeker->StateReset();
		Seeker->SetSeekerGait(EGait::Run);
	}

	// 3. Broadcast completion for external systems (e.g., AI)
	OnHitReactEnd.Broadcast(Montage, bInterrupted);
}

void UGS_HitReactComp::BeginPlay()
{
	Super::BeginPlay();
}

void UGS_HitReactComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HitReactEndDelegate.Unbind();
	Super::EndPlay(EndPlayReason);
}
