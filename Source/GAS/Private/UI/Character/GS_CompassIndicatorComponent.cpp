// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Character/GS_CompassIndicatorComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "System/GS_PlayerState.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "UObject/UObjectGlobals.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"

UGS_CompassIndicatorComponent::UGS_CompassIndicatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bShowOnCompass = true;
	CustomIcon = nullptr;
	IconColor = FLinearColor::White;
	MaxDisplayDistance = 10000.0f;
	bCheckPlayerStatus = true;
	bIsManuallyHidden = false;

	bWantsInitializeComponent = true;
}

void UGS_CompassIndicatorComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->RegisterCompassIndicator(this);
		}
	}
}

void UGS_CompassIndicatorComponent::UninitializeComponent()
{
	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->UnregisterCompassIndicator(this);
		}
	}

	Super::UninitializeComponent();
}

FVector UGS_CompassIndicatorComponent::GetWorldLocation() const
{
	if (AActor* Owner = GetOwner())
	{
		return Owner->GetActorLocation();
	}
	return FVector::ZeroVector;
}

bool UGS_CompassIndicatorComponent::IsValidForCompass() const
{
	if (!bShowOnCompass || bIsManuallyHidden)
	{
		return false;
	}
	
	return bCachedIsValid;
}

ESeekerJob UGS_CompassIndicatorComponent::GetSeekerJob() const
{
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AGS_PlayerState* PS = OwnerPawn->GetPlayerState<AGS_PlayerState>())
		{
			return PS->CurrentSeekerJob;
		}
	}
	return ESeekerJob::Ares; // Default fallback
}

FString UGS_CompassIndicatorComponent::GetPlayerName() const
{
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerState* PS = OwnerPawn->GetPlayerState())
		{
			return PS->GetPlayerName();
		}
	}
	return TEXT("Unknown");
}

bool UGS_CompassIndicatorComponent::IsPlayerAlive() const
{
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const AGS_PlayerState* PS = OwnerPawn->GetPlayerState<AGS_PlayerState>())
		{
			return PS->bIsAlive;
		}
		// Fallback for when PlayerState is not yet available
		return true;
	}
	// Not a pawn, so the concept of being "alive" in a player sense doesn't apply.
	return true;
}

TArray<UGS_CompassIndicatorComponent*> UGS_CompassIndicatorComponent::GetAllCompassIndicators(const UObject* WorldContext)
{
	TArray<UGS_CompassIndicatorComponent*> FoundComponents;
	
	if (!WorldContext)
	{
		return FoundComponents;
	}

	UWorld* World = WorldContext->GetWorld();
	if (!World)
	{
		return FoundComponents;
	}

	// Iterate through all registered compass indicators
	if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
	{
		const TArray<TWeakObjectPtr<UGS_CompassIndicatorComponent>>& RegisteredIndicators = Registry->GetCompassIndicators();
		for (const TWeakObjectPtr<UGS_CompassIndicatorComponent>& IndicatorPtr : RegisteredIndicators)
		{
			UGS_CompassIndicatorComponent* CompassComponent = IndicatorPtr.Get();
			if (IsValid(CompassComponent) && CompassComponent->IsValidForCompass())
			{
				FoundComponents.Add(CompassComponent);
			}
		}
	}
    
	return FoundComponents;
}

void UGS_CompassIndicatorComponent::BeginPlay()
{
	Super::BeginPlay();

	// Start caching timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CacheTimerHandle, this, &UGS_CompassIndicatorComponent::UpdateCachedValidity, 0.5f, true);
	}
	// Initial update
	UpdateCachedValidity();
}

void UGS_CompassIndicatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CacheTimerHandle);
	}
	
	Super::EndPlay(EndPlayReason);
}

void UGS_CompassIndicatorComponent::UpdateCachedValidity()
{
	if (bCheckPlayerStatus)
	{
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			if (const AGS_PlayerState* PS = OwnerPawn->GetPlayerState<AGS_PlayerState>())
			{
				bCachedIsValid = PS->bIsAlive;
				return;
			}
			bCachedIsValid = true; // Default to true if PS not found (yet)
			return;
		}
	}
	bCachedIsValid = true;
} 