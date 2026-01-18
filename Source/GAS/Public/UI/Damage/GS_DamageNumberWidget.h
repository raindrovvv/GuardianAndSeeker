// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Damage/EDamageNumberType.h"
#include "GS_DamageNumberWidget.generated.h"

class UTextBlock;
class UCanvasPanel;

/**
 * 데미지 숫자 타입별 스타일 정보
 */
USTRUCT(BlueprintType)
struct FDamageNumberStyle
{
	GENERATED_BODY()

	/** 텍스트 색상 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor Color = FLinearColor::White;

	/** 크기 배율 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Scale = 1.0f;

	/** 폰트 크기 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 FontSize = 24;

	/** 흔들림 효과 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bShake = false;

	/** 확대 후 축소 효과 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bPunchScale = false;
};

/**
 * 데미지 숫자 표시 위젯
 * 
 * 화면에 데미지 숫자를 표시하고 애니메이션 처리
 * 위젯 풀링을 통해 재사용됨
 */
UCLASS()
class GAS_API UGS_DamageNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGS_DamageNumberWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/**
	 * 데미지 숫자 표시 시작
	 * 
	 * @param Damage 표시할 데미지
	 * @param Type 데미지 타입
	 * @param ScreenPosition 화면 위치
	 * @param Duration 표시 시간
	 * @param DistanceScale 거리 기반 스케일 (1.0 = 기본, 0.6 = 원거리)
	 */
	UFUNCTION(BlueprintCallable, Category = "Damage Number")
	void ShowDamage(float Damage, EDamageNumberType Type, FVector2D ScreenPosition, float Duration, float DistanceScale = 1.0f);

	/**
	 * 기존 숫자에 데미지 누적 (연속 타격 시 사용)
	 * 
	 * @param AdditionalDamage 추가할 데미지
	 * @return 누적 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Damage Number")
	bool AddDamage(float AdditionalDamage);

	/** 위젯이 현재 활성 상태인지 */
	UFUNCTION(BlueprintCallable, Category = "Damage Number")
	bool IsActive() const { return bIsActive; }

	/** 현재 타겟 액터 가져오기 (누적 시스템용) */
	UFUNCTION(BlueprintCallable, Category = "Damage Number")
	AActor* GetTargetActor() const { return TargetActor.Get(); }

	/** 타겟 액터 설정 */
	void SetTargetActor(AActor* InTarget) { TargetActor = InTarget; }

	/** 현재 스크린 위치 가져오기 (겹침 방지용) */
	FVector2D GetCurrentScreenPosition() const { return CurrentPosition; }

	/** 표시 완료 시 호출되는 델리게이트 */
	DECLARE_DELEGATE_OneParam(FOnAnimationComplete, UGS_DamageNumberWidget*);
	FOnAnimationComplete OnAnimationComplete;

protected:
	/** 숫자 텍스트 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DamageText;

	/** ==== 타입별 스타일 설정 ==== */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Style")
	FDamageNumberStyle NormalStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Style")
	FDamageNumberStyle CriticalStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Style")
	FDamageNumberStyle DoTStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Style")
	FDamageNumberStyle HealStyle;

	/** ==== 포물선(Arc) 애니메이션 파라미터 ==== */

	/** 초기 수직 속도 (위로 튀어오르는 힘, 픽셀/초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Animation")
	float InitialUpwardSpeed = 180.0f;

	/** 초기 수평 속도 범위 (좌우로 튀는 힘, 픽셀/초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Animation")
	float HorizontalSpeedRange = 80.0f;

	/** 중력 가속도 (픽셀/초²) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Animation")
	float Gravity = 350.0f;

	/** 크리티컬 히트 시 속도 배율 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Animation")
	float CriticalSpeedMultiplier = 1.4f;

	/** 페이드 아웃 시작 비율 (0~1, 남은 시간 비율) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number|Animation")
	float FadeOutStartRatio = 0.3f;

private:
	/** 타입에 맞는 스타일 가져오기 */
	const FDamageNumberStyle& GetStyleForType(EDamageNumberType Type) const;

	/** 위젯 비활성화 및 풀 반환 */
	void Deactivate();

	/** 데미지 텍스트 업데이트 (중복 로직 방지) */
	void UpdateDamageText(float DamageAmount);

	/** 현재 활성 상태 */
	bool bIsActive = false;

	/** 현재 표시 타입 */
	EDamageNumberType CurrentType = EDamageNumberType::Normal;

	/** 애니메이션 타이머 */
	float ElapsedTime = 0.f;
	float TotalDuration = 1.0f;

	/** 시작 위치 */
	FVector2D StartPosition = FVector2D::ZeroVector;

	/** 현재 적용 중인 스타일 */
	FDamageNumberStyle CurrentStyle;

	/** 펀치 스케일 상태 */
	float PunchScalePhase = 0.f;

	/** 흔들림 오프셋 (미리 계산해두고 재사용) */
	FVector2D CachedShakeOffset = FVector2D::ZeroVector;

	/** 데미지 양 기반 스케일 */
	float CachedDamageScale = 1.0f;

	/** 거리 기반 스케일 */
	float CachedDistanceScale = 1.0f;

	/** ==== 포물선 물리 상태 ==== */

	/** 초기 속도 벡터 (픽셀/초) - ShowDamage에서 랜덤 생성 */
	FVector2D InitialVelocity = FVector2D::ZeroVector;

	/** 현재 속도 벡터 (중력 적용됨) */
	FVector2D CurrentVelocity = FVector2D::ZeroVector;

	/** 현재 위치 (물리 시뮬레이션용) */
	FVector2D CurrentPosition = FVector2D::ZeroVector;

	/** ==== 데미지 누적 시스템 ==== */

	/** 누적된 총 데미지 */
	float TotalAccumulatedDamage = 0.f;

	/** 타겟 액터 (누적 판단용) */
	TWeakObjectPtr<AActor> TargetActor = nullptr;

	/** 누적 펄스 효과 타이머 (데미지 추가 시 리셋) */
	float AccumulationPulseTime = -1.f;

	/** 누적 펄스 효과 지속 시간 */
	static constexpr float AccumulationPulseDuration = 0.1f;

	/** 누적 펄스 효과 강도 */
	static constexpr float AccumulationPulseStrength = 0.25f;
};
