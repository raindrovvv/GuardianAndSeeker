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
	CriticalStyle.Scale = 1.5f;
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

	// 애니메이션 진행률 (0~1)
	const float Progress = FMath::Clamp(ElapsedTime / TotalDuration, 0.0f, 1.0f);

	// 위로 상승 애니메이션 (기본 위치 계산)
	const float YOffset = Progress * FloatSpeed * TotalDuration;
	FVector2D CurrentPosition = StartPosition;
	CurrentPosition.Y -= YOffset;

	// 흔들림 효과 (처음 0.3초 동안만, 기본 위치에 오프셋 추가)
	if (CurrentStyle.bShake && ElapsedTime < 0.3f)
	{
		const float ShakeIntensity = 3.0f * (1.0f - ElapsedTime / 0.3f);
		CurrentPosition.X += CachedShakeOffset.X * ShakeIntensity;
		CurrentPosition.Y += CachedShakeOffset.Y * ShakeIntensity;
	}

	// 위젯 위치 설정 (한 번만 호출)
	SetPositionInViewport(CurrentPosition, false);

	// 펀치 스케일 효과 (처음 0.15초 동안 - 더 빠르고 강렬하게)
	// 모든 타입에 기본 pop-in 효과, Critical은 더 강하게
	const float PopDuration = CurrentStyle.bPunchScale ? 0.15f : 0.1f;
	const float PopStrength = CurrentStyle.bPunchScale ? 0.5f : 0.2f; // Critical: 1.5x, Normal: 1.2x
	const float BaseScale = CurrentStyle.Scale * CachedDamageScale;

	if (ElapsedTime < PopDuration)
	{
		const float PopPhase = ElapsedTime / PopDuration;
		// 1.0 -> (1.0 + Strength) -> 1.0 커브
		const float PopMultiplier = 1.0f + PopStrength * FMath::Sin(PopPhase * PI);
		SetRenderScale(FVector2D(BaseScale * PopMultiplier));
	}
	else if (ElapsedTime < PopDuration + 0.05f) // pop 직후 리셋
	{
		SetRenderScale(FVector2D(BaseScale));
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

void UGS_DamageNumberWidget::ShowDamage(float Damage, EDamageNumberType Type, FVector2D ScreenPosition, float Duration)
{
	bIsActive = true;
	ElapsedTime = 0.0f;
	TotalDuration = Duration;
	StartPosition = ScreenPosition;
	CurrentType = Type;
	CurrentStyle = GetStyleForType(Type);

	// 흔들림 오프셋 미리 계산 (매 프레임 랜덤 방지)
	CachedShakeOffset.X = FMath::RandRange(-1.0f, 1.0f);
	CachedShakeOffset.Y = FMath::RandRange(-1.0f, 1.0f);

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
	SetRenderScale(FVector2D(CurrentStyle.Scale * DamageScale));

	// 데미지 스케일 저장 (애니메이션에서 사용)
	CachedDamageScale = DamageScale;

	// 텍스트 설정
	if (DamageText)
	{
		// 데미지/힐 표시 (힐은 +, 데미지는 숫자만)
		FString DisplayText;
		if (Type == EDamageNumberType::Heal)
		{
			DisplayText = FString::Printf(TEXT("+%d"), FMath::RoundToInt(Damage));
		}
		else
		{
			DisplayText = FString::Printf(TEXT("%d"), FMath::RoundToInt(Damage));
		}
		DamageText->SetText(FText::FromString(DisplayText));

		// 색상 설정
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

	// 델리게이트 실행
	OnAnimationComplete.ExecuteIfBound(this);
}
