#include "Component/GS_VisualPoolComp.h"
#include "Weapon/Projectile/Seeker/GS_ArrowVisualActor.h"
#include "Engine/World.h"

UGS_VisualPoolComp::UGS_VisualPoolComp()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGS_VisualPoolComp::Initialize(TSubclassOf<AGS_ArrowVisualActor> InActorClass, int32 PoolSize)
{
	ActorClass = InActorClass;
	if (!ActorClass)
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	for (int32 i = 0; i < PoolSize; ++i)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AGS_ArrowVisualActor* NewActor = World->SpawnActor<AGS_ArrowVisualActor>(ActorClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (NewActor)
		{
			NewActor->OwningPool = this;
			NewActor->Deactivate();
			ActorPool.Add(NewActor);
		}
	}
}

AGS_ArrowVisualActor* UGS_VisualPoolComp::GetActorFromPool(const FVector& Location, const FRotator& Rotation)
{
	AGS_ArrowVisualActor* ChosenActor = nullptr;

	for (AGS_ArrowVisualActor* Actor : ActorPool)
	{
		if (IsValid(Actor) && Actor->IsReady())
		{
			ChosenActor = Actor;
			break;
		}
	}

	// 풀이 모자라면 새로 하나 생성 (동적 확장)
	if (!ChosenActor && ActorClass)
	{
		UWorld* World = GetWorld();
		if (!World)
			return nullptr;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ChosenActor = World->SpawnActor<AGS_ArrowVisualActor>(ActorClass, Location, Rotation, Params);
		if (ChosenActor)
		{
			ChosenActor->OwningPool = this;
			ActorPool.Add(ChosenActor);
		}
	}

	if (ChosenActor)
	{
		ChosenActor->Activate(Location, Rotation);
	}

	return ChosenActor;
}

void UGS_VisualPoolComp::ReturnToPool(AGS_ArrowVisualActor* Actor)
{
	if (Actor)
	{
		Actor->Deactivate();
	}
}

void UGS_VisualPoolComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (AGS_ArrowVisualActor* Actor : ActorPool)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	ActorPool.Empty();

	Super::EndPlay(EndPlayReason);
}
