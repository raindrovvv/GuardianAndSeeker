// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Character/Debuff/EDebuffType.h"
#include "GS_DebuffIconWidget.generated.h"

class UImage;
class UTexture2D;
class UMaterialInstanceDynamic;

/**
 * 개별 디버프 아이콘 위젯
 * 
 * 각 디버프 타입에 해당하는 아이콘을 표시하며,
 * 잔여 시간은 원형 게이지로 표현합니다.
 * 
 * CC기(Stun, Confuse, Mute)는 강조 표시:
 * - 1.3배 크기
 * - 빨간 테두리
 * - 펄스 애니메이션
 */
UCLASS()
class GAS_API UGS_DebuffIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGS_DebuffIconWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/**
	 * 디버프 정보로 아이콘 설정
	 * @param Type 디버프 타입
	 * @param RemainingTime 남은 시간
	 * @param TotalDuration 전체 지속 시간 (게이지 계산용)
	 * @param bIsCC CC기 여부 (강조 표시)
	 */
	UFUNCTION(BlueprintCallable, Category = "Debuff Icon")
	void SetDebuffInfo(EDebuffType Type, float RemainingTime, float TotalDuration, bool bIsCC);

	/** 아이콘 숨기기 */
	UFUNCTION(BlueprintCallable, Category = "Debuff Icon")
	void HideIcon();

	/** 아이콘 표시하기 */
	UFUNCTION(BlueprintCallable, Category = "Debuff Icon")
	void ShowIcon();

	/** 잔여 시간 업데이트 (매 틱) */
	UFUNCTION(BlueprintCallable, Category = "Debuff Icon")
	void UpdateRemainingTime(float NewRemainingTime, float TotalDuration);

protected:
	/** 메인 아이콘 이미지 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	/** 원형 게이지 (잔여 시간) */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> GaugeImage;

	/** CC 강조용 테두리 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> BorderImage;

	/** 디버프 타입별 아이콘 텍스처 맵 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff Icon|Assets")
	TMap<EDebuffType, TSoftObjectPtr<UTexture2D>> DebuffIconTextures;

	/** 원형 게이지 머티리얼 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff Icon|Assets")
	TObjectPtr<UMaterialInterface> GaugeMaterial;

	/** 기본 아이콘 크기 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff Icon|Settings")
	float BaseIconSize = 32.f;

	/** CC 강조 시 크기 배율 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff Icon|Settings")
	float CCScaleMultiplier = 1.3f;

	/** 펄스 애니메이션 속도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff Icon|Settings")
	float PulseSpeed = 3.f;

	/** 펄스 애니메이션 강도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff Icon|Settings")
	float PulseIntensity = 0.1f;

	/** 끝날 때 깜빡임 시작 시간 (초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff Icon|Settings")
	float BlinkThreshold = 2.f;

private:
	/** 현재 디버프 타입 */
	EDebuffType CurrentDebuffType = EDebuffType::None;

	/** 게이지용 동적 머티리얼 */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GaugeMaterialInstance;

	/** CC기 여부 */
	bool bIsCrowdControl = false;

	/** 현재 잔여 시간 */
	float CurrentRemainingTime = 0.f;

	/** 전체 지속 시간 */
	float TotalDebuffDuration = 0.f;

	/** 펄스 애니메이션 시간 */
	float PulseTime = 0.f;

	/** 게이지 퍼센트 캐시 (변경 감지용 - 성능 최적화) */
	float CachedGaugePercent = -1.f;

	/** 디버프 타입에 맞는 아이콘 설정 */
	void UpdateIconTexture(EDebuffType Type);

	/** 게이지 퍼센트 업데이트 */
	void UpdateGaugePercent(float Percent);

	/** CC 강조 스타일 적용 */
	void ApplyCCStyle(bool bEnable);

	/** 펄스 애니메이션 업데이트 */
	void UpdatePulseAnimation(float DeltaTime);

	/** 끝날 때 깜빡임 처리 */
	void UpdateBlinkEffect(float RemainingTime, float DeltaTime);

	/** 깜빡임 타이머 */
	float BlinkTimer = 0.f;
	bool bBlinkState = true;
};
