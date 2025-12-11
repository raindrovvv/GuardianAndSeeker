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

void UGS_RTSSkillBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// UI 컴포넌트가 블루프린트에서 바인딩되지 않았으면 C++로 생성
	if (!EtherProgressBar || !EtherText || !SkillSlotContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkillBarWidget::NativeConstruct - Creating UI components dynamically"));
		CreateUIComponents();
	}

	// 위젯 가시성 강제 설정 (디버그)
	SetVisibility(ESlateVisibility::Visible);
	UE_LOG(LogTemp, Warning, TEXT("RTSSkillBarWidget Visibility set to Visible"));
}

void UGS_RTSSkillBarWidget::CreateUIComponents()
{
	// 디버그용 배경 Border 생성 (빨간색으로 명확하게 보이게)
	UBorder* DebugBorder = NewObject<UBorder>(this);
	if (!DebugBorder)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create debug border"));
		return;
	}

	// 빨간색 배경 설정 (디버그용)
	DebugBorder->SetBrushColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.8f)); // 빨간색 반투명
	DebugBorder->SetPadding(FMargin(10.f));

	// 루트 VerticalBox 생성
	UVerticalBox* RootVerticalBox = NewObject<UVerticalBox>(this);
	if (!RootVerticalBox)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create root vertical box"));
		return;
	}

	// Border에 VerticalBox 추가
	DebugBorder->AddChild(RootVerticalBox);

	// 1. 에테르 바 섹션 생성
	USizeBox* EtherBarSizeBox = NewObject<USizeBox>(this);
	if (EtherBarSizeBox)
	{
		EtherBarSizeBox->SetWidthOverride(300.f); // 에테르 바 너비
		EtherBarSizeBox->SetHeightOverride(40.f); // 에테르 바 높이

		UVerticalBoxSlot* EtherSizeBoxSlot = RootVerticalBox->AddChildToVerticalBox(EtherBarSizeBox);
		if (EtherSizeBoxSlot)
		{
			EtherSizeBoxSlot->SetPadding(FMargin(10.f, 5.f, 10.f, 5.f));
			EtherSizeBoxSlot->SetHorizontalAlignment(HAlign_Center);
			EtherSizeBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		UBorder* EtherBarBorder = NewObject<UBorder>(this);
		if (EtherBarBorder)
		{
			// 에테르 바 배경 색상 설정
			FLinearColor BorderColor(0.1f, 0.1f, 0.1f, 0.8f);
			EtherBarBorder->SetBrushColor(BorderColor);
			EtherBarSizeBox->AddChild(EtherBarBorder);

			// 에테르 바 오버레이 (프로그레스 바 + 텍스트)
			UOverlay* EtherOverlay = NewObject<UOverlay>(this);
			if (EtherOverlay)
			{
				EtherBarBorder->AddChild(EtherOverlay);

				// 에테르 프로그레스 바
				EtherProgressBar = NewObject<UProgressBar>(this);
				if (EtherProgressBar)
				{
					EtherProgressBar->SetPercent(1.0f);
					FLinearColor EtherColor(0.0f, 0.8f, 1.0f, 1.0f); // 청록색
					EtherProgressBar->SetFillColorAndOpacity(EtherColor);

					UOverlaySlot* ProgressSlot = EtherOverlay->AddChildToOverlay(EtherProgressBar);
					if (ProgressSlot)
					{
						ProgressSlot->SetHorizontalAlignment(HAlign_Fill);
						ProgressSlot->SetVerticalAlignment(VAlign_Fill);
					}
				}

				// 에테르 텍스트
				EtherText = NewObject<UTextBlock>(this);
				if (EtherText)
				{
					EtherText->SetText(FText::FromString(TEXT("0 / 0")));
					FSlateFontInfo FontInfo;
					FontInfo.Size = 18;
					EtherText->SetFont(FontInfo);
					EtherText->SetJustification(ETextJustify::Center);
					EtherText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

					UOverlaySlot* TextSlot = EtherOverlay->AddChildToOverlay(EtherText);
					if (TextSlot)
					{
						TextSlot->SetHorizontalAlignment(HAlign_Center);
						TextSlot->SetVerticalAlignment(VAlign_Center);
					}
				}
			}
		}
	}

	// 2. 스킬 슬롯 컨테이너 생성
	SkillSlotContainer = NewObject<UHorizontalBox>(this);
	if (SkillSlotContainer)
	{
		UVerticalBoxSlot* SkillContainerSlot = RootVerticalBox->AddChildToVerticalBox(SkillSlotContainer);
		if (SkillContainerSlot)
		{
			SkillContainerSlot->SetPadding(FMargin(10.f, 5.f, 10.f, 10.f));
			SkillContainerSlot->SetHorizontalAlignment(HAlign_Center);
			SkillContainerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	// 루트 위젯 설정
	if (UPanelWidget* RootWidget = Cast<UPanelWidget>(GetRootWidget()))
	{
		RootWidget->ClearChildren();
		RootWidget->AddChild(DebugBorder); // Border 추가 (VerticalBox는 이미 Border 안에 있음)
	}

	UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkillBarWidget::CreateUIComponents - UI components created with RED DEBUG BACKGROUND"));
}

void UGS_RTSSkillBarWidget::NativeDestruct()
{
	if (SkillComponent.IsValid())
	{
		SkillComponent->OnEtherChanged.RemoveDynamic(this, &UGS_RTSSkillBarWidget::HandleEtherChanged);
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
	UpdateEtherBar(SkillComponent->GetCurrentEther(), SkillComponent->GetMaxEther());
}

void UGS_RTSSkillBarWidget::BindToSkillComponent()
{
	if (!SkillComponent.IsValid())
	{
		return;
	}

	SkillComponent->OnEtherChanged.AddDynamic(this, &UGS_RTSSkillBarWidget::HandleEtherChanged);
	SkillComponent->OnSkillCooldownChanged.AddDynamic(this, &UGS_RTSSkillBarWidget::HandleSkillCooldownChanged);
	SkillComponent->OnSkillActivationStateChanged.AddDynamic(this, &UGS_RTSSkillBarWidget::HandleSkillActivationStateChanged);
}

void UGS_RTSSkillBarWidget::CreateSkillSlots()
{
	if (!SkillComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkillBarWidget::CreateSkillSlots - Invalid SkillComponent"));
		return;
	}

	// SkillSlotContainer가 없으면 생성
	if (!SkillSlotContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkillBarWidget::CreateSkillSlots - SkillSlotContainer is null, creating dynamically"));
		SkillSlotContainer = NewObject<UHorizontalBox>(this, UHorizontalBox::StaticClass());
		if (SkillSlotContainer)
		{
			// 루트 위젯에 추가 (WidgetTree 사용)
			if (UPanelWidget* RootWidget = Cast<UPanelWidget>(GetRootWidget()))
			{
				RootWidget->AddChild(SkillSlotContainer);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UGS_RTSSkillBarWidget::CreateSkillSlots - Failed to create SkillSlotContainer"));
			return;
		}
	}

	// 위젯 클래스가 없으면 기본 C++ 클래스 사용
	TSubclassOf<UGS_RTSSkillSlotWidget> WidgetClass = SkillSlotWidgetClass;
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkillBarWidget::CreateSkillSlots - SkillSlotWidgetClass not set, using default C++ class"));
		WidgetClass = UGS_RTSSkillSlotWidget::StaticClass();
	}

	// 기존 슬롯 제거
	SkillSlotContainer->ClearChildren();
	SkillSlots.Empty();

	// 스킬 개수만큼 슬롯 생성
	int32 SkillCount = SkillComponent->GetSkillCount();
	UE_LOG(LogTemp, Log, TEXT("UGS_RTSSkillBarWidget::CreateSkillSlots - Creating %d skill slots"), SkillCount);

	for (int32 i = 0; i < SkillCount; ++i)
	{
		UGS_RTSSkillSlotWidget* SlotWidget = CreateWidget<UGS_RTSSkillSlotWidget>(this, WidgetClass);
		if (SlotWidget)
		{
			SlotWidget->InitializeSlot(i, SkillComponent.Get());

			UHorizontalBoxSlot* BoxSlot = SkillSlotContainer->AddChildToHorizontalBox(SlotWidget);
			if (BoxSlot)
			{
				BoxSlot->SetPadding(FMargin(5.f, 0.f, 5.f, 0.f));
				BoxSlot->SetHorizontalAlignment(HAlign_Center);
				BoxSlot->SetVerticalAlignment(VAlign_Center);
				BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); // 자동 크기 조정
			}

			SkillSlots.Add(SlotWidget);
			UE_LOG(LogTemp, Log, TEXT("UGS_RTSSkillBarWidget::CreateSkillSlots - Created slot %d (64x64)"), i);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UGS_RTSSkillBarWidget::CreateSkillSlots - Failed to create slot widget %d"), i);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("UGS_RTSSkillBarWidget::CreateSkillSlots - Successfully created %d skill slots"), SkillSlots.Num());

	// 디버그: SkillSlotContainer 자식 개수 확인
	if (SkillSlotContainer)
	{
		int32 ChildCount = SkillSlotContainer->GetChildrenCount();
		UE_LOG(LogTemp, Warning, TEXT("  └─ SkillSlotContainer now has %d children"), ChildCount);

		if (ChildCount == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("  ❌ ERROR: SkillSlotContainer is empty! Slots were not added!"));
		}
	}
}

void UGS_RTSSkillBarWidget::UpdateEtherBar(float CurrentEther, float MaxEther)
{
	if (EtherProgressBar)
	{
		float Percent = MaxEther > 0.f ? CurrentEther / MaxEther : 0.f;
		EtherProgressBar->SetPercent(Percent);
	}

	if (EtherText)
	{
		FString EtherString = FString::Printf(TEXT("%.0f / %.0f"), CurrentEther, MaxEther);
		EtherText->SetText(FText::FromString(EtherString));
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

void UGS_RTSSkillBarWidget::HandleEtherChanged(float CurrentEther, float MaxEther)
{
	UpdateEtherBar(CurrentEther, MaxEther);
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
