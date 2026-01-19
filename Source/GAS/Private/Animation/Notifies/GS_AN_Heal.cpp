// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/GS_AN_Heal.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Skill/Seeker/GS_HealSkill.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Sound/GS_SeekerAudioComponent.h"

UGS_AN_Heal::UGS_AN_Heal()
{
}

void UGS_AN_Heal::Notify(USkeletalMeshComponent* MeshComp,
						 UAnimSequenceBase* Animation,
						 const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	AGS_Seeker* Seeker = Cast<AGS_Seeker>(MeshComp->GetOwner());
	if (!Seeker || Seeker->IsDead())
	{
		return;
	}

	UGS_StatComp* StatComponent = Seeker->GetStatComp();
	UGS_SkillComp* SkillComponent = Seeker->GetSkillComp();

	if (!StatComponent || !SkillComponent)
	{
		return;
	}

	// Retrieve the heal skill instance from the skill component
	UGS_HealSkill* HealSkill = Cast<UGS_HealSkill>(SkillComponent->GetSkillFromSkillMap(ESkillSlot::HealPotion));
	if (!HealSkill)
	{
		return;
	}

	// Healing logic is handled on the server
	if (Seeker->HasAuthority())
	{
		// Calculate and apply health restoration
		const float RestoreAmount = HealSkill->GetHealAmountValue();
		const float CurrentHP = StatComponent->GetCurrentHealth();
		const float MaxHP = StatComponent->GetMaxHealth();

		StatComponent->SetCurrentHealth(FMath::Min(CurrentHP + RestoreAmount, MaxHP), true);

		// Consume potion count
		HealSkill->ConsumeHealCharge();

		// Trigger visual effects via multicast
		if (HealSkill->SkillCastVFX)
		{
			SkillComponent->Multicast_PlayCastVFX(
				HealSkill->CurrentSkillType, Seeker->GetActorLocation(), Seeker->GetActorRotation());
		}

		if (HealSkill->SkillImpactVFX)
		{
			SkillComponent->Multicast_PlayImpactVFX(HealSkill->CurrentSkillType, Seeker->GetActorLocation());
		}

		// Sync heal count to client UI
		SkillComponent->Client_BroadcastHealCountChanged(
			HealSkill->CurrentSkillType, HealSkill->GetCurrentHealCount(), HealSkill->GetMaxHealCount());

		// Trigger sound effects
		if (UGS_SeekerAudioComponent* AudioComponent = Seeker->FindComponentByClass<UGS_SeekerAudioComponent>())
		{
			// Event type 0 corresponds to the start of the heal sound
			AudioComponent->Multicast_RequestSkillAudio(HealSkill->CurrentSkillType, 0, Seeker->GetActorLocation());
		}
	}
}
