// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/RTS/GS_RTSSkillBarWidget.h"
#include "UI/RTS/GS_RTSSkillSlotWidget.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillComponent.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillBase.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"

void UGS_RTSSkillBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 블루프린트에서 바인딩된 UI 컴포넌트 검증
	if (!AetherProgressBar)
	{
		UE_LOG(LogTemp, Error, TEXT("UGS_RTSSkillBarWidget::NativeConstruct - AetherProgressBar is not bound! Please bind it in WBP"));
	}

	if (!AetherText)
	{
		UE_LOG(LogTemp, Error, TEXT("UGS_RTSSkillBarWidget::NativeConstruct - AetherText is not bound! Please bind it in WBP"));
	}

	if (!SkillSlotContainer)
	{
		UE_LOG(LogTemp, Error, TEXT("UGS_RTSSkillBarWidget::NativeConstruct - SkillSlotContainer is not bound! Please bind it in WBP"));
	}

	SetVisibility(ESlateVisibility::Visible);
}

// CreateUIComponents 함수 제거 - 블루프린트에서 UI 계층 구조 생성

void UGS_RTSSkillBarWidget::NativeDestruct()
{
	if (SkillComponent.IsValid())
	{
		SkillComponent->OnAetherChanged.RemoveDynamic(this, &UGS_RTSSkillBarWidget::HandleAetherChanged);
		SkillComponent->OnSkillCooldownChanged.RemoveDynamic(this, &UGS_RTSSkillBarWidget::HandleSkillCooldownChanged);
		SkillComponent->OnSkillActivationStateChanged.RemoveDynamic(this, &UGS_RTSSkillBarWidget::HandleSkillActivationStateChanged);
	}

	Super::NativeDestruct();
}

void UGS_RTSSkillBarWidget::InitializeSkillBar(UGS_RTSSkillComponent* InSkillComponent)
{
	SkillComponent = InSkillComponent;

	if (!SkillComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkillBarWidget::InitializeSkillBar - Invalid skill component"));
		return;
	}

	BindToSkillComponent();
	CreateSkillSlots();

	// 초기 에테르 바 업데이트
	UpdateAetherBar(SkillComponent->GetCurrentAether(), SkillComponent->GetMaxAether());
}

void UGS_RTSSkillBarWidget::BindToSkillComponent()
{
	if (!SkillComponent.IsValid())
	{
		return;
	}

	SkillComponent->OnAetherChanged.AddDynamic(this, &UGS_RTSSkillBarWidget::HandleAetherChanged);
	SkillComponent->OnSkillCooldownChanged.AddDynamic(this, &UGS_RTSSkillBarWidget::HandleSkillCooldownChanged);
	SkillComponent->OnSkillActivationStateChanged.AddDynamic(this, &UGS_RTSSkillBarWidget::HandleSkillActivationStateChanged);
}

void UGS_RTSSkillBarWidget::CreateSkillSlots()
{
	if (!SkillComponent.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("CreateSkillSlots: SkillComponent is invalid"));
		return;
	}

	if (!SkillSlotContainer)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateSkillSlots: SkillSlotContainer is not bound! Please bind it in WBP_RTSSkillBarWidget"));
		return;
	}

	// 위젯 클래스 검증
	TSubclassOf<UGS_RTSSkillSlotWidget> WidgetClass = SkillSlotWidgetClass;
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateSkillSlots: SkillSlotWidgetClass not set! Please set WBP_RTSSkillSlotWidget in WBP_RTSSkillBarWidget"));
		return;
	}

	// 기존 슬롯 제거
	SkillSlotContainer->ClearChildren();
	SkillSlots.Empty();

	// 스킬 개수 확인
	int32 SkillCount = SkillComponent->GetSkillCount();
	if (SkillCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateSkillSlots: No skills to display"));
		return;
	}

	// 슬롯 생성
	for (int32 i = 0; i < SkillCount; ++i)
	{
		UGS_RTSSkillSlotWidget* SlotWidget = CreateWidget<UGS_RTSSkillSlotWidget>(this, WidgetClass);
		if (!SlotWidget)
		{
			UE_LOG(LogTemp, Error, TEXT("CreateSkillSlots: Failed to create slot widget %d"), i);
			continue;
		}

		// 초기화
		SlotWidget->InitializeSlot(i, SkillComponent.Get());

		// HorizontalBox에 추가
		UHorizontalBoxSlot* BoxSlot = SkillSlotContainer->AddChildToHorizontalBox(SlotWidget);
		if (BoxSlot)
		{
			BoxSlot->SetPadding(FMargin(5.f, 0.f, 5.f, 0.f));
			BoxSlot->SetHorizontalAlignment(HAlign_Center);
			BoxSlot->SetVerticalAlignment(VAlign_Center);
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		SkillSlots.Add(SlotWidget);
	}
}

void UGS_RTSSkillBarWidget::UpdateAetherBar(float CurrentAether, float MaxAether)
{
	if (AetherProgressBar)
	{
		float Percent = MaxAether > 0.f ? CurrentAether / MaxAether : 0.f;
		AetherProgressBar->SetPercent(Percent);
	}

	if (AetherText)
	{
		FString AetherString = FString::Printf(TEXT("%.0f / %.0f"), CurrentAether, MaxAether);
		AetherText->SetText(FText::FromString(AetherString));
	}
}

void UGS_RTSSkillBarWidget::UpdateSkillSlot(int32 SlotIndex)
{
	if (SkillSlots.IsValidIndex(SlotIndex) && SkillSlots[SlotIndex])
	{
		// 슬롯 위젯 자체 업데이트 트리거
		if (SkillComponent.IsValid())
		{
			float Remaining = SkillComponent->GetSkillCooldownRemaining(SlotIndex);
			UGS_RTSSkillBase* Skill = SkillComponent->GetSkill(SlotIndex);
			float MaxCooldown = Skill ? Skill->GetCooldownTime() : 0.f;
			
			SkillSlots[SlotIndex]->UpdateCooldownDisplay(Remaining, MaxCooldown);
		}
	}
}

void UGS_RTSSkillBarWidget::HandleAetherChanged(float CurrentAether, float MaxAether)
{
	UpdateAetherBar(CurrentAether, MaxAether);
}

void UGS_RTSSkillBarWidget::HandleSkillCooldownChanged(int32 SkillIndex, float RemainingCooldown, float MaxCooldown)
{
	if (SkillSlots.IsValidIndex(SkillIndex) && SkillSlots[SkillIndex])
	{
		SkillSlots[SkillIndex]->UpdateCooldownDisplay(RemainingCooldown, MaxCooldown);
	}
}

void UGS_RTSSkillBarWidget::HandleSkillActivationStateChanged(int32 SkillIndex, bool bIsActive)
{
	if (SkillSlots.IsValidIndex(SkillIndex) && SkillSlots[SkillIndex])
	{
		SkillSlots[SkillIndex]->SetTargetingActive(bIsActive);
	}
}
