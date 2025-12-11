// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/RTS/GS_RTSSkillBarWidget.h"
#include "UI/RTS/GS_RTSSkillSlotWidget.h"
#include "AI/RTS/Skill/GS_RTSSkillComponent.h"
#include "AI/RTS/Skill/GS_RTSSkillBase.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UGS_RTSSkillBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
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
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkillBarWidget: SkillComponent is invalid"));
		return;
	}

	if (!SkillSlotContainer)
	{
		UE_LOG(LogTemp, Error, TEXT("UGS_RTSSkillBarWidget: SkillSlotContainer is missing. Check BindWidget in Blueprint."));
		return;
	}

	if (!SkillSlotWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("UGS_RTSSkillBarWidget: SkillSlotWidgetClass is not set. Please assign BP_RTSSkillSlotWidget in details panel."));
		return;
	}

	// 기존 슬롯 제거
	SkillSlotContainer->ClearChildren();
	SkillSlots.Empty();

	// 스킬 개수만큼 슬롯 생성
	const int32 SkillCount = SkillComponent->GetSkillCount();
	if (SkillCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkillBarWidget: SkillComponent has 0 skills. Check SkillDataAssets in component."));
	}

	for (int32 i = 0; i < SkillCount; ++i)
	{
		UGS_RTSSkillSlotWidget* SlotWidget = CreateWidget<UGS_RTSSkillSlotWidget>(this, SkillSlotWidgetClass);
		if (SlotWidget)
		{
			SlotWidget->InitializeSlot(i, SkillComponent.Get());
			
			UHorizontalBoxSlot* BoxSlot = SkillSlotContainer->AddChildToHorizontalBox(SlotWidget);
			if (BoxSlot)
			{
				BoxSlot->SetPadding(FMargin(5.f, 0.f, 5.f, 0.f));
				BoxSlot->SetHorizontalAlignment(HAlign_Center);
				BoxSlot->SetVerticalAlignment(VAlign_Center);
			}

			SkillSlots.Add(SlotWidget);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("UGS_RTSSkillBarWidget: Created %d skill slots"), SkillSlots.Num());
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
