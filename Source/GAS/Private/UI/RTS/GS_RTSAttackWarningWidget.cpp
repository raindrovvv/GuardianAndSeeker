// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/RTS/GS_RTSAttackWarningWidget.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

void UGS_RTSAttackWarningWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!WarningText)
	{
		UE_LOG(LogTemp, Error, TEXT("WarningText is NOT bound in GS_RTSAttackWarningWidget! Make sure you have a TextBlock named 'WarningText' in your Widget Blueprint."));
	}

	// Start hidden
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGS_RTSAttackWarningWidget::NativeDestruct()
{
	// Clear timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(HideTimer);
	}

	Super::NativeDestruct();
}

void UGS_RTSAttackWarningWidget::ShowWarning()
{
	// Cancel existing timer (implement "latest only" behavior)
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(HideTimer);
	}

	// Show widget
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// Set auto-hide timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			HideTimer,
			this,
			&UGS_RTSAttackWarningWidget::HideWarning,
			DisplayDuration,
			false
		);
	}
}

void UGS_RTSAttackWarningWidget::HideWarning()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
