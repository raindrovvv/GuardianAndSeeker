// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Character/GS_HitIndicatorWidget.h"
#include "Character/Component/GS_HitIndicatorComponent.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/GS_Character.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"

UGS_HitIndicatorWidget::UGS_HitIndicatorWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UGS_HitIndicatorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 모든 인디케이터 초기 숨김
	if (FrontIndicator)
		FrontIndicator->SetRenderOpacity(0.f);
	if (BackIndicator)
		BackIndicator->SetRenderOpacity(0.f);
	if (LeftIndicator)
		LeftIndicator->SetRenderOpacity(0.f);
	if (RightIndicator)
		RightIndicator->SetRenderOpacity(0.f);
	if (TopGradient)
		TopGradient->SetRenderOpacity(0.f);
	if (BottomGradient)
		BottomGradient->SetRenderOpacity(0.f);
	if (LeftGradient)
		LeftGradient->SetRenderOpacity(0.f);
	if (RightGradient)
		RightGradient->SetRenderOpacity(0.f);
}

void UGS_HitIndicatorWidget::NativeDestruct()
{
	// 델리게이트 언바인드
	if (OwnerComponent.IsValid())
	{
		OwnerComponent->OnDamageDirectionReceived.RemoveDynamic(
		    this, &UGS_HitIndicatorWidget::OnDamageDirectionReceived);
	}

	Super::NativeDestruct();
}

void UGS_HitIndicatorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 활성 인디케이터가 없으면 연산 스킵
	if (ActiveIndicators.Num() > 0)
	{
		UpdateIndicators(InDeltaTime);
	}
}

void UGS_HitIndicatorWidget::SetOwnerComponent(UGS_HitIndicatorComponent* InComponent)
{
	OwnerComponent = InComponent;

	if (OwnerComponent.IsValid())
	{
		OwnerComponent->OnDamageDirectionReceived.AddDynamic(
		    this, &UGS_HitIndicatorWidget::OnDamageDirectionReceived);

		// MaxHP 캐싱
		if (AGS_Character* Character = Cast<AGS_Character>(OwnerComponent->GetOwner()))
		{
			if (UGS_StatComp* StatComp = Character->GetComponentByClass<UGS_StatComp>())
			{
				CachedMaxHealth = StatComp->GetMaxHealth();
			}
		}
	}
}

void UGS_HitIndicatorWidget::ShowHitIndicator(EHitDirection Direction, float DamageAmount)
{
	if (Direction == EHitDirection::None)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	float CurrentTime = World->GetTimeSeconds();

	// 기존 활성 인디케이터 찾기 (같은 방향)
	FActiveIndicatorInfo* ExistingInfo = ActiveIndicators.FindByPredicate(
	    [Direction](const FActiveIndicatorInfo& Info)
	    {
		    return Info.Direction == Direction;
	    });

	if (ExistingInfo)
	{
		// 연속 피격 체크
		if (CurrentTime - ExistingInfo->LastHitTime < ConsecutiveHitWindow)
		{
			// 강화 (최대 3단계)
			ExistingInfo->IntensityLevel = FMath::Min(ExistingInfo->IntensityLevel + 1, 3);
		}

		// 시간 리셋
		ExistingInfo->RemainingTime = DefaultDisplayDuration;
		ExistingInfo->LastHitTime = CurrentTime;
	}
	else
	{
		// 새 인디케이터 추가
		FActiveIndicatorInfo NewInfo;
		NewInfo.Direction = Direction;
		NewInfo.RemainingTime = DefaultDisplayDuration;
		NewInfo.IntensityLevel = 1;
		NewInfo.LastHitTime = CurrentTime;
		ActiveIndicators.Add(NewInfo);
	}

	// 즉시 시각적 업데이트
	FLinearColor Color = CalculateIndicatorColor(DamageAmount);

	// 수평 방향 (화살표)
	if (Direction == EHitDirection::Front || Direction == EHitDirection::Back ||
	    Direction == EHitDirection::Left || Direction == EHitDirection::Right)
	{
		if (UImage* Indicator = GetIndicatorForDirection(Direction))
		{
			Indicator->SetColorAndOpacity(Color);
			Indicator->SetRenderOpacity(Color.A);
			UE_LOG(LogTemp, Warning, TEXT("[HitIndicator Widget] Showing Indicator for Direction %d, Opacity: %.2f"),
			       (int32)Direction, Color.A);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[HitIndicator Widget] Indicator is NULL for Direction %d!"), (int32)Direction);
		}
	}
	// 수직 방향 (그레디언트)
	else if (Direction == EHitDirection::Up || Direction == EHitDirection::Down)
	{
		if (UImage* Gradient = GetGradientForDirection(Direction))
		{
			Gradient->SetColorAndOpacity(Color);
			Gradient->SetRenderOpacity(Color.A);
		}
	}
	// 전 방향 (화면 모든 테두리 그레디언트)
	else if (Direction == EHitDirection::Omni)
	{
		UImage* Gradients[] = {TopGradient, BottomGradient, LeftGradient, RightGradient};
		for (UImage* Grad : Gradients)
		{
			if (Grad)
			{
				Grad->SetColorAndOpacity(Color);
				Grad->SetRenderOpacity(Color.A);
			}
		}
	}
}

void UGS_HitIndicatorWidget::OnDamageDirectionReceived(EHitDirection Direction, float DamageAmount)
{
	ShowHitIndicator(Direction, DamageAmount);
}

UImage* UGS_HitIndicatorWidget::GetIndicatorForDirection(EHitDirection Direction) const
{
	switch (Direction)
	{
	case EHitDirection::Front:
		return FrontIndicator;
	case EHitDirection::Back:
		return BackIndicator;
	case EHitDirection::Left:
		return LeftIndicator;
	case EHitDirection::Right:
		return RightIndicator;
	default:
		return nullptr;
	}
}

UImage* UGS_HitIndicatorWidget::GetGradientForDirection(EHitDirection Direction) const
{
	switch (Direction)
	{
	case EHitDirection::Up:
		return TopGradient;
	case EHitDirection::Down:
		return BottomGradient;
	case EHitDirection::Left:
		return LeftGradient;
	case EHitDirection::Right:
		return RightGradient;
	default:
		return nullptr;
	}
}

void UGS_HitIndicatorWidget::UpdateIndicators(float DeltaTime)
{
	for (int32 i = ActiveIndicators.Num() - 1; i >= 0; --i)
	{
		FActiveIndicatorInfo& Info = ActiveIndicators[i];
		Info.RemainingTime -= DeltaTime;

		float Alpha = 1.f;

		// 페이드 아웃 계산
		if (Info.RemainingTime <= FadeOutDuration)
		{
			Alpha = FMath::Max(0.f, Info.RemainingTime / FadeOutDuration);
		}

		// 수평 방향 업데이트
		if (Info.Direction == EHitDirection::Front || Info.Direction == EHitDirection::Back ||
		    Info.Direction == EHitDirection::Left || Info.Direction == EHitDirection::Right)
		{
			if (UImage* Indicator = GetIndicatorForDirection(Info.Direction))
			{
				Indicator->SetRenderOpacity(Alpha * IndicatorColor.A);

				// 강화 레벨에 따른 스케일
				float Scale = CalculateScaleFromIntensity(Info.IntensityLevel);
				Indicator->SetRenderScale(FVector2D(Scale, Scale));
			}
		}
		// 수직 방향 업데이트
		else if (Info.Direction == EHitDirection::Up || Info.Direction == EHitDirection::Down)
		{
			if (UImage* Gradient = GetGradientForDirection(Info.Direction))
			{
				Gradient->SetRenderOpacity(Alpha * IndicatorColor.A);
			}
		}
		// 전 방향 업데이트
		else if (Info.Direction == EHitDirection::Omni)
		{
			UImage* Gradients[] = {TopGradient, BottomGradient, LeftGradient, RightGradient};
			for (UImage* Grad : Gradients)
			{
				if (Grad)
				{
					Grad->SetRenderOpacity(Alpha * IndicatorColor.A);
				}
			}
		}

		// 만료된 인디케이터 제거
		if (Info.RemainingTime <= 0.f)
		{
			// 제거 전 opacity 0으로 초기화 (다음 히트 시 깔끔하게 시작)
			if (UImage* Indicator = GetIndicatorForDirection(Info.Direction))
			{
				Indicator->SetRenderOpacity(0.f);
				Indicator->SetRenderScale(FVector2D(1.f, 1.f));
			}
			if (UImage* Gradient = GetGradientForDirection(Info.Direction))
			{
				Gradient->SetRenderOpacity(0.f);
			}

			if (Info.Direction == EHitDirection::Omni)
			{
				UImage* Gradients[] = {TopGradient, BottomGradient, LeftGradient, RightGradient};
				for (UImage* Grad : Gradients)
				{
					if (Grad)
						Grad->SetRenderOpacity(0.f);
				}
			}

			ActiveIndicators.RemoveAt(i);
		}
	}
}

FLinearColor UGS_HitIndicatorWidget::CalculateIndicatorColor(float DamageAmount) const
{
	// 0 나누기 방지
	float SafeMaxHealth = FMath::Max(CachedMaxHealth, 1.f);
	float DamageRatio = DamageAmount / SafeMaxHealth;

	if (DamageRatio >= HeavyDamageThreshold)
	{
		// 강한 피격: 밝은 빨강 + 높은 불투명도
		return HeavyHitColor;
	}
	else if (DamageRatio >= LightDamageThreshold)
	{
		// 보통 피격: 기본 색상
		return IndicatorColor;
	}
	else
	{
		// 경미한 피격: 더 투명하게
		FLinearColor LightColor = IndicatorColor;
		LightColor.A *= 0.5f;
		return LightColor;
	}
}

float UGS_HitIndicatorWidget::CalculateScaleFromIntensity(int32 IntensityLevel) const
{
	// 1단계: 1.0, 2단계: 1.15, 3단계: 1.3
	return 1.f + (IntensityLevel - 1) * 0.15f;
}
