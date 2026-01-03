#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Character/Skill/ESkill.h"
#include "GS_ANS_CancelWindow.generated.h"

/**
 * 특정 애니메이션 구간 동안 다른 액션(회피, 스킬 등)으로 캔슬할 수 있게 해주는 노티파이 스테이트.
 */
UCLASS()
class GAS_API UGS_ANS_CancelWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 이 구간 동안 사용 가능한 스킬 슬롯 목록 (예: Rolling) */
	UPROPERTY(EditAnywhere, Category = "Cancel")
	TArray<ESkillSlot> CancellableSkills;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
