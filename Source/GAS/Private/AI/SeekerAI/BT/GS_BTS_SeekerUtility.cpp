// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/BT/GS_BTS_SeekerUtility.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UGS_BTS_SeekerUtility::UGS_BTS_SeekerUtility()
{
	NodeName = "Seeker Utility Service";
	Interval = 0.2f; // 초당 5회 업데이트
	RandomDeviation = 0.05f;
	bCreateNodeInstance = true; // 다중 AI 인스턴스 지원 필수

	BehaviorKey.SelectedKeyName = TEXT("CurrentBehavior");

	BehaviorSwitchCooldown = 0.3f; // 행동 전환 쿨다운
	HysteresisThreshold = 0.05f; // 점수 차이 임계값
}

void UGS_BTS_SeekerUtility::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AGS_SeekerAIController* AIC = Cast<AGS_SeekerAIController>(OwnerComp.GetAIOwner());
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();

	if (AIC && BB)
	{
		// 1. 핵심 AI 업데이트 (체력, 위협 평가, 유틸리티 점수)
		AIC->UpdateHealthStatus();
		AIC->UpdateThreatAssessment();
		AIC->UpdateUtilityScores();

		ESeekerBehavior BestBehavior = AIC->SelectBestBehavior();

		// 2. 행동 전환 쿨다운 적용으로 떨림 방지
		SwitchCooldownRemaining -= DeltaSeconds;

		bool bShouldSwitch = false;

		if (BestBehavior != CurrentActiveBehavior)
		{
			// 긴급 행동(힐, 회피)은 즉시 전환 허용
			bool bIsCriticalBehavior = (BestBehavior == ESeekerBehavior::Heal ||
			                            BestBehavior == ESeekerBehavior::Evade);

			// 히스테리시스 체크를 위한 점수 차이 계산
			float BestScore = AIC->GetUtilityScoreForBehavior(BestBehavior);
			float CurrentScore = AIC->GetUtilityScoreForBehavior(CurrentActiveBehavior);
			float ScoreDifference = BestScore - CurrentScore;

			if (bIsCriticalBehavior)
			{
				// 긴급 행동은 쿨다운과 히스테리시스 무시
				bShouldSwitch = true;
			}
			else if (SwitchCooldownRemaining <= 0.0f)
			{
				// 일반 행동은 쿨다운 만료 및 점수 차이 임계값 이상일 때만 전환
				if (ScoreDifference > HysteresisThreshold)
				{
					bShouldSwitch = true;
				}
			}
		}

		// 행동 전환 적용
		if (bShouldSwitch && BestBehavior != CurrentActiveBehavior)
		{
			CurrentActiveBehavior = BestBehavior;
			SwitchCooldownRemaining = BehaviorSwitchCooldown;
		}

		// 3. 안정된 행동으로 블랙보드 업데이트
		BB->SetValueAsEnum(BehaviorKey.SelectedKeyName, static_cast<uint8>(CurrentActiveBehavior));

		// 4. 안정된 행동 기반으로 다른 키 동기화
		BB->SetValueAsBool(AGS_SeekerAIController::IsInCombatKey,
		                   CurrentActiveBehavior == ESeekerBehavior::Combat || AIC->IsInCombat());
		BB->SetValueAsObject(AGS_SeekerAIController::TargetEnemyKey, AIC->GetHighestPriorityThreat());
		BB->SetValueAsBool(AGS_SeekerAIController::ShouldHealKey,
		                   CurrentActiveBehavior == ESeekerBehavior::Heal);
		BB->SetValueAsBool(AGS_SeekerAIController::ShouldEvadeKey,
		                   CurrentActiveBehavior == ESeekerBehavior::Evade);
		BB->SetValueAsObject(AGS_SeekerAIController::DownedAllyKey,
		                     (CurrentActiveBehavior == ESeekerBehavior::Revive) ? BB->GetValueAsObject(AGS_SeekerAIController::DownedAllyKey) : nullptr);
	}
}
