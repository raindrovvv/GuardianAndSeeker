#include "Rendering/GS_RenderingConstants.h"
#include "Kismet/GameplayStatics.h"
#include "AI/RTS/GS_RTSController.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"

namespace GS_Rendering
{
bool IsRTSMode(const UObject* WorldContext)
{
	if (!WorldContext)
		return false;

	// 데디케이티드 서버에는 로컬 플레이어가 없으므로 항상 false (렌더링 최적화용)
	if (FApp::CanEverRender() == false)
		return false;

	UWorld* World = WorldContext->GetWorld();
	if (!World)
		return false;

	// 로컬 플레이어 컨트롤러 확인
	APlayerController* PC = World->GetFirstPlayerController();
	return PC && PC->IsA(AGS_RTSController::StaticClass());
}

float CalculateCullDistance(const UObject* WorldContext, float BaseDistance)
{
	float Scale = IsRTSMode(WorldContext) ? RTS_CULL_DISTANCE_SCALE : TPS_CULL_DISTANCE_SCALE;
	return BaseDistance * Scale;
}

int32 CalculateMinLOD(const UObject* WorldContext)
{
	return IsRTSMode(WorldContext) ? RTS_MIN_LOD : TPS_MIN_LOD;
}

float CalculateNetUpdateFrequency(const UObject* WorldContext, const FVector& ActorLocation)
{
	if (!WorldContext)
		return NET_UPDATE_FREQ_MIN;

	UWorld* World = WorldContext->GetWorld();
	if (!World)
		return NET_UPDATE_FREQ_MIN;

	float MinDistanceSq = TNumericLimits<float>::Max();
	bool bAnyPlayerNear = false;

	// 서버에서는 모든 연결된 플레이어와의 거리를 체크해야 함
	if (World->GetNetMode() == NM_DedicatedServer || World->GetNetMode() == NM_ListenServer)
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PC = Iterator->Get();
			if (PC && PC->GetPawn())
			{
				float DistSq = FVector::DistSquared(ActorLocation, PC->GetPawn()->GetActorLocation());
				if (DistSq < MinDistanceSq)
				{
					MinDistanceSq = DistSq;
					bAnyPlayerNear = true;
				}
			}
		}
	}
	else // 클라이언트 로컬 체크
	{
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			MinDistanceSq = FVector::DistSquared(ActorLocation, PC->PlayerCameraManager->GetCameraLocation());
			bAnyPlayerNear = true;
		}
	}

	if (!bAnyPlayerNear)
		return NET_UPDATE_FREQ_FAR;

	float Distance = FMath::Sqrt(MinDistanceSq);

	// 거리 기반 네트워크 업데이트 빈도 계산
	if (Distance < NET_DISTANCE_CLOSE)
	{
		return NET_UPDATE_FREQ_CLOSE; // 80m 이내
	}
	else if (Distance < NET_DISTANCE_MEDIUM)
	{
		return NET_UPDATE_FREQ_MEDIUM; // 80-150m
	}
	else
	{
		return NET_UPDATE_FREQ_FAR; // 150m 이상
	}
}

float CalculateAIPerceptionDistance(const UObject* WorldContext)
{
	// 서버 AI는 "가디언(RTS)"이 게임에 존재할 수 있으므로 항상 최대 인지 범위를 유지하는 것이 안전함
	// (가디언의 명령 사거리와 몬스터의 반응 사거리를 일치시키기 위함)
	if (WorldContext && WorldContext->GetWorld() && WorldContext->GetWorld()->IsNetMode(NM_DedicatedServer))
	{
		return AI_PERCEPTION_DISTANCE_RTS;
	}

	return IsRTSMode(WorldContext) ? AI_PERCEPTION_DISTANCE_RTS : AI_PERCEPTION_DISTANCE_TPS;
}

void UpdateShadowCulling(const AActor* Actor, USceneComponent* MeshComp)
{
	if (!Actor || !MeshComp)
		return;

	UWorld* World = Actor->GetWorld();
	if (!World)
		return;

	// 데디케이티드 서버에서는 그림자 연산이 필요 없음
	if (FApp::CanEverRender() == false)
		return;

	UPrimitiveComponent* PrimitiveMesh = Cast<UPrimitiveComponent>(MeshComp);
	if (!PrimitiveMesh)
		return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (PC && PC->PlayerCameraManager)
	{
		float DistSq = FVector::DistSquared(Actor->GetActorLocation(), PC->PlayerCameraManager->GetCameraLocation());

		// 1. 전체 그림자 활성화 여부 (SHADOW_DISABLE_DISTANCE - 80m)
		bool bCastAnyShadow = DistSq < (SHADOW_DISABLE_DISTANCE * SHADOW_DISABLE_DISTANCE);

		// 2. 동적 그림자 활성화 여부 (DYNAMIC_SHADOW_DISABLE_DISTANCE - 40m)
		// 40~80m 사이는 정적 그림자만 출력하여 비용 절감 및 시각적 안정성 확보
		bool bCastDynamicShadow = DistSq < (DYNAMIC_SHADOW_DISABLE_DISTANCE * DYNAMIC_SHADOW_DISABLE_DISTANCE);

		if (PrimitiveMesh->CastShadow != bCastAnyShadow)
		{
			PrimitiveMesh->SetCastShadow(bCastAnyShadow);
		}

		if (PrimitiveMesh->bCastDynamicShadow != bCastDynamicShadow)
		{
			PrimitiveMesh->bCastDynamicShadow = bCastDynamicShadow;
		}
	}
}
} // namespace GS_Rendering
