// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Character/Component/GS_HitIndicatorComponent.h"
#include "GS_HitIndicatorWidget.generated.h"

class UImage;
class UCanvasPanel;

/**
 * 활성 인디케이터 정보를 저장하는 구조체
 */
USTRUCT(BlueprintType)
struct FActiveIndicatorInfo
{
	GENERATED_BODY()

	UPROPERTY()
	EHitDirection Direction = EHitDirection::None;

	UPROPERTY()
	float RemainingTime = 0.f;

	/** 연속 피격 강화 단계 (1~3) */
	UPROPERTY()
	int32 IntensityLevel = 1;

	/** 마지막 피격 시간 */
	UPROPERTY()
	float LastHitTime = 0.f;
};

/**
 * 방향성 피격 HUD 위젯
 * 
 * 수평 공격 (전후좌우): 화면 가장자리 화살표 인디케이터
 * 수직 공격 (상하): 화면 테두리 그라데이션 + 아이콘
 */
UCLASS()
class GAS_API UGS_HitIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGS_HitIndicatorWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 소유 컴포넌트 설정 */
	void SetOwnerComponent(UGS_HitIndicatorComponent* InComponent);

	/**
	 * 피격 인디케이터 표시
	 * 
	 * @param Direction 피격 방향
	 * @param DamageAmount 데미지 양 (강도 결정에 사용)
	 */
	UFUNCTION(BlueprintCallable, Category = "Hit Indicator")
	void ShowHitIndicator(EHitDirection Direction, float DamageAmount);

protected:
	/** ==== 수평 방향 인디케이터 (화살표) ==== */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> FrontIndicator;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackIndicator;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LeftIndicator;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> RightIndicator;

	/** ==== 수직 방향 인디케이터 (그라데이션 + 아이콘) ==== */
	/** 상단 그라데이션 이미지 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> TopGradient;

	/** 하단 그레디언트 이미지 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> BottomGradient;

	/** 좌측 그레디언트 이미지 (Omni 전용) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LeftGradient;

	/** 우측 그레디언트 이미지 (Omni 전용) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> RightGradient;

	/** ==== 설정값 ==== */
	/** 인디케이터 기본 표시 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Indicator|Settings")
	float DefaultDisplayDuration = 0.8f;

	/** 페이드 아웃 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Indicator|Settings")
	float FadeOutDuration = 0.3f;

	/** 연속 피격 인정 시간 (이 시간 내 같은 방향 피격 시 강화) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Indicator|Settings")
	float ConsecutiveHitWindow = 0.5f;

	/** 경미한 데미지 기준 (MaxHP 대비 비율) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Indicator|Settings")
	float LightDamageThreshold = 0.1f;

	/** 강한 데미지 기준 (MaxHP 대비 비율) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Indicator|Settings")
	float HeavyDamageThreshold = 0.3f;

	/** 인디케이터 기본 색상 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Indicator|Settings")
	FLinearColor IndicatorColor = FLinearColor(1.f, 0.2f, 0.2f, 0.8f);

	/** 강한 피격 시 색상 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Indicator|Settings")
	FLinearColor HeavyHitColor = FLinearColor(1.f, 0.f, 0.f, 1.f);

private:
	/** 델리게이트 수신 핸들러 */
	UFUNCTION()
	void OnDamageDirectionReceived(EHitDirection Direction, float DamageAmount);

	/** 방향별로 해당하는 이미지 위젯 반환 */
	UImage* GetIndicatorForDirection(EHitDirection Direction) const;

	/** 그레디언트 이미지 반환 (상하좌우 전용) */
	UImage* GetGradientForDirection(EHitDirection Direction) const;

	/** 인디케이터 업데이트 (Tick에서 호출) */
	void UpdateIndicators(float DeltaTime);

	/** 데미지 양에 따른 색상 계산 */
	FLinearColor CalculateIndicatorColor(float DamageAmount) const;

	/** 강화 레벨에 따른 스케일 계산 */
	float CalculateScaleFromIntensity(int32 IntensityLevel) const;

	/** 소유 컴포넌트 */
	UPROPERTY()
	TWeakObjectPtr<UGS_HitIndicatorComponent> OwnerComponent;

	/** 활성 인디케이터 목록 */
	UPROPERTY()
	TArray<FActiveIndicatorInfo> ActiveIndicators;

	/** 플레이어 MaxHP (데미지 비율 계산용) */
	float CachedMaxHealth = 100.f;
};
