// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Interaction/GS_InteractionWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

void UGS_InteractionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 초기화 시 모든 상호작용 UI 숨김
	if (InteractionPromptBox)
	{
		InteractionPromptBox->SetVisibility(ESlateVisibility::Hidden);
	}

	if (NearbyIndicatorBox)
	{
		NearbyIndicatorBox->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UGS_InteractionWidget::ShowInteraction(AActor* Target, float Duration, const FText& ActionText)
{
	if (InteractionPromptBox)
	{
		InteractionPromptBox->SetVisibility(ESlateVisibility::Visible);
	}
	
	// 근처 알림은 숨기기 (상호작용 중이므로)
	HideNearbyIndicator();

	if (Text_ActionName)
	{
		Text_ActionName->SetText(ActionText);
	}

	if (ProgressBar_Interaction)
	{
		ProgressBar_Interaction->SetPercent(0.0f);
	}
}

void UGS_InteractionWidget::HideInteraction()
{
	if (InteractionPromptBox)
	{
		InteractionPromptBox->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UGS_InteractionWidget::UpdateProgress(float Progress)
{
	if (ProgressBar_Interaction)
	{
		ProgressBar_Interaction->SetPercent(Progress);
	}
}

void UGS_InteractionWidget::OnInteractionComplete()
{
	// 완료 시 깜빡임 효과 등을 넣을 수 있음
	HideInteraction();
}

void UGS_InteractionWidget::OnInteractionCancelled()
{
	HideInteraction();
}

void UGS_InteractionWidget::ShowNearbyIndicator(AActor* Target, const FText& ActionText)
{
	// 이미 상호작용 중이면 근처 알림 무시
	if (InteractionPromptBox && InteractionPromptBox->IsVisible())
	{
		return;
	}

	if (NearbyIndicatorBox)
	{
		NearbyIndicatorBox->SetVisibility(ESlateVisibility::Visible);
	}

	if (Text_NearbyInfo)
	{
		// 예: "E 획득"
		FText FullText = FText::Format(NSLOCTEXT("Interaction", "NearbyFormat", "E {0}"), ActionText);
		Text_NearbyInfo->SetText(FullText);
	}
}

void UGS_InteractionWidget::HideNearbyIndicator()
{
	if (NearbyIndicatorBox)
	{
		NearbyIndicatorBox->SetVisibility(ESlateVisibility::Hidden);
	}
}
