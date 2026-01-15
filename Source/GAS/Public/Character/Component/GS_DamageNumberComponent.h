// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/Damage/EDamageNumberType.h"
#include "GS_DamageNumberComponent.generated.h"

class UGS_DamageNumberWidget;
class UWidgetComponent;
class APlayerController;

/**
 * 데미지 숫자 표시를 관리하는 컴포넌트
 * 
 * 캐릭터가 가한 데미지를 화면에 숫자로 표시합니다.
 * 멀티플레이어 환경에서 서버→클라 RPC를 통해 동기화됩니다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GAS_API UGS_DamageNumberComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_DamageNumberComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 데미지 숫자 표시 (서버에서 호출)
	 * 
	 * @param Damage 표시할 데미지 양
	 * @param Type 데미지 타입 (Normal, Critical, DoT, Heal)
	 * @param WorldLocation 표시할 월드 위치
	 * @param TargetActor 데미지를 받은 대상 (누적 시스템용, nullptr 가능)
	 */
	UFUNCTION(BlueprintCallable, Category = "Damage Number")
	void ShowDamageNumber(float Damage, EDamageNumberType Type, FVector WorldLocation, AActor* TargetActor = nullptr);

protected:
	/** 멀티캐스트 RPC - 모든 클라이언트에서 데미지 숫자 표시 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ShowDamageNumber(float Damage, EDamageNumberType Type, FVector WorldLocation, AActor* TargetActor);

	/** 데미지 숫자 위젯 클래스 (블루프린트에서 설정) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|UI")
	TSubclassOf<UGS_DamageNumberWidget> DamageNumberWidgetClass;

	/** 위젯 풀 최대 크기 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Pool")
	int32 MaxPoolSize = 20;

	/** 숫자 표시 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Settings")
	float DisplayDuration = 0.7f;

	/** 숫자 상승 높이 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Settings")
	float FloatUpDistance = 100.0f;

	/** 위치 랜덤 오프셋 (겹침 방지) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Settings")
	float RandomOffset = 60.0f;

	/** 데미지 누적 활성화 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Accumulation")
	bool bEnableAccumulation = true;

	/** 누적 윈도우 시간 - 이 시간 내 동일 타겟 데미지는 누적됨 (초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Accumulation", meta = (EditCondition = "bEnableAccumulation"))
	float AccumulationWindow = 0.6f;

	/** ==== 거리 기반 스케일링 ==== */

	/** 거리 기반 스케일링 활성화 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Distance Scaling")
	bool bEnableDistanceScaling = true;

	/** 최소 스케일 (원거리) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Distance Scaling", meta = (EditCondition = "bEnableDistanceScaling", ClampMin = "0.3", ClampMax = "1.0"))
	float MinDistanceScale = 0.6f;

	/** 최대 스케일 (근거리) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Distance Scaling", meta = (EditCondition = "bEnableDistanceScaling", ClampMin = "1.0", ClampMax = "2.0"))
	float MaxDistanceScale = 1.2f;

	/** 근거리 기준 (이 거리 이하면 최대 스케일) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Distance Scaling", meta = (EditCondition = "bEnableDistanceScaling"))
	float NearDistance = 500.0f;

	/** 원거리 기준 (이 거리 이상이면 최소 스케일) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Distance Scaling", meta = (EditCondition = "bEnableDistanceScaling"))
	float FarDistance = 3000.0f;

	/** ==== 스마트 겹침 방지 ==== */

	/** 겹침 방지 활성화 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Overlap Prevention")
	bool bEnableOverlapPrevention = true;

	/** 겹침 감지 반경 (픽셀) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Overlap Prevention", meta = (EditCondition = "bEnableOverlapPrevention"))
	float OverlapDetectionRadius = 50.0f;

	/** 겹침 시 Y축 오프셋 (위로 밀어내는 거리) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Overlap Prevention", meta = (EditCondition = "bEnableOverlapPrevention"))
	float OverlapYOffset = 35.0f;

private:
	/** 로컬에서 데미지 숫자 표시 처리 */
	void ShowDamageNumberInternal(float Damage, EDamageNumberType Type, FVector WorldLocation, AActor* TargetActor);

	/** 타겟에 대해 활성화된 위젯 찾기 */
	UGS_DamageNumberWidget* FindActiveWidgetForTarget(AActor* TargetActor) const;

	/** 위젯 풀에서 사용 가능한 위젯 가져오기 */
	UGS_DamageNumberWidget* GetPooledWidget(APlayerController* PC);

	/** 위젯을 풀에 반환 */
	void ReturnToPool(UGS_DamageNumberWidget* Widget);

	/** 활성 위젯 목록 */
	UPROPERTY()
	TArray<TObjectPtr<UGS_DamageNumberWidget>> ActiveWidgets;

	/** 비활성(재사용 가능) 위젯 풀 */
	UPROPERTY()
	TArray<TObjectPtr<UGS_DamageNumberWidget>> WidgetPool;
};
