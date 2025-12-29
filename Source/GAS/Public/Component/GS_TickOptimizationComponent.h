// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GS_TickOptimizationComponent.generated.h"

/**
 * 거리 기반 틱 인터벌 버킷
 * Close: 0-30m (매 프레임 60Hz)
 * Medium: 30-80m (10Hz, 6프레임마다)
 * Far: 80m+ (2Hz, 30프레임마다)
 * Critical: 전투/빈사/스킬 사용 시 강제 60Hz
 */
UENUM(BlueprintType)
enum class ETickDistanceBucket : uint8
{
	Close UMETA(DisplayName = "Close (0-30m, 60Hz)"),
	Medium UMETA(DisplayName = "Medium (30-80m, 10Hz)"),
	Far UMETA(DisplayName = "Far (80m+, 2Hz)"),
	Critical UMETA(DisplayName = "Critical (Combat, 60Hz)")
};

/**
 * 거리 기반 틱 최적화 컴포넌트
 * 카메라와의 거리에 따라 액터의 틱 빈도를 조절하여 CPU 부하를 감소시킵니다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GAS_API UGS_TickOptimizationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_TickOptimizationComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	/**
	 * 현재 거리 버킷 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "Tick Optimization")
	ETickDistanceBucket GetCurrentDistanceBucket() const { return CurrentDistanceBucket; }

	/**
	 * Critical 상태 강제 설정 (전투/스킬 사용 등)
	 */
	UFUNCTION(BlueprintCallable, Category = "Tick Optimization")
	void SetCriticalState(bool bInCritical);

	/**
	 * Throttled Tick 실행 가능 여부 확인
	 * @param CurrentTime 현재 World Time
	 * @return 틱 실행 가능 시 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Tick Optimization")
	bool ShouldExecuteThrottledTick(float CurrentTime) const;

	/**
	 * Throttled Tick 실행 시 호출 (타임스탬프 업데이트)
	 */
	UFUNCTION(BlueprintCallable, Category = "Tick Optimization")
	void MarkThrottledTickExecuted(float CurrentTime);

protected:
	/**
	 * 거리 버킷 재계산
	 */
	void UpdateDistanceBucket();

	/**
	 * 카메라와의 거리 계산
	 * @return 카메라와의 거리 (cm)
	 */
	float CalculateCameraDistance() const;

	/**
	 * 버킷별 틱 인터벌 반환
	 * @param Bucket 거리 버킷
	 * @return 틱 인터벌 (초)
	 */
	float GetTickIntervalForBucket(ETickDistanceBucket Bucket) const;

private:
	// 현재 거리 버킷
	UPROPERTY()
	ETickDistanceBucket CurrentDistanceBucket;

	// Critical 상태 플래그
	UPROPERTY()
	bool bIsCriticalState;

	// 마지막 Throttled Tick 실행 시간
	float LastThrottledTickTime;

	// 거리 버킷 업데이트 타이머
	FTimerHandle DistanceBucketUpdateTimer;

	// === 틱 인터벌 상수 ===
	static constexpr float TICK_INTERVAL_CLOSE = 0.0167f; // 60Hz (매 프레임 at 60fps)
	static constexpr float TICK_INTERVAL_MEDIUM = 0.1f; // 10Hz
	static constexpr float TICK_INTERVAL_FAR = 0.5f; // 2Hz
	static constexpr float TICK_INTERVAL_CRITICAL = 0.0167f; // 60Hz (강제)

	// === 거리 임계값 상수 (cm) ===
	static constexpr float DISTANCE_THRESHOLD_CLOSE = 6000.0f; // 60m
	static constexpr float DISTANCE_THRESHOLD_MEDIUM = 12000.0f; // 120m

	// === 버킷 업데이트 간격 ===
	static constexpr float BUCKET_UPDATE_INTERVAL = 1.0f; // 1초마다 거리 버킷 재계산
};
