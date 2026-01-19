// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/GS_AN_SetState.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/GS_TpsController.h"
#include "Character/Skill/GS_SkillComp.h"

UGS_AN_SetState::UGS_AN_SetState()
{
}

void UGS_AN_SetState::Notify(USkeletalMeshComponent* MeshComp,
							 UAnimSequenceBase* Animation,
							 const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	AGS_Seeker* Seeker = Cast<AGS_Seeker>(MeshComp->GetOwner());
	if (!Seeker)
	{
		return;
	}

	// Update Montage Slot (Server-side Multicast)
	if (TargetMontageSlot != ESeekerMontageSlot::End)
	{
		if (Seeker->HasAuthority())
		{
			Seeker->Multicast_SetMontageSlot(TargetMontageSlot);
		}
	}

	// Update Gait Change permission
	if (Seeker->CanChangeSeekerGait != bAllowGaitChange)
	{
		Seeker->CanChangeSeekerGait = bAllowGaitChange;
	}

	// Update Combo Input permission
	if (Seeker->CanAcceptComboInput != bAllowComboInput)
	{
		Seeker->CanAcceptComboInput = bAllowComboInput;
	}

	// Update Control Values via Controller
	if (bOverrideControlValues)
	{
		if (AGS_TpsController* TpsController = Cast<AGS_TpsController>(Seeker->GetController()))
		{
			TpsController->SetMoveControlValue(ControlSettings.bCanMoveRight, ControlSettings.bCanMoveForward);
			TpsController->SetLookControlValue(ControlSettings.bCanLookRight, ControlSettings.bCanLookUp);
		}
	}

	// Apply New Gait
	if (bApplyNewGait)
	{
		Seeker->SetSeekerGait(NewGait);
	}

	// Reset Allowed Skills
	if (bTriggerSkillReset)
	{
		if (UGS_SkillComp* SkillComponent = Seeker->GetSkillComp())
		{
			SkillComponent->ResetAllowedSkillsMask();
		}
	}
}
