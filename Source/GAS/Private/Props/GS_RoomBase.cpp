#include "Props/GS_RoomBase.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"
#include "SignificanceManager.h"
#include "Engine/World.h"
#include "AkAudioDevice.h"
#include "Misc/App.h"

// FName 상수 캐싱 (매 프레임 생성 방지)
namespace RoomTags
{
static const FName MainLight(TEXT("MainLight"));
static const FName IgnoreCulling(TEXT("IgnoreCulling"));
} // namespace RoomTags

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

	// 데디케이티드 서버에서는 렌더링 최적화 불필요
	if (IsRunningDedicatedServer())
	{
		return;
	}

	// ========================================
	// 1. 상수값 캐싱 (한 번만 계산)
	// ========================================
	const float RoomCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::ROOM_CULL_DISTANCE);
	const float WallCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::WALL_CULL_DISTANCE);
	const float FoliageCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::FOLIAGE_CULL_DISTANCE);
	const int32 MinLOD = GS_Rendering::CalculateMinLOD(this);

	// 컬링 거리 제곱값 캐싱
	CachedShadowDisableDistanceSq = FMath::Square(GS_Rendering::DYNAMIC_SHADOW_DISABLE_DISTANCE);

	// 보스룸 여부 판단 (한 번만)
	const FString ActorName = GetName();
	bIsBossRoom = ActorName.Contains(TEXT("Boss")) || ActorHasTag(FName("BossRoom"));

	// ========================================
	// 2. Static Mesh Distance Culling 설정 (엔진에 위임)
	// ========================================
	TArray<UMeshComponent*> MeshComponents;
	GetComponents<UMeshComponent>(MeshComponents);

	for (UMeshComponent* MeshComp : MeshComponents)
	{
		if (!MeshComp)
		{
			continue;
		}

		// 폴리지 컴포넌트는 더 공격적인 컬링 적용 (Nanite 미지원)
		const FString CompName = MeshComp->GetName();
		const bool bIsFoliage = CompName.Contains(TEXT("Foliage")) ||
		                        CompName.Contains(TEXT("Vine")) ||
		                        CompName.Contains(TEXT("Grass"));
		const bool bIsWall = (MeshComp == Wall);
		const float CullDistance = bIsFoliage ? FoliageCullDistance : (bIsWall ? WallCullDistance : RoomCullDistance);

		// [핵심] 엔진의 Distance Culling에 위임 - SetVisibility 불필요
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
	}

	// ========================================
	// 3. Occlusion Culling 설정
	// ========================================
	if (Floor)
	{
		Floor->bUseAsOccluder = true;
		Floor->SetCastShadow(true);
	}

	if (Wall)
	{
		Wall->bUseAsOccluder = true;
		Wall->SetCastShadow(true);
	}

	if (Ceiling)
	{
		Ceiling->bUseAsOccluder = true;
		Ceiling->SetCastShadow(true);
	}

	// ========================================
	// 4. Light 최적화 (보스룸 스마트 최적화)
	// ========================================
	TArray<ULightComponent*> LightComponents;
	GetComponents<ULightComponent>(LightComponents);

	const float LightCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::LIGHT_CULL_DISTANCE);

	for (ULightComponent* LightComp : LightComponents)
	{
		if (!LightComp)
		{
			continue;
		}

		// [핵심] 엔진의 MaxDrawDistance로 컬링 위임
		LightComp->MaxDrawDistance = LightCullDistance;
		LightComp->MaxDistanceFadeRange = 500.0f;

		// 보스룸이 아닌 경우: 모든 라이트 최적화
		if (!bIsBossRoom)
		{
			LightComp->SetCastShadows(false);
			LightComp->SetAffectReflection(false);
			LightComp->SetAffectGlobalIllumination(false);
		}
		// 보스룸인 경우: 중요도별 스마트 최적화
		else
		{
			const float Intensity = LightComp->Intensity;
			const bool bHasMainTag = LightComp->ComponentHasTag(RoomTags::MainLight);

			// High Priority: 주요 라이트 (밝기 > 20 또는 "MainLight" 태그)
			if (bHasMainTag || Intensity > 20.0f)
			{
				LightComp->ShadowResolutionScale = 0.5f;
			}
			// Medium Priority: 보조 라이트
			else if (Intensity >= 10.0f)
			{
				LightComp->SetCastShadows(false);
			}
			// Low Priority: 배경 라이트
			else
			{
				LightComp->SetCastShadows(false);
				LightComp->SetAffectReflection(false);
			}
		}
	}

	// ========================================
	// 5. Niagara (VFX) 최적화 (엔진에 위임)
	// ========================================
	TArray<UNiagaraComponent*> NiagaraComponents;
	GetComponents<UNiagaraComponent>(NiagaraComponents);

	const float VFXCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::VFX_DISABLE_DISTANCE);

	for (UNiagaraComponent* NiagaraComp : NiagaraComponents)
	{
		if (NiagaraComp)
		{
			// [핵심] 엔진의 Distance Culling에 위임
			NiagaraComp->SetCullDistance(VFXCullDistance);
			NiagaraComp->SetCachedMaxDrawDistance(VFXCullDistance);
			NiagaraComp->bAllowCullDistanceVolume = true;
		}
	}

	// ========================================
	// 6. 가시성 및 그림자 최적화 시스템 설정
	// ========================================
	RegisterSignificanceManager();
	CacheOptimizedComponents();

	// 타이머 시작 (로드 밸런싱을 위한 랜덤 오프셋)
	float RandomVariance = FMath::RandRange(0.0f, 0.5f);
	GetWorld()->GetTimerManager().SetTimer(
	    ShadowCullingTimerHandle, this, &AGS_RoomBase::UpdateVisibilityAndShadowCulling,
	    0.5f, true, RandomVariance);

	// 초기 1회 즉시 실행
	UpdateVisibilityAndShadowCulling();
}

void AGS_RoomBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Significance Manager 해제
	if (!IsRunningDedicatedServer() && GetWorld())
	{
		if (USignificanceManager* SM = USignificanceManager::Get(GetWorld()))
		{
			SM->UnregisterObject(this);
		}

		GetWorld()->GetTimerManager().ClearTimer(ShadowCullingTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AGS_RoomBase::CacheOptimizedComponents()
{
	CachedPrimitiveComponents.Empty();
	CachedLightComponents.Empty();
	CachedNiagaraComponents.Empty();

	TArray<UActorComponent*> AllComponents;
	GetComponents(AllComponents, true);

	for (UActorComponent* Comp : AllComponents)
	{
		if (!IsValid(Comp))
		{
			continue;
		}

		if (Comp->ComponentTags.Contains(RoomTags::IgnoreCulling))
		{
			continue;
		}

		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
		{
			CachedPrimitiveComponents.Add(PrimComp);
		}
		else if (ULightComponent* LightComp = Cast<ULightComponent>(Comp))
		{
			CachedLightComponents.Add(LightComp);
		}
		else if (UNiagaraComponent* NiagaraComp = Cast<UNiagaraComponent>(Comp))
		{
			CachedNiagaraComponents.Add(NiagaraComp);
		}
	}
}

void AGS_RoomBase::UpdateVisibilityAndShadowCulling()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
	const float DistSq = FVector::DistSquared(GetActorLocation(), CameraLoc);

	// 컬링 거리 계산 (RTS 배율 적용)
	const float RoomCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::ROOM_CULL_DISTANCE);
	const float WallCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::WALL_CULL_DISTANCE);

	// 거리 제곱 임계값 캐싱 (1.21 = 1.1^2, 10% 여유치)
	const float RoomCullDistSqThreshold = (RoomCullDistance * RoomCullDistance) * 1.21f;
	const float WallCullDistSqThreshold = (WallCullDistance * WallCullDistance) * 1.21f;

	// 방 전체 가시성 판단 (Floor 기준)
	const bool bShouldBeVisible = DistSq < RoomCullDistSqThreshold;

	const bool bIsRTSMode = GS_Rendering::IsRTSMode(this);

	// === Primitive 컴포넌트 처리 ===
	for (TObjectPtr<UPrimitiveComponent> PrimComp : CachedPrimitiveComponents)
	{
		if (!PrimComp || !IsValid(PrimComp))
		{
			continue;
		}

		// [핵심] 개별 컴포넌트 위치 기준으로 거리 계산 (대형 방 모듈 대응)
		const float CompDistSq = FVector::DistSquared(PrimComp->GetComponentLocation(), CameraLoc);
		const bool bIsWall = (PrimComp == Wall);
		const float currentThreshold = bIsWall ? WallCullDistSqThreshold : RoomCullDistSqThreshold;
		bool bCompShouldBeVisible = CompDistSq < currentThreshold;

		// 태그 기반 가시성 판단
		if (bCompShouldBeVisible)
		{
			if (PrimComp->ComponentTags.Contains(FName("Hidden")) ||
			    (bIsRTSMode && PrimComp->ComponentTags.Contains(FName("RTS"))))
			{
				bCompShouldBeVisible = false;
			}
		}

		// 1. 가시성 업데이트 (나나이트 메시도 강제 숨김)
		if (PrimComp->GetVisibleFlag() != bCompShouldBeVisible)
		{
			PrimComp->SetVisibility(bCompShouldBeVisible, true);
		}

		// 2. 그림자 업데이트 (보일 때만)
		if (bCompShouldBeVisible)
		{
			GS_Rendering::UpdateShadowCulling(this, PrimComp);
		}
	}

	// === Light 컴포넌트 처리 ===
	for (TObjectPtr<ULightComponent> LightComp : CachedLightComponents)
	{
		if (!LightComp || !IsValid(LightComp))
		{
			continue;
		}

		// 태그 기반 가시성 판단
		bool bLightShouldBeVisible = bShouldBeVisible;
		if (bLightShouldBeVisible)
		{
			if (LightComp->ComponentTags.Contains(FName("Hidden")) ||
			    (bIsRTSMode && LightComp->ComponentTags.Contains(FName("RTS"))))
			{
				bLightShouldBeVisible = false;
			}
		}

		// 1. 가시성 업데이트
		if (LightComp->GetVisibleFlag() != bLightShouldBeVisible)
		{
			LightComp->SetVisibility(bLightShouldBeVisible);
		}

		// 2. 그림자 처리 (보일 때만, MainLight 제외)
		if (bLightShouldBeVisible && !LightComp->ComponentHasTag(RoomTags::MainLight))
		{
			// 보스룸이 아닌 경우만 그림자 토글
			if (!bIsBossRoom)
			{
				const bool bNearby = DistSq < CachedShadowDisableDistanceSq;
				if (LightComp->CastShadows != bNearby)
				{
					LightComp->SetCastShadows(bNearby);
				}
			}
		}
	}

	// === Niagara 컴포넌트 처리 ===
	for (TObjectPtr<UNiagaraComponent> NiagaraComp : CachedNiagaraComponents)
	{
		if (!NiagaraComp || !IsValid(NiagaraComp))
		{
			continue;
		}

		// 태그 기반 가시성 판단
		bool bVfxShouldBeVisible = bShouldBeVisible;
		if (bVfxShouldBeVisible)
		{
			if (NiagaraComp->ComponentTags.Contains(FName("Hidden")) ||
			    (bIsRTSMode && NiagaraComp->ComponentTags.Contains(FName("RTS"))))
			{
				bVfxShouldBeVisible = false;
			}
		}

		if (NiagaraComp->GetVisibleFlag() != bVfxShouldBeVisible)
		{
			NiagaraComp->SetVisibility(bVfxShouldBeVisible);
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

void AGS_RoomBase::RegisterSignificanceManager()
{
	if (USignificanceManager* SM = USignificanceManager::Get(GetWorld()))
	{
		TWeakObjectPtr<AGS_RoomBase> WeakThis(this);
		SM->RegisterObject(
		    this, "Room",
		    [WeakThis](USignificanceManager::FManagedObjectInfo* ObjectInfo, const FTransform& Viewpoint) -> float
		    {
			    if (AGS_RoomBase* StrongThis = WeakThis.Get())
			    {
				    return StrongThis->CalculateSignificance(Viewpoint);
			    }
			    return 0.0f;
		    },
		    USignificanceManager::EPostSignificanceType::Sequential,
		    [WeakThis](USignificanceManager::FManagedObjectInfo* ObjectInfo, float OldValue, float NewValue, bool bExternal)
		    {
			    if (AGS_RoomBase* StrongThis = WeakThis.Get())
			    {
				    StrongThis->OnSignificanceChanged(NewValue);
			    }
		    });
	}
}

float AGS_RoomBase::CalculateSignificance(const FTransform& Viewpoint)
{
	const float DistSq = FVector::DistSquared(GetActorLocation(), Viewpoint.GetLocation());
	const float MaxDistSq = FMath::Square(GS_Rendering::ROOM_CULL_DISTANCE * 1.5f);
	return FMath::Clamp(1.0f - (DistSq / MaxDistSq), 0.1f, 1.0f);
}

void AGS_RoomBase::OnSignificanceChanged(float NewSignificance)
{
	CurrentSignificance = NewSignificance;

	// 중요도에 따라 타이머 주기 동적 변경 (0.1s ~ 1.0s)
	const float NewInterval = GS_Rendering::GetAdaptiveTimerInterval(NewSignificance);

	if (GetWorld())
	{
		const float RemainingTime = GetWorld()->GetTimerManager().GetTimerRemaining(ShadowCullingTimerHandle);
		GetWorld()->GetTimerManager().SetTimer(
		    ShadowCullingTimerHandle, this, &AGS_RoomBase::UpdateVisibilityAndShadowCulling,
		    NewInterval, true, FMath::Max(0.01f, RemainingTime));
	}
}
