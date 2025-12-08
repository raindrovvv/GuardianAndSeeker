// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GS_MinimapCapture.generated.h"

// Forward Declarations
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/**
 * 미니맵 SceneCapture 최적화 액터
 *
 * 역할:
 * - 업데이트 빈도 조절 (타이머 기반)
 * - ShowFlags 최적화 (불필요한 렌더링 비활성화)
 * - Custom Depth 필터링 (적 캐릭터만 표시)
 */
UCLASS()
class GAS_API AGS_MinimapCapture : public AActor
{
	GENERATED_BODY()

public:
	AGS_MinimapCapture();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
	TObjectPtr<USceneCaptureComponent2D> SceneCaptureComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	/** 캡처 업데이트 주기 (초) - 0이면 매 프레임 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Performance", meta = (ClampMin = "0.0"))
	float CaptureUpdateInterval = 0.1f; // 10 FPS

	/** LOD 레벨 강제 설정 (0 = 최고 품질, 3 = 최저 품질) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Performance", meta = (ClampMin = "0", ClampMax = "3"))
	int32 ForcedLODLevel = 2;

	/** Custom Depth 사용 여부 (적만 표시) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Rendering")
	bool bUseCustomDepthFilter = false;

	/** 미니맵에서 숨길 액터 목록 (레벨에 배치된 액터 직접 지정) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Rendering")
	TArray<TObjectPtr<AActor>> InitialHiddenActors;

	/** 미니맵 캡처 위치 (Z축 높이 조절) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Transform")
	FVector CaptureLocation = FVector(0.0f, 0.0f, 6000.0f);

	/** 미니맵 캡처 회전 (기본: 위에서 아래) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Transform")
	FRotator CaptureRotation = FRotator(-90.0f, 0.0f, 0.0f);

private:
	FTimerHandle TimerHandle_CaptureUpdate;

	void InitializeSceneCapture();
	void OptimizeShowFlags();
	void HideDungeonCeilings();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void CaptureScene();
};
