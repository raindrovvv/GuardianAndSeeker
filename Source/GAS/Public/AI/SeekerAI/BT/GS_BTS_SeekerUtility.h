// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "GS_BTS_SeekerUtility.generated.h"

class AGS_SeekerAIController;

/**
 * 유틸리티 AI 서비스
 * 
 * 유틸리티 점수를 계산하고 블랙보드를 업데이트합니다.
 * 행동 전환 쿨다운으로 떨림 현상을 방지합니다.
 */
UCLASS()
class GAS_API UGS_BTS_SeekerUtility : public UBTService
{
	GENERATED_BODY()

public:
	UGS_BTS_SeekerUtility();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** 행동 상태를 저장할 블랙보드 키 */
	UPROPERTY(EditAnywhere, Category = "블랙보드")
	FBlackboardKeySelector BehaviorKey;

	/** 행동 전환 최소 대기 시간 (초) */
	UPROPERTY(EditAnywhere, Category = "행동 전환", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float BehaviorSwitchCooldown = 0.3f;

	/** 행동 전환에 필요한 점수 차이 (flip-flopping 방지) */
	UPROPERTY(EditAnywhere, Category = "행동 전환", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HysteresisThreshold = 0.08f;

private:
	/** 현재 활성 행동 */
	ESeekerBehavior CurrentActiveBehavior = ESeekerBehavior::Idle;

	/** 행동 전환 가능까지 남은 시간 */
	float SwitchCooldownRemaining = 0.0f;
};
