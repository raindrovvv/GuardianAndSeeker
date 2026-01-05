// Fill out your copyright notice in the Description page of Project Settings.

#include "System/Subsystem/GS_ActorRegistrySubsystem.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Guardian/GS_Guardian.h"
#include "AI/RTS/GS_RTSController.h"
#include "Props/Trap/NonTriggerTrap/GS_LavaTrap.h"
#include "Props/Trap/GS_TrapManager.h"
#include "UI/Character/GS_CompassIndicatorComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UGS_ActorRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 30초마다 한 번씩 유효하지 않은 엔트리 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CleanupTimerHandle, this, &UGS_ActorRegistrySubsystem::CleanupInvalidEntries, 30.0f, true);
	}
}

void UGS_ActorRegistrySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CleanupTimerHandle);
	}

	Super::Deinitialize();
}

void UGS_ActorRegistrySubsystem::CleanupInvalidEntries()
{
	// RemoveAllSwap은 배열의 순서를 유지하지 않지만,
	// 등록된 캐릭터 리스트에서는 순서가 중요하지 않으므로 일반적인 RemoveAll보다 성능상 유리.
	RegisteredMonsters.RemoveAllSwap([](const TWeakObjectPtr<AGS_Monster>& Ptr)
	                                 { return !Ptr.IsValid(); });

	RegisteredSeekers.RemoveAllSwap([](const TWeakObjectPtr<AGS_Seeker>& Ptr)
	                                { return !Ptr.IsValid(); });

	if (RegisteredGuardian.IsStale())
	{
		RegisteredGuardian = nullptr;
	}

	if (RegisteredRTSController.IsStale())
	{
		RegisteredRTSController = nullptr;
	}

	RegisteredLavaTraps.RemoveAllSwap([](const TWeakObjectPtr<AGS_LavaTrap>& Ptr)
	                                  { return !Ptr.IsValid(); });

	RegisteredCompassIndicators.RemoveAllSwap([](const TWeakObjectPtr<UGS_CompassIndicatorComponent>& Ptr)
	                                          { return !Ptr.IsValid(); });

	if (RegisteredTrapManager.IsStale())
	{
		RegisteredTrapManager = nullptr;
	}
}


void UGS_ActorRegistrySubsystem::RegisterMonster(AGS_Monster* Monster)
{
	if (IsValid(Monster))
	{
		RegisteredMonsters.AddUnique(Monster);
	}
}

void UGS_ActorRegistrySubsystem::UnregisterMonster(AGS_Monster* Monster)
{
	if (Monster)
	{
		RegisteredMonsters.Remove(Monster);
	}
}

void UGS_ActorRegistrySubsystem::RegisterSeeker(AGS_Seeker* Seeker)
{
	if (IsValid(Seeker))
	{
		RegisteredSeekers.AddUnique(Seeker);
	}
}

void UGS_ActorRegistrySubsystem::UnregisterSeeker(AGS_Seeker* Seeker)
{
	if (Seeker)
	{
		RegisteredSeekers.Remove(Seeker);
	}
}

void UGS_ActorRegistrySubsystem::RegisterGuardian(AGS_Guardian* Guardian)
{
	if (IsValid(Guardian))
	{
		RegisteredGuardian = Guardian;
	}
}

void UGS_ActorRegistrySubsystem::UnregisterGuardian(AGS_Guardian* Guardian)
{
	if (RegisteredGuardian == Guardian)
	{
		RegisteredGuardian = nullptr;
	}
}

void UGS_ActorRegistrySubsystem::RegisterRTSController(AGS_RTSController* RTSController)
{
	if (IsValid(RTSController))
	{
		RegisteredRTSController = RTSController;
	}
}

void UGS_ActorRegistrySubsystem::UnregisterRTSController(AGS_RTSController* RTSController)
{
	if (RegisteredRTSController == RTSController)
	{
		RegisteredRTSController = nullptr;
	}
}

void UGS_ActorRegistrySubsystem::RegisterLavaTrap(AGS_LavaTrap* LavaTrap)
{
	if (IsValid(LavaTrap))
	{
		RegisteredLavaTraps.AddUnique(LavaTrap);
	}
}

void UGS_ActorRegistrySubsystem::UnregisterLavaTrap(AGS_LavaTrap* LavaTrap)
{
	if (LavaTrap)
	{
		RegisteredLavaTraps.Remove(LavaTrap);
	}
}

void UGS_ActorRegistrySubsystem::RegisterCompassIndicator(UGS_CompassIndicatorComponent* CompassIndicator)
{
	if (IsValid(CompassIndicator))
	{
		RegisteredCompassIndicators.AddUnique(CompassIndicator);
	}
}

void UGS_ActorRegistrySubsystem::UnregisterCompassIndicator(UGS_CompassIndicatorComponent* CompassIndicator)
{
	if (CompassIndicator)
	{
		RegisteredCompassIndicators.Remove(CompassIndicator);
	}
}

void UGS_ActorRegistrySubsystem::RegisterTrapManager(AGS_TrapManager* TrapManager)
{
	if (IsValid(TrapManager))
	{
		RegisteredTrapManager = TrapManager;
	}
}

void UGS_ActorRegistrySubsystem::UnregisterTrapManager(AGS_TrapManager* TrapManager)
{
	if (RegisteredTrapManager == TrapManager)
	{
		RegisteredTrapManager = nullptr;
	}
}

void UGS_ActorRegistrySubsystem::GetAllHostileActors(TArray<AActor*>& OutActors) const
{
	OutActors.Reset();

	for (const auto& MonsterPtr : RegisteredMonsters)
	{
		if (MonsterPtr.IsValid())
		{
			OutActors.Add(MonsterPtr.Get());
		}
	}

	if (RegisteredGuardian.IsValid())
	{
		OutActors.Add(RegisteredGuardian.Get());
	}
}
