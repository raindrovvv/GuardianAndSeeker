// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Character/GS_DebuffIndicatorWidget.h"
#include "UI/Character/GS_DebuffIconWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

UGS_DebuffIndicatorWidget::UGS_DebuffIndicatorWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UGS_DebuffIndicatorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// IconContainer 유효성 확인
	if (!IconContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebuffIndicatorWidget] IconContainer (HorizontalBox) is not bound! Make sure to add a HorizontalBox named 'IconContainer' in the widget blueprint."));
	}
}

void UGS_DebuffIndicatorWidget::NativeDestruct()
{
	// 풀 정리 (위젯은 Outer에 의해 자동 정리되지만 참조는 제거)
	IconPool.Empty();

	Super::NativeDestruct();
}

void UGS_DebuffIndicatorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 아이콘 위젯의 틱은 각 위젯에서 자체 처리
}

void UGS_DebuffIndicatorWidget::UpdateDebuffIcons(const TArray<FDebuffRepInfo>& DebuffList, int32 MaxIcons)
{
	if (!IconContainer)
	{
		return;
	}

	// 위젯 클래스 확인
	if (!DebuffIconWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebuffIndicatorWidget] DebuffIconWidgetClass is not set!"));
		return;
	}

	// 표시할 아이콘 수 계산
	int32 IconsToShow = FMath::Min(DebuffList.Num(), MaxIcons);

	// 기존 활성 아이콘보다 더 많이 필요하면 추가 생성/활성화
	for (int32 i = 0; i < IconsToShow; ++i)
	{
		UGS_DebuffIconWidget* Icon = GetOrCreateIcon(i);
		if (!Icon)
		{
			continue;
		}

		const FDebuffRepInfo& DebuffInfo = DebuffList[i];
		bool bIsCC = IsCrowdControlDebuff(DebuffInfo.Type);

		// TotalDuration이 0이면 RemainingTime 기반 추정 사용 (하위 호환)
		float TotalDuration = DebuffInfo.TotalDuration > 0.f
		                          ? DebuffInfo.TotalDuration
		                          : FMath::Max(DebuffInfo.RemainingTime * 1.5f, 5.f);

		Icon->SetDebuffInfo(DebuffInfo.Type, DebuffInfo.RemainingTime, TotalDuration, bIsCC);
		Icon->ShowIcon();
	}

	// 남은 아이콘은 숨김
	for (int32 i = IconsToShow; i < IconPool.Num(); ++i)
	{
		if (IconPool[i])
		{
			IconPool[i]->HideIcon();
		}
	}

	ActiveIconCount = IconsToShow;
}

void UGS_DebuffIndicatorWidget::HideAllIcons()
{
	for (UGS_DebuffIconWidget* Icon : IconPool)
	{
		if (Icon)
		{
			Icon->HideIcon();
		}
	}
	ActiveIconCount = 0;
}

UGS_DebuffIconWidget* UGS_DebuffIndicatorWidget::GetOrCreateIcon(int32 Index)
{
	// 풀에 이미 있으면 반환
	if (IconPool.IsValidIndex(Index) && IconPool[Index])
	{
		return IconPool[Index];
	}

	// 위젯 클래스 확인
	if (!DebuffIconWidgetClass)
	{
		return nullptr;
	}

	// 새 아이콘 생성
	UGS_DebuffIconWidget* NewIcon = CreateWidget<UGS_DebuffIconWidget>(this, DebuffIconWidgetClass);
	if (!NewIcon)
	{
		UE_LOG(LogTemp, Error, TEXT("[DebuffIndicatorWidget] Failed to create DebuffIconWidget"));
		return nullptr;
	}

	// HorizontalBox에 추가
	if (IconContainer)
	{
		UHorizontalBoxSlot* IconSlot = IconContainer->AddChildToHorizontalBox(NewIcon);
		if (IconSlot)
		{
			IconSlot->SetPadding(FMargin(IconSpacing / 2.f, 0.f, IconSpacing / 2.f, 0.f));
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	// 풀에 추가
	if (IconPool.Num() <= Index)
	{
		IconPool.SetNum(Index + 1);
	}
	IconPool[Index] = NewIcon;

	return NewIcon;
}

bool UGS_DebuffIndicatorWidget::IsCrowdControlDebuff(EDebuffType Type) const
{
	// 중앙 집중화된 유틸리티 함수로 위임
	return FDebuffTypeUtils::IsCrowdControl(Type);
}
