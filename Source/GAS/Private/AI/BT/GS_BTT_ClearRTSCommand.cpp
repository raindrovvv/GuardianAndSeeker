// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BT/GS_BTT_ClearRTSCommand.h"
#include "AI/GS_AIController.h"
#include "AI/RTS/RTSCommand.h"
#include "BehaviorTree/BlackboardComponent.h"

UGS_BTT_ClearRTSCommand::UGS_BTT_ClearRTSCommand()
{
	NodeName = TEXT("ClearRTS");
}

EBTNodeResult::Type UGS_BTT_ClearRTSCommand::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController && AIController->GetPawn())
	{
		uint8 CommandType = Blackboard->GetValueAsEnum(AGS_AIController::CommandKey);
		
		// Move나 Attack 모드인 경우, 목표 지점과의 거리를 체크
		if (CommandType == static_cast<uint8>(ERTSCommand::Move) || CommandType == static_cast<uint8>(ERTSCommand::Attack))
		{
			FVector TargetLocation = Blackboard->GetValueAsVector(AGS_AIController::MoveLocationKey);
			
			// MoveLocationKey가 유효한 경우에만 거리 체크 (Attack의 경우 TargetActor를 향해 갈 때는 Location이 없을 수 있음)
			if (!TargetLocation.IsZero())
			{
				float Distance = FVector::Dist(AIController->GetPawn()->GetActorLocation(), TargetLocation);
				
				// 목표 지점과의 거리가 아직 멀다면 (문 등에 막혀 부분 경로만 이동한 경우)
				// 커맨드를 초기화하지 않고 성공을 반환하여, BT에서 다시 루프를 돌며 이동을 시도하게 함
				if (Distance > 250.0f) // 문 두께와 유격 등을 고려하여 2.5m 정도로 설정
				{
					return EBTNodeResult::Succeeded;
				}
			}
		}
	}

	Blackboard->SetValueAsEnum(AGS_AIController::CommandKey, static_cast<uint8>(ERTSCommand::None));
	return EBTNodeResult::Succeeded;
}
