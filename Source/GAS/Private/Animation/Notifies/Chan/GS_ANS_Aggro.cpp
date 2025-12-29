// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/Notifies/Chan/GS_ANS_Aggro.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Character/Skill/GS_SkillBase.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"

void UGS_ANS_Aggro::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
                                const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
}

void UGS_ANS_Aggro::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (AGS_Chan* Character = Cast<AGS_Chan>(MeshComp->GetOwner()))
	{
		// Server RPC는 로컬에서 조종하는 캐릭터에서만 호출
		// (Simulated Proxy에서 중복 호출 방지)
		if (Character->IsLocallyControlled())
		{
			Character->GetSkillComp()->Server_TrySkillAnimationEnd(ESkillSlot::Moving);
		}
	}
}
