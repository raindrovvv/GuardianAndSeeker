#include "Props/GS_RoomBase.h"
#include "Rendering/GS_RenderingConstants.h"

AGS_RoomBase::AGS_RoomBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Floor = CreateDefaultSubobject<UStaticMeshComponent>("Floor");
	SetRootComponent(Floor);
	
	Wall = CreateDefaultSubobject<UStaticMeshComponent>("Wall");
	Wall->SetupAttachment(Floor);

	Ceiling = CreateDefaultSubobject<UStaticMeshComponent>("Ceiling");
	Ceiling->SetupAttachment(Floor);

	BGMTrigger = CreateDefaultSubobject<UChildActorComponent>("BGMTrigger");
	BGMTrigger->SetupAttachment(Floor);
	BGMTrigger->SetMobility(EComponentMobility::Movable);
}

void AGS_RoomBase::BeginPlay()
{
	Super::BeginPlay();

	// === Static Mesh Distance Culling 설정 (클라이언트만) ===
	if (!IsRunningDedicatedServer())
	{
		float CullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::ROOM_CULL_DISTANCE);
		int32 MinLOD = GS_Rendering::CalculateMinLOD(this);

		TArray<UMeshComponent*> MeshComponents;
		GetComponents<UMeshComponent>(MeshComponents);

		for (UMeshComponent* MeshComp : MeshComponents)
		{
			if (MeshComp)
			{
				MeshComp->SetCullDistance(CullDistance);
				MeshComp->SetCachedMaxDrawDistance(CullDistance);
				MeshComp->bAllowCullDistanceVolume = true;
				
				// 방 모듈은 크기가 크므로 팝인 방지를 위해 바운드 스케일 약간 조정
				MeshComp->SetBoundsScale(GS_Rendering::DEFAULT_BOUNDS_SCALE);
				
				if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(MeshComp))
				{
					StaticMesh->MinLOD = MinLOD;
				}
				else if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(MeshComp))
				{
					SkeletalMesh->MinLodModel = MinLOD;
				}
			}
		}
	}

	// === Occlusion Culling 설정 ===
	if (!IsRunningDedicatedServer())
	{
		if (Floor)
		{
			Floor->bUseAsOccluder = true;
			Floor->SetCastShadow(true);
		}

		if (Wall)
		{
			Wall->bUseAsOccluder = true; // 벽은 강력한 Occluder
			Wall->SetCastShadow(true);
		}

		if (Ceiling)
		{
			Ceiling->bUseAsOccluder = true;
			Ceiling->SetCastShadow(true);
		}
	}
}


void AGS_RoomBase::HideCeiling()
{
	if (Ceiling)
	{
		Ceiling->SetVisibility(false, true);
	}
}

void AGS_RoomBase::ShowCeiling()
{
	if (Ceiling)
	{
		Ceiling->SetVisibility(true, true);
	}
}

void AGS_RoomBase::UseDepthStencil()
{
	Floor->SetRenderCustomDepth(true);
	Wall->SetRenderCustomDepth(true);
}
