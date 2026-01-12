// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/BT/GS_BTD_SeekerCondition.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "AI/SeekerAI/GS_AISeeker.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Seeker/GS_Ares.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Character/Player/Seeker/GS_Merci.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Skill/GS_SkillComp.h"

UGS_BTD_SeekerCondition::UGS_BTD_SeekerCondition()
{
	NodeName = "Seeker Condition";

	// 기본 블랙보드 키 설정 - TargetEnemy를 기본값으로 사용
	TargetActorKey.SelectedKeyName = AGS_SeekerAIController::TargetEnemyKey;
}

// ========================================
// 메인 조건 평가 함수
// ConditionType에 따라 적절한 체크 함수를 호출합니다.
// ========================================
bool UGS_BTD_SeekerCondition::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	switch (ConditionType)
	{
	case ESeekerConditionType::HasTargetEnemy:
		return CheckHasTargetEnemy(OwnerComp);
	case ESeekerConditionType::IsLowHealth:
		return CheckIsLowHealth(OwnerComp);
	case ESeekerConditionType::IsCriticalHealth:
		return CheckIsCriticalHealth(OwnerComp);
	case ESeekerConditionType::IsNearTrap:
		return CheckIsNearTrap(OwnerComp);
	case ESeekerConditionType::HasReachedGoal:
		return CheckHasReachedGoal(OwnerComp);
	case ESeekerConditionType::CanUseSkill:
		return CheckCanUseSkill(OwnerComp);
	case ESeekerConditionType::CanHeal:
		return CheckCanHeal(OwnerComp);
	case ESeekerConditionType::IsInCombat:
		return CheckIsInCombat(OwnerComp);
	case ESeekerConditionType::IsTargetInRange:
		return CheckIsTargetInRange(OwnerComp);
	case ESeekerConditionType::IsAlive:
		return CheckIsAlive(OwnerComp);
	case ESeekerConditionType::HasDownedAlly:
		return CheckHasDownedAlly(OwnerComp);
	case ESeekerConditionType::HasInteractiveItem:
		return CheckHasInteractiveItem(OwnerComp);
	case ESeekerConditionType::IsRanged:
		return CheckIsRanged(OwnerComp);
	case ESeekerConditionType::IsMelee:
		return CheckIsMelee(OwnerComp);
	default:
		return false;
	}
}

// ========================================
// 개별 조건 체크 함수들
// ========================================

/**
 * 블랙보드에 타겟 적이 설정되어 있는지 확인
 * 
 * @return 타겟이 존재하면 true
 */
bool UGS_BTD_SeekerCondition::CheckHasTargetEnemy(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	UObject* Target = Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName);
	return Target != nullptr;
}

/**
 * 체력이 CustomHealthThreshold(기본 30%) 이하인지 확인
 * 힐 판단에 사용됩니다.
 * 
 * @return 체력이 임계값 이하면 true
 */
bool UGS_BTD_SeekerCondition::CheckIsLowHealth(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	float HealthPercent = Blackboard->GetValueAsFloat(AGS_SeekerAIController::CurrentHealthPercentKey);
	return HealthPercent <= CustomHealthThreshold;
}

/**
 * 체력이 위험 수준(15% 이하)인지 확인
 * 긴급 회피/도망 판단에 사용됩니다.
 * 
 * @return 체력이 15% 이하면 true
 */
bool UGS_BTD_SeekerCondition::CheckIsCriticalHealth(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	float HealthPercent = Blackboard->GetValueAsFloat(AGS_SeekerAIController::CurrentHealthPercentKey);
	return HealthPercent <= 0.15f; // 위험 임계값: 15%
}

/**
 * 블랙보드에 근처 트랩이 설정되어 있는지 확인
 * 트랩 회피 판단에 사용됩니다.
 * 
 * @return 근처에 트랩이 있으면 true
 */
bool UGS_BTD_SeekerCondition::CheckIsNearTrap(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	UObject* Trap = Blackboard->GetValueAsObject(AGS_SeekerAIController::NearbyTrapKey);
	return Trap != nullptr;
}

/**
 * 탐험 목표 지점에 도달했는지 확인
 * 
 * @return 목표 도달 시 true
 */
bool UGS_BTD_SeekerCondition::CheckHasReachedGoal(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	return Blackboard->GetValueAsBool(AGS_SeekerAIController::HasReachedGoalKey);
}

/**
 * 특정 인덱스의 스킬을 사용할 수 있는지 확인
 * AGS_AISeeker를 통해 확인하거나, 폴백으로 시커 생존 여부로 판단
 * 
 * @return 스킬 사용 가능 시 true
 */
bool UGS_BTD_SeekerCondition::CheckCanUseSkill(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return false;
	}

	APawn* Pawn = AIController->GetPawn();
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn);
	if (!Seeker)
	{
		return false;
	}

	// AGS_AISeeker 래퍼를 통해 스킬 사용 가능 여부 확인
	if (AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(Pawn->GetOwner()))
	{
		return AISeeker->CanUseSkill(SkillIndex);
	}

	// 폴백: 시커가 살아있고 스킬 컴포넌트가 있으면 사용 가능으로 간주
	if (UGS_SkillComp* SkillComp = Seeker->FindComponentByClass<UGS_SkillComp>())
	{
		return !Seeker->IsDead();
	}

	return false;
}

/**
 * 힐 포션/스킬을 사용할 수 있는지 확인
 * 
 * @return 힐 사용 가능 시 true
 */
bool UGS_BTD_SeekerCondition::CheckCanHeal(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return false;
	}

	APawn* Pawn = AIController->GetPawn();

	// AGS_AISeeker 래퍼를 통해 힐 가능 여부 확인
	if (AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(Pawn->GetOwner()))
	{
		return AISeeker->CanHeal();
	}

	// 폴백: 시커가 살아있으면 힐 가능으로 간주
	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn))
	{
		if (UGS_SkillComp* SkillComp = Seeker->FindComponentByClass<UGS_SkillComp>())
		{
			return !Seeker->IsDead();
		}
	}

	return false;
}

/**
 * 현재 전투 중인지 확인
 * 블랙보드의 IsInCombat 키를 확인합니다.
 * 
 * @return 전투 중이면 true
 */
bool UGS_BTD_SeekerCondition::CheckIsInCombat(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	return Blackboard->GetValueAsBool(AGS_SeekerAIController::IsInCombatKey);
}

/**
 * 타겟이 RangeThreshold 거리 내에 있는지 확인
 * 공격 사거리 판단에 사용됩니다.
 * 
 * @return 타겟이 범위 내에 있으면 true
 */
bool UGS_BTD_SeekerCondition::CheckIsTargetInRange(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return false;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return false;
	}

	// 블랙보드에서 타겟 액터 가져오기
	UObject* TargetObject = Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName);
	AActor* Target = Cast<AActor>(TargetObject);
	if (!Target)
	{
		return false;
	}

	// 거리 계산 및 임계값과 비교
	float Distance = FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation());

	// bUseAttackRange가 활성화되어 있으면 AI 컨트롤러의 동적 사거리를 사용
	if (bUseAttackRange)
	{
		if (AGS_SeekerAIController* SeekerAIC = Cast<AGS_SeekerAIController>(AIController))
		{
			// AI 컨트롤러에 설정된 사거리 (Merci: 1200, Melee: 350 등) 사용
			return Distance <= SeekerAIC->AttackRange;
		}
	}

	return Distance <= RangeThreshold;
}

/**
 * 시커가 살아있는지 확인
 * 
 * @return 살아있으면 true, 시커가 아닌 폰은 기본적으로 true 반환
 */
bool UGS_BTD_SeekerCondition::CheckIsAlive(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return false;
	}

	APawn* Pawn = AIController->GetPawn();
	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(Pawn))
	{
		return !Seeker->IsDead();
	}

	// 시커가 아닌 폰은 살아있다고 가정
	return true;
}

/**
 * 블랙보드에 쓰러진 아군이 설정되어 있는지 확인
 * 부활 판단에 사용됩니다.
 * 
 * @return 쓰러진 아군이 있으면 true
 */
bool UGS_BTD_SeekerCondition::CheckHasDownedAlly(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	UObject* Ally = Blackboard->GetValueAsObject(AGS_SeekerAIController::DownedAllyKey);
	return Ally != nullptr;
}

/**
 * 블랙보드에 상호작용 가능한 아이템이 설정되어 있는지 확인
 * 
 * @return 상호작용 아이템이 있으면 true
 */
bool UGS_BTD_SeekerCondition::CheckHasInteractiveItem(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return false;
	}

	UObject* Item = Blackboard->GetValueAsObject(AGS_SeekerAIController::InteractiveItemKey);
	return Item != nullptr;
}

/**
 * 현재 AI가 원거리 시커(Merci)인지 확인
 * 
 * 원거리 시커는 엄폐물을 활용한 전술을 사용합니다.
 * AGS_AISeeker를 통해 확인하거나, 폴백으로 Merci 클래스 여부로 판단
 * 
 * @return 원거리 시커면 true
 */
bool UGS_BTD_SeekerCondition::CheckIsRanged(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return false;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return false;
	}

	// AGS_AISeeker 래퍼를 통해 원거리 시커 여부 확인
	if (AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(Pawn->GetOwner()))
	{
		return AISeeker->IsRangedSeeker();
	}

	// 폴백: Merci 클래스 여부로 직접 확인
	return Pawn->IsA(AGS_Merci::StaticClass());
}

/**
 * 현재 AI가 근접 시커(Ares, Chan)인지 확인
 * 
 * 근접 시커는 우회/측면 공격 전술을 사용합니다.
 * AGS_AISeeker를 통해 확인하거나, 폴백으로 Ares/Chan 클래스 여부로 판단
 * 
 * @return 근접 시커면 true
 */
bool UGS_BTD_SeekerCondition::CheckIsMelee(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return false;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return false;
	}

	// AGS_AISeeker 래퍼를 통해 근접 시커 여부 확인
	if (AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(Pawn->GetOwner()))
	{
		return AISeeker->IsMeleeSeeker();
	}

	// 폴백: Ares 또는 Chan 클래스 여부로 직접 확인
	return Pawn->IsA(AGS_Ares::StaticClass()) || Pawn->IsA(AGS_Chan::StaticClass());
}

// ========================================
// 에디터 표시용 설명 문자열 생성
// 비헤이비어 트리 에디터에서 노드 위에 표시됩니다.
// ========================================
FString UGS_BTD_SeekerCondition::GetStaticDescription() const
{
	FString ConditionName;
	switch (ConditionType)
	{
	case ESeekerConditionType::HasTargetEnemy:
		ConditionName = TEXT("Has Target Enemy");
		break;
	case ESeekerConditionType::IsLowHealth:
		ConditionName = FString::Printf(TEXT("Health <= %.0f%%"), CustomHealthThreshold * 100.0f);
		break;
	case ESeekerConditionType::IsCriticalHealth:
		ConditionName = TEXT("Health <= 15%");
		break;
	case ESeekerConditionType::IsNearTrap:
		ConditionName = TEXT("Near Trap");
		break;
	case ESeekerConditionType::HasReachedGoal:
		ConditionName = TEXT("Reached Goal");
		break;
	case ESeekerConditionType::CanUseSkill:
		ConditionName = FString::Printf(TEXT("Can Use Skill %d"), SkillIndex);
		break;
	case ESeekerConditionType::CanHeal:
		ConditionName = TEXT("Can Heal");
		break;
	case ESeekerConditionType::IsInCombat:
		ConditionName = TEXT("In Combat");
		break;
	case ESeekerConditionType::IsTargetInRange:
		ConditionName = FString::Printf(TEXT("Target In Range (%.0f)"), RangeThreshold);
		break;
	case ESeekerConditionType::IsAlive:
		ConditionName = TEXT("Is Alive");
		break;
	case ESeekerConditionType::HasDownedAlly:
		ConditionName = TEXT("Has Downed Ally");
		break;
	case ESeekerConditionType::HasInteractiveItem:
		ConditionName = TEXT("Has Interactive Item");
		break;
	case ESeekerConditionType::IsRanged:
		ConditionName = TEXT("Is Ranged Seeker");
		break;
	case ESeekerConditionType::IsMelee:
		ConditionName = TEXT("Is Melee Seeker");
		break;
	default:
		ConditionName = TEXT("Unknown");
		break;
	}

	return FString::Printf(TEXT("Condition: %s"), *ConditionName);
}
