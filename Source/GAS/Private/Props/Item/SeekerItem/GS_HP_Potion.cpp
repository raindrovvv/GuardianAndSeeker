// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Props/Item/SeekerItem/GS_HP_Potion.h"
#include "Props/Item/E_ItemType.h"

AGS_HP_Potion::AGS_HP_Potion()
{
	PrimaryActorTick.bCanEverTick = true;

	if (UStaticMeshComponent* MeshComp = GetVisualMesh())
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetSimulatePhysics(false);
	}
}

void AGS_HP_Potion::BeginPlay()
{
	Super::BeginPlay();
}

void AGS_HP_Potion::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGS_HP_Potion::ReleaseFromHolder()
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	ApplyDropPhysics();
	GetWorldTimerManager().SetTimer(DestructionTimerHandle, this, &AGS_Item::DestroyItem, DestructionDelay, false);
}

void AGS_HP_Potion::ApplyDropPhysics()
{
	UStaticMeshComponent* MeshComp = GetVisualMesh();
	if (!MeshComp)
	{
		return;
	}

	// Enable physics simulation
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetEnableGravity(true);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// Apply directional impulse
	const FVector ImpulseDirection =
		(-GetActorForwardVector() * DropImpulseStrength) + (-FVector::UpVector * (DropImpulseStrength * 0.48f));
	MeshComp->AddImpulse(ImpulseDirection, NAME_None, true);

	// Apply random angular torque for tumbling effect
	const FVector RandomTorque = FVector(
		FMath::FRandRange(-100.0f, 100.0f), FMath::FRandRange(-100.0f, 100.0f), FMath::FRandRange(-100.0f, 100.0f));
	MeshComp->AddAngularImpulseInDegrees(RandomTorque, NAME_None, true);
}
