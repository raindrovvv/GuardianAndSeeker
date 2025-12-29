// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BT/GS_BTS_Cooldown.h"
#include "AI/GS_AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/GS_Character.h"
#include "Character/Component/GS_StatComp.h"

UGS_BTS_Cooldown::UGS_BTS_Cooldown()
{
	NodeName = TEXT("Cooldown");
	bNotifyTick = true;
	
	// [성능 최적화] 쿨다운 체크 주기를 0.1초로 설정 (약 10 FPS)
	// 매 프레임 체크 대신 주기적 체크로 CPU 부하를 약 6배 감소
	// RandomDeviation으로 다수 AI의 틱 동시 발생 방지
	Interval = 0.1f;
	RandomDeviation = 0.05f;
}

void UGS_BTS_Cooldown::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return;
	}
	
	AGS_Character* Character = Cast<AGS_Character>(AIController->GetPawn());
	if (!Character)
	{
		return;
	}
	
	const float NowTime = OwnerComp.GetWorld()->GetTimeSeconds();

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Blackboard))
	{
		return;
	}

	const float LastTime = Blackboard->GetValueAsFloat(AGS_AIController::LastAttackTimeKey);
	const float AttackSpeed = Character->GetStatComp()->GetAttackSpeed();
	const bool bCanAttack = (NowTime - LastTime) >= AttackSpeed;

	Blackboard->SetValueAsBool(AGS_AIController::CanAttackKey, bCanAttack);
	
}
