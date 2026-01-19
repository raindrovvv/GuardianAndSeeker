// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Character/Skill/Seeker/GS_HealSkill.h"
#include "Character/Player/GS_Player.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Props/Item/SeekerItem/GS_HP_Potion.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Net/UnrealNetwork.h"

UGS_HealSkill::UGS_HealSkill()
{
	HealAmountValue = 200.0f;
	MaxHealthPotions = 5;
	CurrentHealCount = MaxHealthPotions;
	bIsActivationBlocked = false;
}

void UGS_HealSkill::ActiveSkill()
{
	Super::ActiveSkill();

	// Primary skill execution logic is server-side
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	if (!CanActivateHealLogic())
	{
		NotifyHealBlocked();
		return;
	}

	CachedSeeker = Cast<AGS_Seeker>(OwnerCharacter);

	// Start healing montage (Index 0 is the drink animation)
	if (UAnimMontage* DrinkMontage = GetCachedMontage(0))
	{
		OwnerCharacter->Multicast_PlaySkillMontage(DrinkMontage);

		if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
		{
			AnimInstance->OnMontageEnded.AddUniqueDynamic(this, &UGS_HealSkill::HandleHealMontageEnded);
		}
	}

	bIsCoolingDown = true;

	if (CachedSeeker.IsValid())
	{
		// Force the character into a walking state and upper-body rotation during consumption
		CachedSeeker->Multicast_SetMontageSlot(ESeekerMontageSlot::UpperBody);
		CachedSeeker->Server_SetSeekerGait(EGait::Walk);
	}
}

void UGS_HealSkill::DeactiveSkill()
{
	Super::DeactiveSkill();

	bIsCoolingDown = false;

	// Reset skill permissions once consumption is complete
	if (OwnerCharacter && OwnerCharacter->HasAuthority() && CachedSeeker.IsValid())
	{
		if (UGS_SkillComp* SkillComp = CachedSeeker->GetSkillComp())
		{
			SkillComp->ResetAllowedSkillsMask();
		}
	}
}

void UGS_HealSkill::InterruptSkill()
{
	Super::InterruptSkill();

	if (!OwnerCharacter || !CachedSeeker.IsValid() || CachedSeeker->IsDead())
	{
		return;
	}

	// Cleanup on interrupt (e.g., getting hit or staggared while drinking)
	CachedSeeker->Multicast_SetMontageSlot(ESeekerMontageSlot::None);
	CachedSeeker->SetMoveControlValue(true, true);

	SetIsActive(false);
	bIsCoolingDown = false;

	// Visually drop the potion if currently held
	if (AGS_HP_Potion* Potion = Cast<AGS_HP_Potion>(CachedSeeker->GetItem(EItemType::HP_Potion)))
	{
		Potion->ReleaseFromHolder();

		if (UStaticMeshComponent* PotionMesh = Potion->GetVisualMesh())
		{
			PotionMesh->SetSimulatePhysics(true);
			PotionMesh->SetEnableGravity(true);
			PotionMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}

	if (UGS_SkillComp* SkillComp = CachedSeeker->GetSkillComp())
	{
		SkillComp->ResetAllowedSkillsMask();
	}
}

bool UGS_HealSkill::CanActive() const
{
	if (!Super::CanActive())
	{
		return false;
	}

	// Can only trigger if charges are available and health is not full
	return HasPotionsRemaining() && !IsCharacterHealthFull();
}

void UGS_HealSkill::SetCurrentHealCount(int32 NewCount)
{
	const int32 OldCount = CurrentHealCount;
	CurrentHealCount = FMath::Clamp(NewCount, 0, MaxHealthPotions);

	// If we acquired new potions, unblock the skill
	if (OldCount == 0 && CurrentHealCount > 0)
	{
		bIsActivationBlocked = false;
		SetCoolingDown(false);
	}

	// Broadcast updates to clients for UI synchronization
	if (OwningComp && OwnerCharacter && OwnerCharacter->HasAuthority())
	{
		OwningComp->Client_BroadcastHealCountChanged(CurrentSkillType, CurrentHealCount, MaxHealthPotions);
	}
}

bool UGS_HealSkill::IsCharacterHealthFull() const
{
	if (OwnerCharacter)
	{
		if (UGS_StatComp* StatComp = OwnerCharacter->GetStatComp())
		{
			return StatComp->GetCurrentHealth() >= StatComp->GetMaxHealth();
		}
	}
	return false;
}

bool UGS_HealSkill::CanActivateHealLogic() const
{
	if (bIsCoolingDown)
	{
		return false;
	}

	return HasPotionsRemaining() && !IsCharacterHealthFull();
}

void UGS_HealSkill::NotifyHealBlocked()
{
	// Trigger UI/feedback logic on the client
	bIsActivationBlocked = true;

	if (OwningComp)
	{
		OwningComp->Client_BroadcastSkillCooldownBlocked_Implementation(CurrentSkillType);
	}
}

void UGS_HealSkill::InitializeDelegate()
{
	Super::InitializeDelegate();

	if (OwnerCharacter)
	{
		OwnerCharacter->OnTakeAnyDamage.AddDynamic(this, &UGS_HealSkill::HandleOwnerDamaged);

		if (!CachedSeeker.IsValid())
		{
			CachedSeeker = Cast<AGS_Seeker>(OwnerCharacter);
		}
	}
}

void UGS_HealSkill::HandleHealMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	UAnimMontage* HealMontage = GetCachedMontage(0);
	if (!HealMontage || Montage != HealMontage)
	{
		return;
	}

	// Unsubscribe from completion delegate
	if (OwnerCharacter && OwnerCharacter->GetMesh())
	{
		if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
		{
			AnimInstance->OnMontageEnded.RemoveDynamic(this, &UGS_HealSkill::HandleHealMontageEnded);
		}
	}

	DeactiveSkill();
}

void UGS_HealSkill::ConsumeHealCharge()
{
	if (CurrentHealCount > 0)
	{
		CurrentHealCount--;
	}
}

void UGS_HealSkill::OnRep_CurrentHealCount()
{
	// Replicating potion count (client-side broadcast logic could be added here if needed)
}

void UGS_HealSkill::HandleOwnerDamaged(AActor* DamagedActor,
									   float DamageAmount,
									   const class UDamageType* DamageType,
									   class AController* InstigatedBy,
									   AActor* DamageCauser)
{
	// If the character was previously at full health but just took damage, allow healing again
	if (bIsActivationBlocked && !IsCharacterHealthFull())
	{
		bIsActivationBlocked = false;
		SetCoolingDown(false);
	}
}

void UGS_HealSkill::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGS_HealSkill, CurrentHealCount);
}