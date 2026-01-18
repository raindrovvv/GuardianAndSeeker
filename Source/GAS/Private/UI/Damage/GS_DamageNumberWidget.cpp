// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Damage/GS_DamageNumberWidget.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"

UGS_DamageNumberWidget::UGS_DamageNumberWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	// 기본 스타일 설정
	NormalStyle.Color = FLinearColor::White;
	NormalStyle.Scale = 1.0f;
	NormalStyle.FontSize = 24;
	NormalStyle.bShake = false;
	NormalStyle.bPunchScale = false;

	CriticalStyle.Color = FLinearColor(1.0f, 0.4f, 0.1f, 1.0f); // 주황색
	CriticalStyle.Scale = 1.2f; // 크리티컬 크기 배율 (1.5 → 1.2로 축소)
	CriticalStyle.FontSize = 32;
	CriticalStyle.bShake = true;
	CriticalStyle.bPunchScale = true;

	DoTStyle.Color = FLinearColor(0.6f, 0.2f, 0.8f, 1.0f); // 보라색
	DoTStyle.Scale = 0.7f;
	DoTStyle.FontSize = 18;
	DoTStyle.bShake = false;
	DoTStyle.bPunchScale = false;

	HealStyle.Color = FLinearColor(0.2f, 1.0f, 0.3f, 1.0f); // 녹색
	HealStyle.Scale = 1.0f;
	HealStyle.FontSize = 24;
	HealStyle.bShake = false;
	HealStyle.bPunchScale = false;
}

void UGS_DamageNumberWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UGS_DamageNumberWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsActive)
	{
		return;
	}

	ElapsedTime += InDeltaTime;

	// 애니메이션 진행률 (0~1) - Division by Zero 방지
	const float SafeDuration = FMath::Max(TotalDuration, KINDA_SMALL_NUMBER);
	const float Progress = FMath::Clamp(ElapsedTime / SafeDuration, 0.0f, 1.0f);

	// ====== 포물선 물리 시뮬레이션 ======
	// 중력 적용: V = V0 + g*t (스크린 좌표계에서 Y+ 가 아래이므로 중력은 양수)
	CurrentVelocity.Y = InitialVelocity.Y + (Gravity * ElapsedTime);
	// X 속도는 일정 (공기 저항 무시)
	CurrentVelocity.X = InitialVelocity.X;

	// 위치 계산: P = P0 + V0*t + 0.5*g*t^2
	CurrentPosition.X = StartPosition.X + (InitialVelocity.X * ElapsedTime);
	CurrentPosition.Y = StartPosition.Y + (InitialVelocity.Y * ElapsedTime) + (0.5f * Gravity * ElapsedTime * ElapsedTime);

	// 흔들림 효과 (처음 0.3초 동안만, 기본 위치에 오프셋 추가)
	FVector2D FinalPosition = CurrentPosition;
	if (CurrentStyle.bShake && ElapsedTime < 0.3f)
	{
		const float ShakeIntensity = 3.0f * (1.0f - ElapsedTime / 0.3f);
		FinalPosition.X += CachedShakeOffset.X * ShakeIntensity;
		FinalPosition.Y += CachedShakeOffset.Y * ShakeIntensity;
	}

	// 위젯 위치 설정
	SetPositionInViewport(FinalPosition, false);

	// 펀치 스케일 효과 (처음 0.15초 동안 - 더 빠르고 강렬하게)
	// 모든 타입에 기본 pop-in 효과, Critical은 더 강하게
	const float PopDuration = CurrentStyle.bPunchScale ? 0.15f : 0.1f;
	const float PopStrength = CurrentStyle.bPunchScale ? 0.5f : 0.2f; // Critical: 1.5x, Normal: 1.2x
	const float BaseScale = CurrentStyle.Scale * CachedDamageScale * CachedDistanceScale;

	// 누적 펄스 효과 (AddDamage 호출 시 발동)
	float AccumulationMultiplier = 1.0f;
	if (AccumulationPulseTime >= 0.f)
	{
		AccumulationPulseTime += InDeltaTime;
		if (AccumulationPulseTime < AccumulationPulseDuration)
		{
			const float PulsePhase = AccumulationPulseTime / AccumulationPulseDuration;
			AccumulationMultiplier = 1.0f + AccumulationPulseStrength * FMath::Sin(PulsePhase * PI);
		}
		else
		{
			AccumulationPulseTime = -1.f; // 펄스 종료
		}
	}

	// 스케일 계산 및 적용
	const float FinalScale = BaseScale * AccumulationMultiplier;
	if (ElapsedTime < PopDuration)
	{
		const float PopPhase = ElapsedTime / PopDuration;
		// 1.0 -> (1.0 + Strength) -> 1.0 커브
		const float PopMultiplier = 1.0f + PopStrength * FMath::Sin(PopPhase * PI);
		SetRenderScale(FVector2D(FinalScale * PopMultiplier));
	}
	else if (AccumulationPulseTime >= 0.f)
	{
		// 누적 펄스 중일 때만 스케일 업데이트
		SetRenderScale(FVector2D(FinalScale));
	}

	// 페이드 아웃 (마지막 30% 구간)
	const float RemainingRatio = 1.0f - Progress;
	if (RemainingRatio < FadeOutStartRatio)
	{
		const float FadeAlpha = RemainingRatio / FadeOutStartRatio;
		SetRenderOpacity(FadeAlpha);
	}

	// 애니메이션 완료
	if (Progress >= 1.0f)
	{
		Deactivate();
	}
}

void UGS_DamageNumberWidget::ShowDamage(float Damage, EDamageNumberType Type, FVector2D ScreenPosition, float Duration, float DistanceScale)
{
	bIsActive = true;
	ElapsedTime = 0.0f;
	TotalDuration = Duration;
	StartPosition = ScreenPosition;
	CurrentType = Type;
	CurrentStyle = GetStyleForType(Type);

	// 거리 기반 스케일 저장
	CachedDistanceScale = DistanceScale;

	// 흔들림 오프셋 미리 계산 (매 프레임 랜덤 방지)
	CachedShakeOffset.X = FMath::RandRange(-1.0f, 1.0f);
	CachedShakeOffset.Y = FMath::RandRange(-1.0f, 1.0f);

	// ====== 포물선 초기 속도 설정 ======
	// 랜덤 수평 속도 (-HorizontalSpeedRange ~ +HorizontalSpeedRange)
	const float RandomHorizontal = FMath::RandRange(-HorizontalSpeedRange, HorizontalSpeedRange);

	// 수직 속도는 위쪽(음수 방향, 스크린 좌표계)
	float UpwardSpeed = -InitialUpwardSpeed;

	// 크리티컬 히트 시 속도 증폭
	const float SpeedMultiplier = (Type == EDamageNumberType::Critical) ? CriticalSpeedMultiplier : 1.0f;

	InitialVelocity.X = RandomHorizontal * SpeedMultiplier;
	InitialVelocity.Y = UpwardSpeed * SpeedMultiplier;

	// 현재 속도와 위치 초기화
	CurrentVelocity = InitialVelocity;
	CurrentPosition = StartPosition;

	// 누적 데미지 초기화
	TotalAccumulatedDamage = Damage;
	AccumulationPulseTime = -1.f;

	// 데미지 양에 따른 크기 스케일링
	// 10 데미지 = 0.8x, 100 데미지 = 1.2x, 500+ 데미지 = 1.5x
	float DamageScale = 1.0f;
	if (Type != EDamageNumberType::Heal) // 힐은 고정 크기
	{
		const float DamageLog = FMath::Loge(FMath::Max(Damage, 1.0f));
		// log(10) ≈ 2.3, log(100) ≈ 4.6, log(500) ≈ 6.2
		DamageScale = FMath::GetMappedRangeValueClamped(FVector2D(2.0f, 6.0f), FVector2D(0.8f, 1.5f), DamageLog);
	}

	// 위치 설정
	SetPositionInViewport(ScreenPosition, false);
	SetRenderOpacity(1.0f);

	// 데미지 스케일 저장 (애니메이션에서 사용)
	CachedDamageScale = DamageScale;

	// 초기 스케일 설정 (거리 스케일 포함)
	const float InitialScale = CurrentStyle.Scale * DamageScale * DistanceScale;
	SetRenderScale(FVector2D(InitialScale));

	// 텍스트 설정
	UpdateDamageText(Damage);

	// 색상 설정
	if (DamageText)
	{
		DamageText->SetColorAndOpacity(FSlateColor(CurrentStyle.Color));
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
}

const FDamageNumberStyle& UGS_DamageNumberWidget::GetStyleForType(EDamageNumberType Type) const
{
	switch (Type)
	{
	case EDamageNumberType::Critical:
		return CriticalStyle;
	case EDamageNumberType::DoT:
		return DoTStyle;
	case EDamageNumberType::Heal:
		return HealStyle;
	case EDamageNumberType::Normal:
	default:
		return NormalStyle;
	}
}

void UGS_DamageNumberWidget::Deactivate()
{
	bIsActive = false;
	SetVisibility(ESlateVisibility::Collapsed);

	// 누적 시스템 상태 리셋
	TargetActor = nullptr;
	TotalAccumulatedDamage = 0.f;

	// 델리게이트 실행
	OnAnimationComplete.ExecuteIfBound(this);
}

bool UGS_DamageNumberWidget::AddDamage(float AdditionalDamage)
{
	if (!bIsActive || AdditionalDamage <= 0.f)
	{
		return false;
	}

	// 누적 데미지 업데이트
	TotalAccumulatedDamage += AdditionalDamage;

	// 텍스트 업데이트
	UpdateDamageText(TotalAccumulatedDamage);

	// 누적 시 데미지 스케일 재계산
	if (CurrentType != EDamageNumberType::Heal)
	{
		const float DamageLog = FMath::Loge(FMath::Max(TotalAccumulatedDamage, 1.0f));
		CachedDamageScale = FMath::GetMappedRangeValueClamped(FVector2D(2.0f, 6.0f), FVector2D(0.8f, 1.5f), DamageLog);
	}

	// 펄스 효과 시작 (시각적 피드백)
	AccumulationPulseTime = 0.f;

	// 애니메이션 시간 연장 (최소 0.5초 남도록)
	const float RemainingTime = TotalDuration - ElapsedTime;
	if (RemainingTime < 0.5f)
	{
		TotalDuration = ElapsedTime + 0.5f;
	}

	return true;
}

void UGS_DamageNumberWidget::UpdateDamageText(float DamageAmount)
{
	if (!DamageText)
	{
		return;
	}

	FString DisplayText;
	if (CurrentType == EDamageNumberType::Heal)
	{
		DisplayText = FString::Printf(TEXT("+%d"), FMath::RoundToInt(DamageAmount));
	}
	else
	{
		DisplayText = FString::Printf(TEXT("%d"), FMath::RoundToInt(DamageAmount));
	}
	DamageText->SetText(FText::FromString(DisplayText));
}
