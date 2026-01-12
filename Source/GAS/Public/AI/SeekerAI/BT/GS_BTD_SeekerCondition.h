// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "GS_BTD_SeekerCondition.generated.h"

/**
 * 시커 AI 의사결정을 위한 조건 타입 열거형
 * 
 * 비헤이비어 트리에서 다양한 상황 판단에 사용됩니다.
 * 각 조건은 특정 게임플레이 상태를 확인하여 true/false를 반환합니다.
 */
UENUM(BlueprintType)
enum class ESeekerConditionType : uint8
{
	/** 블랙보드에 공격 대상이 설정되어 있는지 확인 */
	HasTargetEnemy UMETA(DisplayName = "Has Target Enemy"),

	/** 체력이 CustomHealthThreshold(기본 30%) 이하인지 확인 */
	IsLowHealth UMETA(DisplayName = "Is Low Health"),

	/** 체력이 위험 수준(15% 이하)인지 확인 */
	IsCriticalHealth UMETA(DisplayName = "Is Critical Health"),

	/** 근처에 트랩이 있는지 확인 */
	IsNearTrap UMETA(DisplayName = "Is Near Trap"),

	/** 목표 지점에 도달했는지 확인 */
	HasReachedGoal UMETA(DisplayName = "Has Reached Goal"),

	/** 특정 스킬을 사용할 수 있는지 확인 (SkillIndex 필요) */
	CanUseSkill UMETA(DisplayName = "Can Use Skill"),

	/** 힐 포션/스킬을 사용할 수 있는지 확인 */
	CanHeal UMETA(DisplayName = "Can Heal"),

	/** 현재 전투 중인지 확인 */
	IsInCombat UMETA(DisplayName = "Is In Combat"),

	/** 타겟이 RangeThreshold 거리 내에 있는지 확인 */
	IsTargetInRange UMETA(DisplayName = "Is Target In Attack Range"),

	/** 시커가 살아있는지 확인 */
	IsAlive UMETA(DisplayName = "Is Alive"),

	/** 쓰러진 아군이 있는지 확인 (부활 판단용) */
	HasDownedAlly UMETA(DisplayName = "Has Downed Ally"),

	/** 상호작용 가능한 아이템이 근처에 있는지 확인 */
	HasInteractiveItem UMETA(DisplayName = "Has Interactive Item"),

	/** 원거리 시커(Merci)인지 확인 - 엄폐 전술용 */
	IsRanged UMETA(DisplayName = "Is Ranged Seeker"),

	/** 근접 시커(Ares, Chan)인지 확인 - 우회 전술용 */
	IsMelee UMETA(DisplayName = "Is Melee Seeker")
};

/**
 * 시커 AI 조건 데코레이터
 * 
 * 비헤이비어 트리에서 다양한 조건을 검사하는 데코레이터입니다.
 * ConditionType을 선택하여 원하는 조건을 설정할 수 있습니다.
 * 
 * 사용 예시:
 * - Combat 브랜치: IsInCombat + IsTargetInRange
 * - Heal 브랜치: IsLowHealth + CanHeal
 * - 전술 분기: IsRanged(엄폐) / IsMelee(우회)
 */
UCLASS()
class GAS_API UGS_BTD_SeekerCondition : public UBTDecorator
{
	GENERATED_BODY()

public:
	UGS_BTD_SeekerCondition();

	/** 검사할 조건 타입 */
	UPROPERTY(EditAnywhere, Category = "조건")
	ESeekerConditionType ConditionType = ESeekerConditionType::HasTargetEnemy;

	/** 스킬 인덱스 (CanUseSkill 조건에서만 사용) */
	UPROPERTY(EditAnywhere, Category = "조건", meta = (EditCondition = "ConditionType == ESeekerConditionType::CanUseSkill"))
	int32 SkillIndex = 0;

	/** 체력 임계값 (IsLowHealth 조건에서 사용, 0.0~1.0) */
	UPROPERTY(EditAnywhere, Category = "조건", meta = (EditCondition = "ConditionType == ESeekerConditionType::IsLowHealth", ClampMin = "0.0", ClampMax = "1.0"))
	float CustomHealthThreshold = 0.3f;

	/** 공격 사거리(AttackRange)를 거리 임계값으로 사용할지 여부 (IsTargetInRange 전용) */
	UPROPERTY(EditAnywhere, Category = "조건", meta = (EditCondition = "ConditionType == ESeekerConditionType::IsTargetInRange"))
	bool bUseAttackRange = true;

	/** 거리 임계값 (IsTargetInRange 조건에서 사용, bUseAttackRange가 false일 때만 사용됨) */
	UPROPERTY(EditAnywhere, Category = "조건", meta = (EditCondition = "ConditionType == ESeekerConditionType::IsTargetInRange && !bUseAttackRange"))
	float RangeThreshold = 200.0f;

	/** 타겟 액터를 저장하는 블랙보드 키 */
	UPROPERTY(EditAnywhere, Category = "블랙보드")
	FBlackboardKeySelector TargetActorKey;

protected:
	/** 실제 조건 평가 로직 - ConditionType에 따라 적절한 체크 함수 호출 */
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

	/** 에디터에 표시할 설명 문자열 생성 */
	virtual FString GetStaticDescription() const override;

private:
	// ========================================
	// 조건 체크 함수들
	// 각 함수는 특정 조건을 검사하여 bool을 반환
	// ========================================

	/** 블랙보드에 타겟 적이 설정되어 있는지 확인 */
	bool CheckHasTargetEnemy(UBehaviorTreeComponent& OwnerComp) const;

	/** 체력이 CustomHealthThreshold 이하인지 확인 */
	bool CheckIsLowHealth(UBehaviorTreeComponent& OwnerComp) const;

	/** 체력이 15% 이하(위험 수준)인지 확인 */
	bool CheckIsCriticalHealth(UBehaviorTreeComponent& OwnerComp) const;

	/** 블랙보드에 근처 트랩이 설정되어 있는지 확인 */
	bool CheckIsNearTrap(UBehaviorTreeComponent& OwnerComp) const;

	/** 목표 지점 도달 여부 확인 */
	bool CheckHasReachedGoal(UBehaviorTreeComponent& OwnerComp) const;

	/** 특정 스킬 사용 가능 여부 확인 */
	bool CheckCanUseSkill(UBehaviorTreeComponent& OwnerComp) const;

	/** 힐 사용 가능 여부 확인 */
	bool CheckCanHeal(UBehaviorTreeComponent& OwnerComp) const;

	/** 전투 상태 여부 확인 */
	bool CheckIsInCombat(UBehaviorTreeComponent& OwnerComp) const;

	/** 타겟이 공격 사거리 내에 있는지 확인 */
	bool CheckIsTargetInRange(UBehaviorTreeComponent& OwnerComp) const;

	/** 시커 생존 여부 확인 */
	bool CheckIsAlive(UBehaviorTreeComponent& OwnerComp) const;

	/** 쓰러진 아군 존재 여부 확인 */
	bool CheckHasDownedAlly(UBehaviorTreeComponent& OwnerComp) const;

	/** 근처 상호작용 아이템 존재 여부 확인 */
	bool CheckHasInteractiveItem(UBehaviorTreeComponent& OwnerComp) const;

	/** 원거리 시커(Merci) 여부 확인 - 엄폐 전술에 사용 */
	bool CheckIsRanged(UBehaviorTreeComponent& OwnerComp) const;

	/** 근접 시커(Ares, Chan) 여부 확인 - 우회 전술에 사용 */
	bool CheckIsMelee(UBehaviorTreeComponent& OwnerComp) const;
};
