// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/Debuff/EDebuffType.h"
#include "Character/Component/GS_DebuffComp.h" // FDebuffRepInfo 완전한 정의 필요
#include "GS_DebuffIndicatorComponent.generated.h"

class UWidgetComponent;
class UGS_DebuffIndicatorWidget;
class UGS_DebuffComp;

/**
 * 디버프 아이콘 표시를 담당하는 컴포넌트
 * 
 * 시커 시점에서 몬스터, 가디언, 동료 시커의 머리 위에
 * 현재 활성화된 디버프 아이콘을 표시합니다.
 * 
 * 주요 기능:
 * - 최대 4개의 디버프 아이콘 표시 (우선순위 기반)
 * - CC기(Stun, Confuse, Mute) 강조 표시
 * - 원형 게이지로 대략적인 남은 시간 표시
 * - 시커/가디언 시점에서만 활성화 (RTS 시점 제외)
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GAS_API UGS_DebuffIndicatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_DebuffIndicatorComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 디버프 인디케이터 표시/숨김 */
	UFUNCTION(BlueprintCallable, Category = "Debuff Indicator")
	void SetIndicatorVisible(bool bVisible);

	/** 현재 표시 중인지 여부 */
	UFUNCTION(BlueprintPure, Category = "Debuff Indicator")
	bool IsIndicatorVisible() const { return bIsVisible; }

protected:
	/** 디버프 위젯 컴포넌트 (머리 위 3D 공간) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debuff Indicator")
	TObjectPtr<UWidgetComponent> DebuffWidgetComponent;

	/** 디버프 인디케이터 위젯 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff Indicator|UI")
	TSubclassOf<UGS_DebuffIndicatorWidget> DebuffIndicatorWidgetClass;

	/** 위젯 표시 위치 오프셋 (캐릭터 기준) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff Indicator|Settings")
	FVector WidgetLocationOffset = FVector(0.f, 0.f, 170.f);

	/** 최대 표시 아이콘 개수 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff Indicator|Settings")
	int32 MaxDisplayIcons = 4;

	/** 가시성 체크 거리 (이 거리 밖에서는 숨김) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff Indicator|Settings")
	float VisibilityDistance = 3000.f;

private:
	/** 디버프 컴포넌트 캐싱 */
	UPROPERTY()
	TWeakObjectPtr<UGS_DebuffComp> CachedDebuffComp;

	/** 디버프 인디케이터 위젯 인스턴스 */
	UPROPERTY()
	TObjectPtr<UGS_DebuffIndicatorWidget> DebuffIndicatorWidget;

	/** 현재 표시 중 여부 */
	bool bIsVisible = false;

	/** 초기화 완료 여부 */
	bool bIsInitialized = false;

	/** 위젯 컴포넌트 생성 및 초기화 */
	void InitializeWidgetComponent();

	/** DebuffComp의 OnDebuffListUpdated 이벤트 핸들러 */
	void OnDebuffListUpdated(const TArray<FDebuffRepInfo>& DebuffList);

	/** 디버프 우선순위 기반 정렬 (CC기 우선) */
	TArray<FDebuffRepInfo> SortDebuffsByPriority(const TArray<FDebuffRepInfo>& DebuffList) const;

	/** 디버프 타입별 표시 우선순위 반환 (낮을수록 높은 우선순위) */
	int32 GetDebuffDisplayPriority(EDebuffType Type) const;

	/** 로컬 플레이어로부터의 거리 체크하여 가시성 업데이트 */
	void UpdateVisibilityByDistance();

	/** 가시성 업데이트 타이머 */
	FTimerHandle VisibilityUpdateTimerHandle;

	/** 가시성 업데이트 간격 (성능 최적화) */
	static constexpr float VISIBILITY_UPDATE_INTERVAL = 0.2f;
};
