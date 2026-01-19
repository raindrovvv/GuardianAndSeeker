// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Props/Item/GS_Item.h"
#include "Props/Item/GS_ItemData.h"

AGS_Item::AGS_Item()
{
	PrimaryActorTick.bCanEverTick = false;

	VisualMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMeshComponent"));
	RootComponent = VisualMeshComponent;
}

void AGS_Item::BeginPlay()
{
	Super::BeginPlay();
}

void AGS_Item::AssignItemData(UGS_ItemData* InItemData)
{
	ItemConfiguration = InItemData;
}

void AGS_Item::ApplyMeshVariant(FName VariantName)
{
	if (!ItemConfiguration)
	{
		return;
	}

	if (UStaticMesh* const* FoundMesh = ItemConfiguration->MeshVariants.Find(VariantName))
	{
		if (*FoundMesh)
		{
			VisualMeshComponent->SetStaticMesh(*FoundMesh);
		}
	}
}

UStaticMeshComponent* AGS_Item::GetVisualMesh() const
{
	return VisualMeshComponent;
}

void AGS_Item::DestroyItem()
{
	Destroy();
}
