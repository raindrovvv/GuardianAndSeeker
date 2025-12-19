#include "Animation/Notifies/GS_ANS_DrakharDash.h"

#include "Character/Player/Guardian/GS_Drakhar.h"
#include "Character/Skill/GS_SkillComp.h"

void UGS_ANS_DrakharDash::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (AActor* Owner = MeshComp->GetOwner())
	{
		if (AGS_Drakhar* Drakhar = Cast<AGS_Drakhar>(Owner))
		{
			// Server RPC는 로컬에서 조종하는 캐릭터에서만 호출해야 함
			// IsLocallyControlled: Autonomous Proxy (클라이언트 자신) 또는 Listen Server의 자신 캐릭터
			// Simulated Proxy는 제외됨 (다른 플레이어들이 보는 복제본)
			if (Drakhar->IsLocallyControlled())
			{
				Drakhar->ServerRPCCalculateDashLocation();
			}
		}
	}
}

void UGS_ANS_DrakharDash::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (AActor* Owner = MeshComp->GetOwner())
	{
		if (AGS_Drakhar* Drakhar = Cast<AGS_Drakhar>(Owner))
		{
			// Server RPC는 로컬에서 조종하는 캐릭터에서만 호출
			// (Simulated Proxy에서 중복 호출 방지)
			if (!Drakhar->IsLocallyControlled())
			{
				return;
			}
			Drakhar->ServerRPCDoDash(FrameDeltaTime);
		}
	}
}

void UGS_ANS_DrakharDash::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	
	if (AActor* Owner = MeshComp->GetOwner())
	{
		if (AGS_Drakhar* Drakhar = Cast<AGS_Drakhar>(Owner))
		{
			// Server RPC는 로컬에서 조종하는 캐릭터에서만 호출
			// (Simulated Proxy에서 중복 호출 방지)
			if (!Drakhar->IsLocallyControlled())
			{
				return;
			}
			Drakhar->ServerRPCEndDash();
		}
	}
	
}

