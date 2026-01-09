#include "Props/Trap/GS_TrapManager.h"
#include "Props/Trap/GS_TrapBase.h"
#include "Props/Trap/TrapMotion/GS_TrapMotionCompBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"


AGS_TrapManager::AGS_TrapManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetLoadOnClient = true;
}


void AGS_TrapManager::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->RegisterTrapManager(this);
		}
	}
}

void AGS_TrapManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->UnregisterTrapManager(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AGS_TrapManager::TimerTrapManager()
{
	for (auto& WeakTrap : RegisteredTraps)
	{
		if (AGS_TrapBase* Trap = WeakTrap.Get())
		{
			if (!Trap->CanStartMotion())
			{
				continue;
			}
			if (UGS_TrapMotionCompBase* MotionComp = Trap->GetValidMotionComponent())
			{
				if (HasAuthority())
				{
					MotionComp->StartMotion();
				}
			}
		}
	}
}


void AGS_TrapManager::RegisterTrap(AGS_TrapBase* Trap)
{
	if (!RegisteredTraps.Contains(Trap))
	{
		RegisteredTraps.Add(Trap);
		//UE_LOG(LogTemp, Warning, TEXT("[TrapManager] Registered Trap: %s"), *Trap->GetName());
	}
	//else
	//{
	//	UE_LOG(LogTemp, Error, TEXT("[TrapManager] Tried to register null Trap!"));
	//}
}

void AGS_TrapManager::UnregisterTrap(AGS_TrapBase* Trap)
{
	RegisteredTraps.Remove(Trap);
}


void AGS_TrapManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// RegisteredTraps는 TWeakObjectPtr이므로 복제 불가 - 복제 제거됨
}