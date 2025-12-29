#include "Weapon/GS_Weapon.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Components/MeshComponent.h"

// Sets default values
AGS_Weapon::AGS_Weapon()
{
	// Set this actor to call Tick() every frame.
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
}

// Called when the game starts or when spawned
void AGS_Weapon::BeginPlay()
{
	Super::BeginPlay();

	// === 무기 메시 Distance Culling 설정 (클라이언트 전용) ===
	if (!IsRunningDedicatedServer())
	{
		TArray<UMeshComponent*> MeshComponents;
		GetComponents<UMeshComponent>(MeshComponents);

		int32 MinLOD = GS_Rendering::CalculateMinLOD(this);

		for (UMeshComponent* MeshComp : MeshComponents)
		{
			if (MeshComp)
			{
				float CullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::WEAPON_CULL_DISTANCE);

				MeshComp->SetCullDistance(CullDistance);
				MeshComp->SetCachedMaxDrawDistance(CullDistance);
				MeshComp->bAllowCullDistanceVolume = true;
				MeshComp->SetBoundsScale(GS_Rendering::DEFAULT_BOUNDS_SCALE);
				
				if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(MeshComp))
				{
					StaticMesh->MinLOD = MinLOD;
				}
				else if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(MeshComp))
				{
					SkeletalMesh->MinLodModel = MinLOD;
				}

				// UE_LOG(LogTemp, Log, TEXT("[Weapon:%s] Mesh Optimization - Cull Distance: %.1f"), *GetName(), CullDistance);
			}
		}
	}
}

void AGS_Weapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

// Called every frame
void AGS_Weapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ...
}
