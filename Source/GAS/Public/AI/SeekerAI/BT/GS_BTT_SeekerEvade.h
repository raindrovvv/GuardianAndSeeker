// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GS_BTT_SeekerEvade.generated.h"

/**
 * BT 태스크: 트랩 및 위험 지역 회피
 * 
 * 구르기, 재배치, 위험 회피를 처리합니다.
 * 트랩 밀집 지역에서 떨림 현상을 방지하기 위해 쿨다운을 적용합니다.
 */
UCLASS()
class GAS_API UGS_BTT_SeekerEvade : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UGS_BTT_SeekerEvade();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	/** 회피 거리 (cm) */
	UPROPERTY(EditAnywhere, Category = "회피")
	float EvadeDistance = 400.0f;

	/** 걷기보다 구르기 우선 사용 */
	UPROPERTY(EditAnywhere, Category = "회피")
	bool bPreferRollOverWalk = true;

	/** 구르기 쿨다운 (초) */
	UPROPERTY(EditAnywhere, Category = "회피")
	float RollCooldown = 2.0f;

	/** 가능하면 회피 스킬 사용 */
	UPROPERTY(EditAnywhere, Category = "회피")
	bool bUseEvadeSkillIfAvailable = true;

	/** 블랙보드 트랩 위치 키 */
	UPROPERTY(EditAnywhere, Category = "회피|블랙보드")
	FBlackboardKeySelector TrapLocationKey;

protected:
	virtual FString GetStaticDescription() const override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FGS_BTTEvadeMemory); }

private:
	/** 태스크 메모리 구조체 */
	struct FGS_BTTEvadeMemory
	{
		float StartTime;
	};

	float LastRollTime = 0.0f;
	float LastEvadeTaskFinishTime = 0.0f;
	const float GlobalEvadeCooldown = 1.5f;

	/** 안전한 회피 방향 계산 */
	FVector CalculateSafeEvadeDirection(const FVector& ThreatLocation, const FVector& CurrentLocation) const;
};
