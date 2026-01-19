// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Weapon/Equipable/GS_WeaponBow.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ChildActorComponent.h"

AGS_WeaponBow::AGS_WeaponBow()
{
	PrimaryActorTick.bCanEverTick = false;

	// Construct the bow's skeletal mesh
	BowMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BowMesh"));
	SetRootComponent(BowMesh);

	// Setup the arrow child actor attachment
	ArrowActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("ArrowActorComponent"));
	ArrowActorComponent->SetupAttachment(GetRootComponent());
}

void AGS_WeaponBow::BeginPlay()
{
	Super::BeginPlay();
}
