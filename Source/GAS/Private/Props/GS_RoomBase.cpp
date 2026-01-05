#include "Props/GS_RoomBase.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"

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
		const float RoomCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::ROOM_CULL_DISTANCE);
		const float FoliageCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::FOLIAGE_CULL_DISTANCE);
		const int32 MinLOD = GS_Rendering::CalculateMinLOD(this);

		TArray<UMeshComponent*> MeshComponents;
		GetComponents<UMeshComponent>(MeshComponents);

		for (UMeshComponent* MeshComp : MeshComponents)
		{
			if (MeshComp)
			{
				// 폴리지 컴포넌트는 더 공격적인 컬링 적용 (Nanite 미지원)
				const FString CompName = MeshComp->GetName();
				const bool bIsFoliage = CompName.Contains(TEXT("Foliage")) || CompName.Contains(TEXT("Vine")) || CompName.Contains(TEXT("Grass"));
				const float CullDistance = bIsFoliage ? FoliageCullDistance : RoomCullDistance;

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

	// === Light 최적화 ===
	// 보스룸은 스마트 최적화: 중요도별 차등 품질 설정
	if (!IsRunningDedicatedServer())
	{
		// 보스룸 체크 (액터 이름에 "Boss" 포함 또는 "BossRoom" 태그)
		const FString ActorName = GetName();
		const bool bIsBossRoom = ActorName.Contains(TEXT("Boss")) ||
		                         ActorHasTag(FName("BossRoom"));

		TArray<ULightComponent*> LightComponents;
		GetComponents<ULightComponent>(LightComponents);

		const float LightCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::FOLIAGE_CULL_DISTANCE);

		for (ULightComponent* LightComp : LightComponents)
		{
			if (LightComp)
			{
				// 컬링 거리 설정 (모든 방에 적용 - 성능 최적화)
				LightComp->MaxDrawDistance = LightCullDistance;
				LightComp->MaxDistanceFadeRange = 500.0f;

				// 보스룸이 아닌 경우: 모든 라이트 최적화
				if (!bIsBossRoom)
				{
					// 그림자 비활성화 (성능 최적화 핵심)
					LightComp->SetCastShadows(false);

					// 1. 스페큘러 비활성화 (반사 계산 제거)
					LightComp->SetAffectReflection(false);

					// 2. 글로벌 일루미네이션 영향 끄기
					LightComp->SetAffectGlobalIllumination(false);

					// 3. Point/Spot Light 전용 설정
					if (UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComp))
					{
						// 역제곱 감쇠 끄면 계산 단순화
						PointLight->SetIntensityUnits(ELightUnits::Unitless);
						PointLight->bUseInverseSquaredFalloff = false;
					}
				}
				// 보스룸인 경우: 중요도별 스마트 최적화
				else
				{
					// 라이트 중요도 판단 (밝기 기준)
					const float Intensity = LightComp->Intensity;
					const bool bHasMainTag = LightComp->ComponentHasTag(FName("MainLight"));

					// 🟢 High Priority: 주요 라이트 (밝기 > 20 또는 "MainLight" 태그)
					if (bHasMainTag || Intensity > 20.0f)
					{
						// 모든 기능 유지 (그림자, 반사, GI)
						// 섀도우 해상도만 약간 낮춤 (품질 유지하면서 성능 개선)
						LightComp->ShadowResolutionScale = 0.5f;
					}
					// 🟡 Medium Priority: 보조 라이트
					else if (Intensity >= 10.0f)
					{
						// 그림자만 비활성화, 반사/GI는 유지
						LightComp->SetCastShadows(false);
						// 반사와 GI는 분위기에 중요하므로 유지
					}
					// 🔴 Low Priority: 배경 라이트 (밝기 < 10)
					else
					{
						// 그림자/반사 비활성화, 라이팅만 유지
						LightComp->SetCastShadows(false);
						LightComp->SetAffectReflection(false);
						// GI는 유지 (분위기에 기여)

						// Point Light는 역제곱 감쇠 단순화
						if (UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComp))
						{
							PointLight->bUseInverseSquaredFalloff = false;
						}
					}
				}
			}
		}

		// === Niagara (VFX) 최적화 ===
		TArray<UNiagaraComponent*> NiagaraComponents;
		GetComponents<UNiagaraComponent>(NiagaraComponents);

		const float VFXCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::VFX_DISABLE_DISTANCE);

		for (UNiagaraComponent* NiagaraComp : NiagaraComponents)
		{
			if (NiagaraComp)
			{
				// 거리 기반 자동 비활성화 설정
				NiagaraComp->SetCullDistance(VFXCullDistance);
				NiagaraComp->SetCachedMaxDrawDistance(VFXCullDistance);

				// 거리 기반 컬링 활성화
				NiagaraComp->bAllowCullDistanceVolume = true;
			}
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
