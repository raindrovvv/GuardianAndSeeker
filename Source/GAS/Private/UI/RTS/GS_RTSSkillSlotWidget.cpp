// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/RTS/GS_RTSSkillSlotWidget.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillComponent.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillBase.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/SizeBox.h"

void UGS_RTSSkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// UI 컴포넌트가 블루프린트에서 바인딩되지 않았으면 C++로 생성
	if (!SkillButton || !SkillIcon || !CooldownOverlay || !CooldownText || !HotkeyText || !EtherCostText)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkillSlotWidget::NativeConstruct - Creating UI components dynamically"));
		CreateUIComponents();
	}

	if (SkillButton)
	{
		SkillButton->OnClicked.AddDynamic(this, &UGS_RTSSkillSlotWidget::OnSkillButtonClicked);
		SkillButton->OnHovered.AddDynamic(this, &UGS_RTSSkillSlotWidget::OnSkillButtonHovered);
		SkillButton->OnUnhovered.AddDynamic(this, &UGS_RTSSkillSlotWidget::OnSkillButtonUnhovered);
	}

	bIsOnCooldown = false;
}

void UGS_RTSSkillSlotWidget::CreateUIComponents()
{
	// SizeBox로 고정 크기 설정
	USizeBox* RootSizeBox = NewObject<USizeBox>(this);
	if (!RootSizeBox)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create root size box"));
		return;
	}

	// 슬롯 크기 설정 (64x64 픽셀)
	RootSizeBox->SetWidthOverride(64.f);
	RootSizeBox->SetHeightOverride(64.f);

	// 루트 Overlay 생성
	UOverlay* RootOverlay = NewObject<UOverlay>(this);
	if (!RootOverlay)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create root overlay"));
		return;
	}

	// SizeBox에 Overlay 추가
	RootSizeBox->AddChild(RootOverlay);

	// 1. 스킬 버튼 생성 (배경) - 디버그용 밝은 노란색
	SkillButton = NewObject<UButton>(this);
	if (SkillButton)
	{
		// 버튼 배경색 설정 (디버그용 노란색)
		SkillButton->SetBackgroundColor(FLinearColor(1.0f, 1.0f, 0.0f, 1.0f)); // 노란색

		UOverlaySlot* ButtonSlot = RootOverlay->AddChildToOverlay(SkillButton);
		if (ButtonSlot)
		{
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// 스킬 아이콘을 버튼 안에 추가
		SkillIcon = NewObject<UImage>(this);
		if (SkillIcon)
		{
			// 기본 아이콘 색상 (디버그용 녹색)
			SkillIcon->SetColorAndOpacity(FLinearColor(0.0f, 1.0f, 0.0f, 1.0f));
			SkillButton->AddChild(SkillIcon);
		}
	}

	// 2. 쿨다운 오버레이 (프로그레스 바)
	CooldownOverlay = NewObject<UProgressBar>(this);
	if (CooldownOverlay)
	{
		CooldownOverlay->SetPercent(0.f);
		CooldownOverlay->SetVisibility(ESlateVisibility::Hidden);

		UOverlaySlot* CooldownSlot = RootOverlay->AddChildToOverlay(CooldownOverlay);
		if (CooldownSlot)
		{
			CooldownSlot->SetHorizontalAlignment(HAlign_Fill);
			CooldownSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// 3. 쿨다운 텍스트
	CooldownText = NewObject<UTextBlock>(this);
	if (CooldownText)
	{
		CooldownText->SetVisibility(ESlateVisibility::Hidden);
		FSlateFontInfo FontInfo;
		FontInfo.Size = 24;
		CooldownText->SetFont(FontInfo);
		CooldownText->SetJustification(ETextJustify::Center);

		UOverlaySlot* TextSlot = RootOverlay->AddChildToOverlay(CooldownText);
		if (TextSlot)
		{
			TextSlot->SetHorizontalAlignment(HAlign_Center);
			TextSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	// 4. 단축키 텍스트 (좌상단)
	HotkeyText = NewObject<UTextBlock>(this);
	if (HotkeyText)
	{
		FSlateFontInfo FontInfo;
		FontInfo.Size = 14;
		HotkeyText->SetFont(FontInfo);

		UOverlaySlot* HotkeySlot = RootOverlay->AddChildToOverlay(HotkeyText);
		if (HotkeySlot)
		{
			HotkeySlot->SetHorizontalAlignment(HAlign_Left);
			HotkeySlot->SetVerticalAlignment(VAlign_Top);
			HotkeySlot->SetPadding(FMargin(5.f, 5.f, 0.f, 0.f));
		}
	}

	// 5. 에테르 비용 텍스트 (우하단)
	EtherCostText = NewObject<UTextBlock>(this);
	if (EtherCostText)
	{
		FSlateFontInfo FontInfo;
		FontInfo.Size = 14;
		EtherCostText->SetFont(FontInfo);

		UOverlaySlot* CostSlot = RootOverlay->AddChildToOverlay(EtherCostText);
		if (CostSlot)
		{
			CostSlot->SetHorizontalAlignment(HAlign_Right);
			CostSlot->SetVerticalAlignment(VAlign_Bottom);
			CostSlot->SetPadding(FMargin(0.f, 0.f, 5.f, 5.f));
		}
	}

	// 루트 위젯 설정
	if (UPanelWidget* RootWidget = Cast<UPanelWidget>(GetRootWidget()))
	{
		RootWidget->ClearChildren();
		RootWidget->AddChild(RootSizeBox);
	}

	UE_LOG(LogTemp, Log, TEXT("UGS_RTSSkillSlotWidget::CreateUIComponents - UI components created successfully (64x64)"));

	// 디버그: 위젯 크기 확인
	if (RootSizeBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("  └─ RootSizeBox: Width=%.1f, Height=%.1f"),
			RootSizeBox->GetWidthOverride(), RootSizeBox->GetHeightOverride());
	}
}

void UGS_RTSSkillSlotWidget::NativeDestruct()
{
	if (SkillComponent.IsValid())
	{
		SkillComponent->OnSkillCooldownChanged.RemoveDynamic(this, &UGS_RTSSkillSlotWidget::HandleCooldownChanged);
		SkillComponent->OnEtherChanged.RemoveDynamic(this, &UGS_RTSSkillSlotWidget::HandleEtherChanged);
	}

	Super::NativeDestruct();
}

void UGS_RTSSkillSlotWidget::InitializeSlot(int32 InSlotIndex, UGS_RTSSkillComponent* InSkillComponent)
{
	if (SkillComponent.IsValid())
	{
		SkillComponent->OnSkillCooldownChanged.RemoveDynamic(this, &UGS_RTSSkillSlotWidget::HandleCooldownChanged);
		SkillComponent->OnEtherChanged.RemoveDynamic(this, &UGS_RTSSkillSlotWidget::HandleEtherChanged);
	}

	SlotIndex = InSlotIndex;
	SkillComponent = InSkillComponent;

	if (SkillComponent.IsValid())
	{
		SkillComponent->OnSkillCooldownChanged.AddDynamic(this, &UGS_RTSSkillSlotWidget::HandleCooldownChanged);
		SkillComponent->OnEtherChanged.AddDynamic(this, &UGS_RTSSkillSlotWidget::HandleEtherChanged);
	}

	RefreshSkillInfo();

	// 단축키 표시 (1, 2, 3, 4)
	if (HotkeyText)
	{
		HotkeyText->SetText(FText::AsNumber(SlotIndex + 1));
	}
}

void UGS_RTSSkillSlotWidget::RefreshSkillInfo()
{
	if (!SkillComponent.IsValid())
	{
		return;
	}

	UGS_RTSSkillBase* Skill = SkillComponent->GetSkill(SlotIndex);
	if (!Skill)
	{
		return;
	}

	// 스킬 아이콘 설정
	if (SkillIcon && Skill->GetSkillIcon())
	{
		SkillIcon->SetBrushFromTexture(Skill->GetSkillIcon());
	}

	// 에테르 비용 표시
	if (EtherCostText)
	{
		EtherCostText->SetText(FText::AsNumber(FMath::RoundToInt(Skill->GetEtherCost())));
	}

	// 초기 상태로 쿨다운 UI 숨김
	if (CooldownOverlay)
	{
		CooldownOverlay->SetPercent(0.f);
		CooldownOverlay->SetVisibility(ESlateVisibility::Hidden);
	}

	if (CooldownText)
	{
		CooldownText->SetVisibility(ESlateVisibility::Hidden);
	}

	// 사용 가능 여부 업데이트
	HandleEtherChanged(SkillComponent->GetCurrentEther(), SkillComponent->GetMaxEther());
}

void UGS_RTSSkillSlotWidget::UpdateCooldownDisplay(float CurrentCooldown, float MaxCooldown)
{
	bIsOnCooldown = CurrentCooldown > 0.f;

	if (CooldownOverlay)
	{
		if (bIsOnCooldown && MaxCooldown > 0.f)
		{
			CooldownOverlay->SetVisibility(ESlateVisibility::Visible);
			CooldownOverlay->SetPercent(CurrentCooldown / MaxCooldown);
		}
		else
		{
			CooldownOverlay->SetVisibility(ESlateVisibility::Hidden);
			CooldownOverlay->SetPercent(0.f);
		}
	}

	if (CooldownText)
	{
		if (bIsOnCooldown)
		{
			CooldownText->SetVisibility(ESlateVisibility::Visible);
			CooldownText->SetText(FText::AsNumber(FMath::CeilToInt(CurrentCooldown)));
		}
		else
		{
			CooldownText->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UGS_RTSSkillSlotWidget::SetSkillAvailable(bool bAvailable)
{
	if (SkillButton)
	{
		SkillButton->SetIsEnabled(bAvailable && !bIsOnCooldown);
	}

	// 비용 텍스트 색상 변경
	if (EtherCostText)
	{
		FSlateColor Color = bAvailable ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor::Red);
		EtherCostText->SetColorAndOpacity(Color);
	}
}

void UGS_RTSSkillSlotWidget::SetTargetingActive(bool bActive)
{
	// 타겟팅 모드일 때 시각적 피드백
	if (SkillButton)
	{
		// TODO: 타겟팅 모드 시각 효과 (테두리 색 변경 등)
	}
}

void UGS_RTSSkillSlotWidget::OnSkillButtonClicked()
{
	if (SkillComponent.IsValid())
	{
		SkillComponent->TryActivateSkill(SlotIndex);
	}
}

void UGS_RTSSkillSlotWidget::OnSkillButtonHovered()
{
	// TODO: 스킬 툴팁 표시
}

void UGS_RTSSkillSlotWidget::OnSkillButtonUnhovered()
{
	// TODO: 스킬 툴팁 숨김
}

void UGS_RTSSkillSlotWidget::HandleCooldownChanged(int32 InSkillIndex, float RemainingCooldown, float MaxCooldown)
{
	if (InSkillIndex == SlotIndex)
	{
		UpdateCooldownDisplay(RemainingCooldown, MaxCooldown);
	}
}

void UGS_RTSSkillSlotWidget::HandleEtherChanged(float CurrentEther, float MaxEther)
{
	if (!SkillComponent.IsValid())
	{
		return;
	}

	UGS_RTSSkillBase* Skill = SkillComponent->GetSkill(SlotIndex);
	if (!Skill)
	{
		return;
	}

	bool bCanAfford = CurrentEther >= Skill->GetEtherCost();
	SetSkillAvailable(bCanAfford);
}
