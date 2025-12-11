// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/RTS/GS_RTSSkillSlotWidget.h"
#include "AI/RTS/Skill/GS_RTSSkillComponent.h"
#include "AI/RTS/Skill/GS_RTSSkillBase.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

void UGS_RTSSkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SkillButton)
	{
		SkillButton->OnClicked.AddDynamic(this, &UGS_RTSSkillSlotWidget::OnSkillButtonClicked);
		SkillButton->OnHovered.AddDynamic(this, &UGS_RTSSkillSlotWidget::OnSkillButtonHovered);
		SkillButton->OnUnhovered.AddDynamic(this, &UGS_RTSSkillSlotWidget::OnSkillButtonUnhovered);
	}

	bIsOnCooldown = false;
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
