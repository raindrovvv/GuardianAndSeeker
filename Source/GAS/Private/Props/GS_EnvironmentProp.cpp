#include "Props/GS_EnvironmentProp.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Components/PrimitiveComponent.h"
#include "Components/LightComponent.h"
#include "NiagaraComponent.h"
#include "GeometryCacheComponent.h"
#include "SignificanceManager.h"
#include "Character/GS_Character.h"
#include "Engine/World.h"
#include "AkAudioDevice.h"
#include "AkGameplayStatics.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"

AGS_EnvironmentProp::AGS_EnvironmentProp()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AGS_EnvironmentProp::BeginPlay()
{
	Super::BeginPlay();

	if (!IsRunningDedicatedServer())
	{
		ApplyDistanceCulling();
		RegisterSignificanceManager();

		// 초기 가시성 업데이트 간격 설정 (중요도 시스템에 의해 관리됨)
		float RandomVariance = FMath::RandRange(0.0f, 0.5f);
		GetWorld()->GetTimerManager().SetTimer(ShadowCullingTimerHandle, this, &AGS_EnvironmentProp::UpdateCulling, 0.5f, true, RandomVariance);

		// 초기 1회 즉시 실행
		UpdateCulling();
	}
}

void AGS_EnvironmentProp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Significance Manager 해제
	if (!IsRunningDedicatedServer() && GetWorld())
	{
		if (USignificanceManager* SM = USignificanceManager::Get(GetWorld()))
		{
			SM->UnregisterObject(this);
		}
	}

	GetWorld()->GetTimerManager().ClearTimer(ShadowCullingTimerHandle);

	Super::EndPlay(EndPlayReason);
}

void AGS_EnvironmentProp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!IsRunningDedicatedServer())
	{
		ApplyDistanceCulling();

#if WITH_EDITOR
		// === 맵 체크 경고 해결을 위한 자동 보정 로직 ===
		TArray<UStaticMeshComponent*> MeshComps;
		GetComponents<UStaticMeshComponent>(MeshComps);
		for (UStaticMeshComponent* Mesh : MeshComps)
		{
			if (Mesh)
			{
				// 1. BoundsScale이 1보다 크면 퍼포먼스 경고가 발생하므로 1.0으로 강제 수정
				if (Mesh->BoundsScale > 1.0f)
				{
					Mesh->SetBoundsScale(1.0f);
				}

				// 2. Static Mesh가 할당되지 않은 경우 에디터 로그로 알림
				if (Mesh->GetStaticMesh() == nullptr)
				{
					UE_LOG(LogTemp, Warning, TEXT("[MapCheck Fix] %s의 메쉬 컴포넌트(%s)에 StaticMesh가 할당되지 않았습니다!"),
					       *GetName(), *Mesh->GetName());
				}
			}
		}
#endif
	}
}

void AGS_EnvironmentProp::ApplyDistanceCulling()
{
	if (!FApp::CanEverRender())
		return;

	const float FinalCullDistance = GS_Rendering::CalculateCullDistance(this, BaseCullDistance);
	const int32 MinLOD = GS_Rendering::CalculateMinLOD(this);

	TArray<UActorComponent*> AllComponents;
	GetComponents(AllComponents, true);

	OptimizedComponents.Empty();

	for (UActorComponent* Comp : AllComponents)
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp);
		if (!PrimComp || !IsValid(PrimComp))
			continue;

		if (PrimComp->ComponentTags.Contains("IgnoreCulling"))
			continue;

		// 캐싱
		OptimizedComponents.Add(PrimComp);

		PrimComp->bNeverDistanceCull = false;
		PrimComp->SetCullDistance(FinalCullDistance);
		PrimComp->SetCachedMaxDrawDistance(FinalCullDistance);
		PrimComp->bAllowCullDistanceVolume = true;

		if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(PrimComp))
		{
			MeshComp->MinLOD = MinLOD;
			// SetCullDistance 내부에서 MarkRenderStateDirty가 호출됨
		}
	}
}

void AGS_EnvironmentProp::UpdateCulling()
{
	if (IsRunningDedicatedServer())
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
		return;

	FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
	float DistSq = FVector::DistSquared(GetActorLocation(), CameraLoc);

	const float FinalCullDistance = GS_Rendering::CalculateCullDistance(this, BaseCullDistance);

	// 거리 제곱 임계값 캐싱 (1.21 = 1.1^2, 10% 여유치)
	const float CullDistSqThreshold = (FinalCullDistance * FinalCullDistance) * 1.21f;

	// 수동 가시성 제어
	bool bShouldBeVisible = DistSq < CullDistSqThreshold;

	const bool bIsRTSMode = GS_Rendering::IsRTSMode(this);

	// 캐싱된 컴포넌트만 순회하여 최적화
	for (TObjectPtr<UPrimitiveComponent> PrimComp : OptimizedComponents)
	{
		if (!PrimComp || !IsValid(PrimComp))
			continue;

		// 개별 컴포넌트 위치 기준으로 거리 계산
		const float CompDistSq = FVector::DistSquared(PrimComp->GetComponentLocation(), CameraLoc);
		bool bCompShouldBeVisible = CompDistSq < CullDistSqThreshold;

		// 지오메트리 캐시의 경우 에셋이 없으면 렌더링 상태 업데이트 시 크래시 위험이 있음
		if (UGeometryCacheComponent* GeoComp = Cast<UGeometryCacheComponent>(PrimComp))
		{
			if (!GeoComp->GetGeometryCache())
				continue;
		}

		if (PrimComp->ComponentTags.Contains("IgnoreCulling"))
			continue;

		// 태그 기반 가시성 판단
		if (bCompShouldBeVisible)
		{
			if (PrimComp->ComponentTags.Contains(FName("Hidden")) ||
			    (bIsRTSMode && PrimComp->ComponentTags.Contains(FName("RTS"))))
			{
				bCompShouldBeVisible = false;
			}
		}

		if (PrimComp->GetVisibleFlag() != bCompShouldBeVisible)
		{
			PrimComp->SetVisibility(bCompShouldBeVisible);
		}

		if (bCompShouldBeVisible)
		{
			GS_Rendering::UpdateShadowCulling(this, PrimComp);
		}
	}
}

void AGS_EnvironmentProp::RegisterSignificanceManager()
{
	if (USignificanceManager* SM = USignificanceManager::Get(GetWorld()))
	{
		TWeakObjectPtr<AGS_EnvironmentProp> WeakThis(this);
		SM->RegisterObject(
		    this, "Environment",
		    [WeakThis](USignificanceManager::FManagedObjectInfo* ObjectInfo, const FTransform& Viewpoint) -> float
		    {
			    if (AGS_EnvironmentProp* StrongThis = WeakThis.Get())
				    return StrongThis->CalculateSignificance(Viewpoint);
			    return 0.0f;
		    },
		    USignificanceManager::EPostSignificanceType::Sequential,
		    [WeakThis](USignificanceManager::FManagedObjectInfo* ObjectInfo, float OldValue, float NewValue, bool bExternal)
		    {
			    if (AGS_EnvironmentProp* StrongThis = WeakThis.Get())
				    StrongThis->OnSignificanceChanged(NewValue);
		    });
	}
}

float AGS_EnvironmentProp::CalculateSignificance(const FTransform& Viewpoint)
{
	float DistSq = FVector::DistSquared(GetActorLocation(), Viewpoint.GetLocation());
	const float FinalCullDistance = GS_Rendering::CalculateCullDistance(this, BaseCullDistance);

	float MaxDistSq = FMath::Square(FinalCullDistance * 1.1f);
	return FMath::Clamp(1.0f - (DistSq / MaxDistSq), 0.1f, 1.0f);
}

void AGS_EnvironmentProp::OnSignificanceChanged(float NewSignificance)
{
	float NewInterval = GS_Rendering::GetAdaptiveTimerInterval(NewSignificance);
	if (GetWorld())
	{
		float RemainingTime = GetWorld()->GetTimerManager().GetTimerRemaining(ShadowCullingTimerHandle);
		GetWorld()->GetTimerManager().SetTimer(ShadowCullingTimerHandle, this, &AGS_EnvironmentProp::UpdateCulling, NewInterval, true, FMath::Max(0.01f, RemainingTime));
	}
}
