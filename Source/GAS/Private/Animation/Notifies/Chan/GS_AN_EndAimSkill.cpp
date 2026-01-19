// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_AN_EndAimSkill.h"

UGS_AN_EndAimSkill::UGS_AN_EndAimSkill()
{
}

void UGS_AN_EndAimSkill::Notify(USkeletalMeshComponent* MeshComp,
								UAnimSequenceBase* Animation,
								const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// This notify is currently deprecated and performs no logic.
}