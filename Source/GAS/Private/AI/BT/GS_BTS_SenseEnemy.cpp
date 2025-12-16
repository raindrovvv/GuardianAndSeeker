// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BT/GS_BTS_SenseEnemy.h"
#include "AI/GS_AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "GenericTeamAgentInterface.h"
#include "AI/RTS/RTSCommand.h"

UGS_BTS_SenseEnemy::UGS_BTS_SenseEnemy()
{
	NodeName = TEXT("Sense Enemy");
	bNotifyTick = true;
	
	// AI 감지 최적화: 0.5초마다 실행
	Interval = 0.5f;
	RandomDeviation = 0.1f;
}

void UGS_BTS_SenseEnemy::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AGS_AIController* AIController = Cast<AGS_AIController>(OwnerComp.GetAIOwner());
	if(!AIController)
	{
		return;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (Blackboard->GetValueAsBool(AGS_AIController::DebuffLockedKey))
	{
		return;
	}

	if (Blackboard->GetValueAsBool(AGS_AIController::TargetLockedKey))
	{
		return;
	}

	// Move 명령 중에만 적을 감지하지 않음 (Hold는 적 감지해야 함)
	const uint8 CurrentCommand = Blackboard->GetValueAsEnum(AGS_AIController::CommandKey);
	if (CurrentCommand == static_cast<uint8>(ERTSCommand::Move))
	{
		// Move 중에는 목적지로 이동만 (적 감지 안 함)
		return;
	}

	TArray<AActor*> Targets;
	AIController->PerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Targets);

	// 적(Hostile)만 필터링
	TArray<AActor*> HostileTargets;
	for (AActor* Target : Targets)
	{
		if (Target && AIController->GetTeamAttitudeTowards(*Target) == ETeamAttitude::Hostile)
		{
			HostileTargets.Add(Target);
		}
	}

	if (HostileTargets.IsEmpty())
	{
		if (Blackboard->GetValueAsObject(AGS_AIController::TargetActorKey) != nullptr)
		{
			AIController->ClearCurrentTarget();
		}

		return;
	}

	// 가장 가까운 적 찾기
	APawn* ControlledPawn = Cast<APawn>(AIController->GetPawn());
	float ClosestDist = TNumericLimits<float>::Max();
	AActor* NearestTarget = nullptr;

	for (AActor* Target : HostileTargets)
	{
		const float Dist = FVector::DistSquared(ControlledPawn->GetActorLocation(),	Target->GetActorLocation());
		if (Dist < ClosestDist)
		{
			ClosestDist = Dist;
			NearestTarget = Target;
		}
	}

	if (NearestTarget)
	{
		AIController->SetNewTarget(NearestTarget);
	}
}
