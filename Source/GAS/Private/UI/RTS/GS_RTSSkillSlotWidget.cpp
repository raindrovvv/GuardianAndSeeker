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
#include "Blueprint/WidgetTree.h"

void UGS_RTSSkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 블루프린트에서 바인딩된 UI 컴포넌트 검증
	if (!SkillButton)
	{
		UE_LOG(LogTemp, Error, TEXT("UGS_RTSSkillSlotWidget::NativeConstruct - SkillButton is not bound! Please bind it in WBP"));
	}

	if (!SkillIcon)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkillSlotWidget::NativeConstruct - SkillIcon is not bound"));
	}

	// 버튼 이벤트 바인딩
	if (SkillButton)
	{
		SkillButton->OnClicked.AddDynamic(this, &UGS_RTSSkillSlotWidget::OnSkillButtonClicked);
		SkillButton->OnHovered.AddDynamic(this, &UGS_RTSSkillSlotWidget::OnSkillButtonHovered);
		SkillButton->OnUnhovered.AddDynamic(this, &UGS_RTSSkillSlotWidget::OnSkillButtonUnhovered);
	}

	bIsOnCooldown = false;
}

// CreateUIComponents 함수 제거 - 블루프린트에서 UI 계층 구조 생성

void UGS_RTSSkillSlotWidget::NativeDestruct()
{
	if (SkillComponent.IsValid())
	{
		SkillComponent->OnSkillCooldownChanged.RemoveDynamic(this, &UGS_RTSSkillSlotWidget::HandleCooldownChanged);
		SkillComponent->OnAetherChanged.RemoveDynamic(this, &UGS_RTSSkillSlotWidget::HandleAetherChanged);
	}

	Super::NativeDestruct();
}

void UGS_RTSSkillSlotWidget::InitializeSlot(int32 InSlotIndex, UGS_RTSSkillComponent* InSkillComponent)
{
	if (SkillComponent.IsValid())
	{
		SkillComponent->OnSkillCooldownChanged.RemoveDynamic(this, &UGS_RTSSkillSlotWidget::HandleCooldownChanged);
		SkillComponent->OnAetherChanged.RemoveDynamic(this, &UGS_RTSSkillSlotWidget::HandleAetherChanged);
	}

	SlotIndex = InSlotIndex;
	SkillComponent = InSkillComponent;

	if (SkillComponent.IsValid())
	{
		SkillComponent->OnSkillCooldownChanged.AddDynamic(this, &UGS_RTSSkillSlotWidget::HandleCooldownChanged);
		SkillComponent->OnAetherChanged.AddDynamic(this, &UGS_RTSSkillSlotWidget::HandleAetherChanged);
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
	if (AetherCostText)
	{
		AetherCostText->SetText(FText::AsNumber(FMath::RoundToInt(Skill->GetAetherCost())));
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
	HandleAetherChanged(SkillComponent->GetCurrentAether(), SkillComponent->GetMaxAether());
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
	if (AetherCostText)
	{
		FSlateColor Color = bAvailable ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor::Red);
		AetherCostText->SetColorAndOpacity(Color);
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

void UGS_RTSSkillSlotWidget::HandleAetherChanged(float CurrentAether, float MaxAether)
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

	bool bCanAfford = CurrentAether >= Skill->GetAetherCost();
	SetSkillAvailable(bCanAfford);
}
