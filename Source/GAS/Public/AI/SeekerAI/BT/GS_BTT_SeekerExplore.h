// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GS_BTT_SeekerExplore.generated.h"

/**
 * 시커 탐색 BT 태스크
 * 
 * 던전 탐험 시 새로운 탐색 포인트를 찾아 이동합니다.
 * 네비게이션 메시 위의 도달 가능한 위치를 탐색합니다.
 */
UCLASS()
class GAS_API UGS_BTT_SeekerExplore : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UGS_BTT_SeekerExplore();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// ========================================
	// 탐색 설정
	// ========================================

	/** 탐색 반경 */
	UPROPERTY(EditAnywhere, Category = "탐색")
	float ExplorationRadius = 2000.0f;

	/** 도착 인정 반경 */
	UPROPERTY(EditAnywhere, Category = "탐색")
	float AcceptableRadius = 100.0f;

	/** 방문하지 않은 지역 우선 탐색 여부 */
	UPROPERTY(EditAnywhere, Category = "탐색")
	bool bPreferUnvisitedAreas = true;

protected:
	virtual FString GetStaticDescription() const override;

private:
	/** 연속 이동 실패 횟수 (3회 이상 시 타겟 리셋) */
	mutable int32 ConsecutiveFailures = 0;
};
