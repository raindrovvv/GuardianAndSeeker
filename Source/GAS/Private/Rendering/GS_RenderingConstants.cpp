#include "Rendering/GS_RenderingConstants.h"
#include "Kismet/GameplayStatics.h"
#include "AI/RTS/GS_RTSController.h"

namespace GS_Rendering
{
    bool IsRTSMode(const UObject* WorldContext)
    {
        if (!WorldContext) return false;
        
        UWorld* World = WorldContext->GetWorld();
        if (!World) return false;

        // 로컬 플레이어 컨트롤러 확인 (컬링은 로컬 클라이언트의 시점에 따라 결정됨)
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
}
