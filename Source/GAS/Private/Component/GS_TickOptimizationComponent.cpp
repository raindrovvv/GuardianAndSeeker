// Fill out your copyright notice in the Description page of Project Settings.

#include "Component/GS_TickOptimizationComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UGS_TickOptimizationComponent::UGS_TickOptimizationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	CurrentDistanceBucket = ETickDistanceBucket::Close;
	bIsCriticalState = false;
	LastThrottledTickTime = -1.0f;
}

void UGS_TickOptimizationComponent::BeginPlay()
{
	Super::BeginPlay();

	// 초기 거리 버킷 계산
	UpdateDistanceBucket();

	// 1초마다 거리 버킷 재계산
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
		    DistanceBucketUpdateTimer, this,
		    &UGS_TickOptimizationComponent::UpdateDistanceBucket,
		    BUCKET_UPDATE_INTERVAL, true);
	}
}

void UGS_TickOptimizationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DistanceBucketUpdateTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void UGS_TickOptimizationComponent::SetCriticalState(bool bInCritical)
{
	if (bIsCriticalState != bInCritical)
	{
		bIsCriticalState = bInCritical;

		// Critical 상태 변경 시 즉시 버킷 업데이트
		UpdateDistanceBucket();
	}
}

bool UGS_TickOptimizationComponent::ShouldExecuteThrottledTick(float CurrentTime) const
{
	// 첫 틱은 항상 실행
	if (LastThrottledTickTime < 0.0f)
	{
		return true;
	}

	// 현재 버킷의 인터벌 가져오기
	float TickInterval = GetTickIntervalForBucket(CurrentDistanceBucket);

	// 인터벌 경과 여부 확인
	return (CurrentTime - LastThrottledTickTime) >= TickInterval;
}

void UGS_TickOptimizationComponent::MarkThrottledTickExecuted(float CurrentTime)
{
	LastThrottledTickTime = CurrentTime;
}

void UGS_TickOptimizationComponent::UpdateDistanceBucket()
{
	// 서버(Listen Server 포함)에서는 이동 및 AI 로직의 정확성을 위해 틱 최적화를 수행하지 않음
	// 오직 클라이언트(SimulatedProxy, Proxy)에서만 시각적 최적화를 위해 수행
	if (GetOwner()->HasAuthority())
	{
		CurrentDistanceBucket = ETickDistanceBucket::Critical;
		return;
	}

	// Critical 상태가 최우선
	if (bIsCriticalState)
	{
		CurrentDistanceBucket = ETickDistanceBucket::Critical;
		return;
	}

	// 카메라와의 거리 계산
	float Distance = CalculateCameraDistance();

	// 거리 계산에 실패한 경우 (카메라Manager가 아직 유효하지 않은 등) Close 유지
	if (Distance < 0.0f)
	{
		return;
	}

	// 거리 기반 버킷 결정 (TPS 시점을 고려하여 임계값 상향)
	ETickDistanceBucket NewBucket;
	if (Distance < DISTANCE_THRESHOLD_CLOSE)
	{
		NewBucket = ETickDistanceBucket::Close;
	}
	else if (Distance < DISTANCE_THRESHOLD_MEDIUM)
	{
		NewBucket = ETickDistanceBucket::Medium;
	}
	else
	{
		NewBucket = ETickDistanceBucket::Far;
	}

	// 버킷 변경 로깅
	if (NewBucket != CurrentDistanceBucket)
	{
		CurrentDistanceBucket = NewBucket;
		UE_LOG(LogTemp, Verbose,
		       TEXT("[TickOpt:%s] Distance Bucket changed to %d (Distance: %.1f)"),
		       *GetOwner()->GetName(), static_cast<int32>(CurrentDistanceBucket),
		       Distance);
	}
}

float UGS_TickOptimizationComponent::CalculateCameraDistance() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return -1.0f;
	}

	// 로컬 플레이어 컨트롤러 가져오기
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return -1.0f;
	}

	// 카메라 위치와 액터 위치 간 거리 계산
	FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	FVector OwnerLocation = GetOwner()->GetActorLocation();

	return FVector::Dist(CameraLocation, OwnerLocation);
}

float UGS_TickOptimizationComponent::GetTickIntervalForBucket(ETickDistanceBucket Bucket) const
{
	switch (Bucket)
	{
	case ETickDistanceBucket::Close:
		return TICK_INTERVAL_CLOSE;
	case ETickDistanceBucket::Medium:
		return TICK_INTERVAL_MEDIUM;
	case ETickDistanceBucket::Far:
		return TICK_INTERVAL_FAR;
	case ETickDistanceBucket::Critical:
		return TICK_INTERVAL_CRITICAL;
	default:
		return TICK_INTERVAL_CLOSE;
	}
}
