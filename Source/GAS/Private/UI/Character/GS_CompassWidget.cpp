// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Character/GS_CompassWidget.h"
#include "UI/Character/GS_CompassIndicatorComponent.h"
#include "UI/Character/GS_TeammateIconWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UGS_CompassWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeDirections();
	GetWorld()->GetTimerManager().SetTimer(RefreshIconsTimerHandle, this, &UGS_CompassWidget::RefreshTeammateIcons, 1.0f, true, 0.5f);

	if (UpdateFrequency > 0)
	{
		const float UpdateInterval = 1.0f / UpdateFrequency;
		GetWorld()->GetTimerManager().SetTimer(CompassUpdateTimerHandle, this, &UGS_CompassWidget::UpdateCompass, UpdateInterval, true);
	}
}

void UGS_CompassWidget::UpdateCompass()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		const float CurrentYaw = PC->GetControlRotation().Yaw;
		UpdateCompassElements(CurrentYaw);
	}
}

void UGS_CompassWidget::InitializeDirections()
{
	if (!CompassCanvasPanel) return;

	Directions.Emplace(FCompassDirectionInfo{ FText::FromString(TEXT("N")), 0.0f });
	Directions.Emplace(FCompassDirectionInfo{ FText::FromString(TEXT("NE")), 45.0f });
	Directions.Emplace(FCompassDirectionInfo{ FText::FromString(TEXT("E")), 90.0f });
	Directions.Emplace(FCompassDirectionInfo{ FText::FromString(TEXT("SE")), 135.0f });
	Directions.Emplace(FCompassDirectionInfo{ FText::FromString(TEXT("S")), 180.0f });
	Directions.Emplace(FCompassDirectionInfo{ FText::FromString(TEXT("SW")), 225.0f });
	Directions.Emplace(FCompassDirectionInfo{ FText::FromString(TEXT("W")), 270.0f });
	Directions.Emplace(FCompassDirectionInfo{ FText::FromString(TEXT("NW")), 315.0f });

	for (auto& DirInfo : Directions)
	{
		UTextBlock* TextBlock = NewObject<UTextBlock>(this);
		TextBlock->SetText(DirInfo.Text);
		TextBlock->SetFont(DirectionTextFont);
		TextBlock->SetJustification(ETextJustify::Center);
		DirInfo.Widget = TextBlock;

		UCanvasPanelSlot* CanvasSlot = CompassCanvasPanel->AddChildToCanvas(TextBlock);
		if (CanvasSlot)
		{
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetAutoSize(true);
		}
	}
}

float UGS_CompassWidget::GetHorizontalPositionForAngle(float Angle, float PlayerYaw) const
{
	const float DeltaAngle = FMath::FindDeltaAngleDegrees(PlayerYaw, Angle);
	return (DeltaAngle / 45.0f) * DirectionSpacing;
}

void UGS_CompassWidget::UpdateCompassElements(float PlayerYaw)
{
	if (!CompassCanvasPanel) return;

	const float HalfWidth = CompassWidth / 2.0f;
	
	// Update Directions
	for (const auto& DirInfo : Directions)
	{
		if (DirInfo.Widget)
		{
			const float XPos = GetHorizontalPositionForAngle(DirInfo.Angle, PlayerYaw);
			const bool bIsVisible = FMath::Abs(XPos) <= HalfWidth;
			const ESlateVisibility NewVisibility = bIsVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

			if (DirInfo.Widget->GetVisibility() != NewVisibility)
			{
				DirInfo.Widget->SetVisibility(NewVisibility);
			}

			if (bIsVisible)
			{
				DirInfo.Widget->SetRenderTranslation(FVector2D(XPos, 0));
			}
		}
	}

	// Update Teammate Icons
	APawn* PlayerPawn = GetOwningPlayerPawn();
	if (!PlayerPawn) return;
	const FVector PlayerLocation = PlayerPawn->GetActorLocation();

	for (const auto& IconInfo : TeammateIcons)
	{
		UGS_CompassIndicatorComponent* Indicator = IconInfo.IndicatorComponent.Get();
		UGS_TeammateIconWidget* IconWidget = IconInfo.IconWidget;

		if (Indicator && IconWidget)
		{
			IconWidget->SetIconAppearance(Indicator);
			
			ESlateVisibility NewVisibility = ESlateVisibility::Collapsed;
			float XPos = 0.0f;
			bool bVisible = false;

			if (Indicator->IsValidForCompass())
			{
				const FVector TargetLocation = Indicator->GetWorldLocation();
				const float Distance = FVector::Dist(PlayerLocation, TargetLocation);

				if (Distance <= Indicator->GetMaxDisplayDistance())
				{
					const float TargetAngle = FMath::RadiansToDegrees(FMath::Atan2(TargetLocation.Y - PlayerLocation.Y, TargetLocation.X - PlayerLocation.X));
					XPos = GetHorizontalPositionForAngle(TargetAngle, PlayerYaw);
					NewVisibility = ESlateVisibility::HitTestInvisible;
					bVisible = true;
				}
			}

			if (IconWidget->GetVisibility() != NewVisibility)
			{
				IconWidget->SetVisibility(NewVisibility);
			}

			if (bVisible)
			{
				IconWidget->SetRenderTranslation(FVector2D(FMath::Clamp(XPos, -HalfWidth, HalfWidth), 0));
			}
		}
	}
}

void UGS_CompassWidget::RefreshTeammateIcons()
{
	if (!TeammateIconClass || !CompassCanvasPanel) return;

	APawn* PlayerPawn = GetOwningPlayerPawn();
	if (!PlayerPawn) return;

	TArray<UGS_CompassIndicatorComponent*> CurrentIndicators = UGS_CompassIndicatorComponent::GetAllCompassIndicators(this);

	// Remove old widgets
	for (int32 i = TeammateIcons.Num() - 1; i >= 0; --i)
	{
		if (!TeammateIcons[i].IndicatorComponent.IsValid() || !CurrentIndicators.Contains(TeammateIcons[i].IndicatorComponent.Get()))
		{
			if (TeammateIcons[i].IconWidget)
			{
				TeammateIcons[i].IconWidget->RemoveFromParent();
			}
			TeammateIcons.RemoveAt(i);
		}
	}

	// Add new widgets
	for (UGS_CompassIndicatorComponent* Indicator : CurrentIndicators)
	{
		if (!Indicator || Indicator->GetOwner() == PlayerPawn) continue;

		bool bAlreadyExists = TeammateIcons.ContainsByPredicate([Indicator](const FTeammateIconInfo& Info)
		{
			return Info.IndicatorComponent == Indicator;
		});

		if (!bAlreadyExists)
		{
			UGS_TeammateIconWidget* NewIconWidget = CreateWidget<UGS_TeammateIconWidget>(this, TeammateIconClass);
			if (NewIconWidget)
			{
				UCanvasPanelSlot* CanvasSlot = CompassCanvasPanel->AddChildToCanvas(NewIconWidget);
				if (CanvasSlot)
				{
					CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
					CanvasSlot->SetAutoSize(true);
				}

				FTeammateIconInfo NewInfo;
				NewInfo.IndicatorComponent = Indicator;
				NewInfo.IconWidget = NewIconWidget;
				TeammateIcons.Add(NewInfo);
			}
		}
	}
} 