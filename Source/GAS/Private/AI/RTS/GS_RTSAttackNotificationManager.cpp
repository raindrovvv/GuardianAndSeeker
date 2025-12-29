// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/RTS/GS_RTSAttackNotificationManager.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "UI/RTS/GS_RTSAttackWarningWidget.h"
#include "UI/RTS/GS_MinimapWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UGS_RTSAttackNotificationManager::UGS_RTSAttackNotificationManager()
{
	PrimaryComponentTick.bCanEverTick = false;

	LastAttackLocation = FVector::ZeroVector;
	LastNotificationTime = 0.0f;
	LastSoundPlayTime = 0.0f;
	NotificationCooldown = 0.5f;
	SoundCooldown = 1.0f;
}

void UGS_RTSAttackNotificationManager::BeginPlay()
{
	Super::BeginPlay();
}

void UGS_RTSAttackNotificationManager::OnUnitAttacked(AGS_Monster* Unit, FVector Location)
{
	// Validate unit
	if (!Unit || Unit->IsDead())
	{
		return;
	}

	// Check cooldown to prevent spam
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastNotificationTime < NotificationCooldown)
	{
		return;
	}

	// Update tracking
	LastNotificationTime = CurrentTime;
	LastAttackLocation = Location;

	// Show HUD notification
	if (WarningWidget)
	{
		WarningWidget->ShowWarning();
	}

	// Show minimap warning icon
	if (MinimapWidget)
	{
		MinimapWidget->ShowAttackWarning(Location);
	}

	// Play sound alert (with separate cooldown) - Local 2D sound for RTS player
	if (AttackAlertSound && (CurrentTime - LastSoundPlayTime) >= SoundCooldown)
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayStatics::PlaySound2D(World, AttackAlertSound);
			LastSoundPlayTime = CurrentTime;
		}
	}
}

void UGS_RTSAttackNotificationManager::SetWarningWidget(UGS_RTSAttackWarningWidget* InWarningWidget)
{
	WarningWidget = InWarningWidget;
}

void UGS_RTSAttackNotificationManager::SetMinimapWidget(UGS_MinimapWidget* InMinimapWidget)
{
	MinimapWidget = InMinimapWidget;
}
