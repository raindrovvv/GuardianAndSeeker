// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_AN_ChanSwitchingAxeSlot.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Components/SkeletalMeshComponent.h"

UGS_AN_ChanSwitchingAxeSlot::UGS_AN_ChanSwitchingAxeSlot()
{
}

void UGS_AN_ChanSwitchingAxeSlot::Notify(USkeletalMeshComponent* MeshComp,
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

	// This logic handles mesh attachment changes during animation.
	// Typically implemented in character classes or handled here directly if simple.
	// Note: Implementation depends on how Chan's weapon system is exposed via Seeker interface.

	// Implementation placeholder for specific mesh switching logic
	// e.g., Seeker->SwitchWeaponSocket(TargetSocketState);
}
