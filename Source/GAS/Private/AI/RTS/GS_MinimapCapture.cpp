// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/RTS/GS_MinimapCapture.h"
#include "DrawDebugHelpers.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Props/GS_RoomBase.h"
#include "EngineUtils.h"

AGS_MinimapCapture::AGS_MinimapCapture()
{
	PrimaryActorTick.bCanEverTick = false;

	// SceneCapture 컴포넌트 생성
	SceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCaptureComponent"));
	SetRootComponent(SceneCaptureComponent);
}

void AGS_MinimapCapture::BeginPlay()
{
	Super::BeginPlay();

	// Blueprint에서 설정한 Transform 적용
	// 위치 오차 방지를 위해 X, Y는 0으로 강제 (월드 중심 기준)
	CaptureLocation.X = 0.0f;
	CaptureLocation.Y = 0.0f;
	SetActorLocation(CaptureLocation);
	SetActorRotation(CaptureRotation);

	InitializeSceneCapture();
	OptimizeShowFlags();
	HideDungeonCeilings(); // 천장 숨기기

	// 타이머 시작
	if (CaptureUpdateInterval > 0.0f)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			World->GetTimerManager().SetTimer(
				TimerHandle_CaptureUpdate,
				this,
				&AGS_MinimapCapture::CaptureScene,
				CaptureUpdateInterval,
				true // Loop
			);
		}
	}
	else
	{
		// 매 프레임 캡처 (Interval = 0)
		SceneCaptureComponent->bCaptureEveryFrame = true;
	}

	// 즉시 한 번 캡처
	CaptureScene();
}

void AGS_MinimapCapture::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(TimerHandle_CaptureUpdate);
	}

	Super::EndPlay(EndPlayReason);
}

void AGS_MinimapCapture::InitializeSceneCapture()
{
	if (!SceneCaptureComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[MinimapCapture] SceneCaptureComponent가 없습니다!"));
		return;
	}

	// RenderTarget 설정
	if (RenderTarget)
	{
		SceneCaptureComponent->TextureTarget = RenderTarget;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[MinimapCapture] RenderTarget이 nullptr입니다! Blueprint에서 할당하세요."));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[MinimapCapture] Error: RenderTarget is Missing! Please assign it in Blueprint."));
		}
	}

	// 기본 설정
	SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	SceneCaptureComponent->OrthoWidth = 18000.0f; // 미니맵 커버 범위
	SceneCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCaptureComponent->bCaptureEveryFrame = false; // 타이머로 제어
	SceneCaptureComponent->bCaptureOnMovement = false;

	// Primitive Render Mode: 모든 프리미티브 렌더링 (HiddenComponents만 제외)
	SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;

	// 렌더링 상태 유지 (수동 캡처 시 깜빡임 방지)
	SceneCaptureComponent->bAlwaysPersistRenderingState = true;

	// 노출 고정 (밝기 깜빡임 방지)
	SceneCaptureComponent->PostProcessSettings.bOverride_AutoExposureMethod = true;
	SceneCaptureComponent->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	SceneCaptureComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
	SceneCaptureComponent->PostProcessSettings.AutoExposureBias = 0.0f;

	// LOD 설정 (ForcedLODLevel에 따라 LODDistanceFactor 조절)
	// 값이 클수록 LOD가 빨리 떨어짐 (저품질, 고성능)
	switch (ForcedLODLevel)
	{
	case 0: SceneCaptureComponent->LODDistanceFactor = 1.0f; break; // High
	case 1: SceneCaptureComponent->LODDistanceFactor = 2.0f; break; // Medium
	case 2: SceneCaptureComponent->LODDistanceFactor = 4.0f; break; // Low
	case 3: SceneCaptureComponent->LODDistanceFactor = 10.0f; break; // Very Low
	default: SceneCaptureComponent->LODDistanceFactor = 1.0f; break;
	}

	// 렌더링 거리 제한 (불필요한 원거리 객체 렌더링 방지)
	// OrthoWidth가 18000이므로, 깊이도 비슷하게 설정하여 과도한 렌더링 방지
	SceneCaptureComponent->MaxViewDistanceOverride = 20000.0f;

	// Hidden Actors 설정
	// 에디터에서 설정한 'InitialHiddenActors' 목록을 SceneCapture의 숨김 목록에 추가
	if (InitialHiddenActors.Num() > 0)
	{
		for (const auto& HiddenActor : InitialHiddenActors)
		{
			if (HiddenActor)
			{
				SceneCaptureComponent->HiddenActors.AddUnique(HiddenActor);
			}
		}
	}
}

void AGS_MinimapCapture::OptimizeShowFlags()
{
	if (!SceneCaptureComponent) return;

	FEngineShowFlags& ShowFlags = SceneCaptureComponent->ShowFlags;

	// ========== 필수 유지 (미니맵에 보여야 함) ==========
	ShowFlags.SetStaticMeshes(true);      // 바닥, 벽 (StaticMesh)
	ShowFlags.SetLandscape(true);         // 지형
	ShowFlags.SetTranslucency(true);      // Paper2D 스프라이트 (인디케이터)
	ShowFlags.SetLighting(true);          // 기본 라이팅 (너무 어둡지 않게)
	ShowFlags.SetDecals(true);

	// ========== 성능 최적화 (비활성화) ==========
	ShowFlags.SetAntiAliasing(false);
	ShowFlags.SetAtmosphere(false);
	ShowFlags.SetBSP(false);
	ShowFlags.SetFog(false);
	ShowFlags.SetParticles(false);
	ShowFlags.SetSkeletalMeshes(false);   // 캐릭터 3D 모델은 숨김
	ShowFlags.SetPostProcessing(false);
	ShowFlags.SetVolumetricFog(false);
	ShowFlags.SetMotionBlur(false);
	ShowFlags.SetTemporalAA(false);
	ShowFlags.SetBloom(false);
	ShowFlags.SetEyeAdaptation(false);
	ShowFlags.SetLensFlares(false);
	ShowFlags.SetVignette(false);
	ShowFlags.SetGrain(false);
	ShowFlags.SetDepthOfField(false);
	ShowFlags.SetAmbientOcclusion(false);
	ShowFlags.SetDynamicShadows(false);
	ShowFlags.SetWidgetComponents(false); // UI 위젯(체력바 등)은 미니맵에 찍히지 않도록 숨김

	// ========== Custom Depth 필터 (사용 안 함) ==========
	// bUseCustomDepthFilter는 기본값 false로 설정
	// 모든 StaticMesh와 Translucency를 렌더링
}

void AGS_MinimapCapture::HideDungeonCeilings()
{
	if (!SceneCaptureComponent) return;

	UWorld* World = GetWorld();
	if (!World) return;

	int32 CeilingCount = 0;
	int32 TaggedCount = 0;

	// 1. AGS_RoomBase의 Ceiling 컴포넌트 자동 숨김
	for (TActorIterator<AGS_RoomBase> It(World); It; ++It)
	{
		AGS_RoomBase* Room = *It;
		if (Room && Room->Ceiling)
		{
			SceneCaptureComponent->HiddenComponents.AddUnique(Room->Ceiling);
			CeilingCount++;
		}
	}

	// 2. 'MinimapHide' 태그가 붙은 모든 컴포넌트 수동 숨김
	// (에디터에서 특정 메시에 Component Tags "MinimapHide"를 추가하면 됨)
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		TArray<UPrimitiveComponent*> Components;
		Actor->GetComponents(Components);

		for (UPrimitiveComponent* Comp : Components)
		{
			if (Comp && Comp->ComponentTags.Contains(TEXT("MinimapHide")))
			{
				SceneCaptureComponent->HiddenComponents.AddUnique(Comp);
				TaggedCount++;
			}
		}
	}
}

void AGS_MinimapCapture::CaptureScene()
{
	if (SceneCaptureComponent)
	{
		SceneCaptureComponent->CaptureScene();

		// 첫 캡처 시 로그 (한 번만)
		static bool bFirstCapture = true;
		if (bFirstCapture)
		{
			bFirstCapture = false;
		}
	}
}

void AGS_MinimapCapture::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
