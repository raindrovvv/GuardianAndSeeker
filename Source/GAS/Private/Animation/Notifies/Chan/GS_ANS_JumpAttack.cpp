// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_ANS_JumpAttack.h"

UGS_ANS_JumpAttack::UGS_ANS_JumpAttack()
{
}

void UGS_ANS_JumpAttack::NotifyBegin(USkeletalMeshComponent* MeshComp,
									 UAnimSequenceBase* Animation,
									 float TotalDuration,
									 const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
}

void UGS_ANS_JumpAttack::NotifyEnd(USkeletalMeshComponent* MeshComp,
								   UAnimSequenceBase* Animation,
								   const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}
