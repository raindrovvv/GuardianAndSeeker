// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/RTS/GS_RTSCamera.h"
#include "System/GameMode/GS_InGameGM.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Math/Box2D.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "System/GS_PlayerState.h"
#include "System/GS_PlayerRole.h"
#include "UObject/UnrealType.h"

// Sets default values
AGS_RTSCamera::AGS_RTSCamera()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AGS_RTSCamera::BeginPlay()
{
	Super::BeginPlay();

	// 멀티플레이어 환경에서 시커 플레이어에게는 구름 효과가 보이지 않도록 처리
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AGS_PlayerState* PS = PC->GetPlayerState<AGS_PlayerState>())
			{
				if (PS->CurrentPlayerRole != EPlayerRole::PR_Guardian)
				{
					// 시커 플레이어인 경우 구름 효과 로직을 실행하지 않고 틱을 비활성화함
					SetActorTickEnabled(false);
					return;
				}
			}
		}
	}

	// Post Process Setup
	if (CloudMaterialBase)
	{
		CloudMaterialInstance = UMaterialInstanceDynamic::Create(CloudMaterialBase, this);
		if (CloudMaterialInstance)
		{
			UpdateCloudMaterialParameters();

			if (UCameraComponent* CameraComp = GetCameraComponent())
			{
				CameraComp->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, CloudMaterialInstance));
			}
		}
	}

	// Niagara Setup
	if (CloudNiagaraSystem)
	{
		CloudNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		    CloudNiagaraSystem,
		    GetCameraComponent(),
		    NAME_None,
		    FVector::ZeroVector,
		    FRotator::ZeroRotator,
		    EAttachLocation::SnapToTarget,
		    true);
	}

	// Sound Setup
	if (CloudWindSound)
	{
		CloudWindAudioComponent = UGameplayStatics::SpawnSound2D(
		    this,
		    CloudWindSound,
		    0.0f, // Start with 0 volume
		    1.0f,
		    0.0f,
		    nullptr,
		    true,
		    true);

		if (CloudWindAudioComponent)
		{
			CloudWindAudioComponent->bAutoDestroy = false;
			CloudWindAudioComponent->Play();
		}
	}

	// Cache components
	CachedCameraComp = FindComponentByClass<UCameraComponent>();
	CachedSpringArmComp = FindComponentByClass<USpringArmComponent>();
}

void AGS_RTSCamera::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateCloudMaterialParameters();
}

void AGS_RTSCamera::UpdateCloudMaterialParameters()
{
	if (CloudMaterialInstance)
	{
		CloudMaterialInstance->SetScalarParameterValue(FName("CloudHeightMin"), CloudHeightMin);
		CloudMaterialInstance->SetScalarParameterValue(FName("CloudHeightMax"), CloudHeightMax);
		CloudMaterialInstance->SetVectorParameterValue(FName("CloudFogColor"), CloudFogColor);
	}
}

void AGS_RTSCamera::UpdateCloudNiagaraParameters()
{
	if (CloudNiagaraComponent && GetCameraComponent())
	{
		FVector CamLoc = GetCameraComponent()->GetComponentLocation();
		float CurrentZ = CamLoc.Z;

		// Calculate Alpha based on height (Same logic as Material)
		float Alpha = FMath::GetMappedRangeValueClamped(
		    FVector2D(CloudHeightMin, CloudHeightMax),
		    FVector2D(0.0f, 1.0f),
		    CurrentZ);

		CloudNiagaraComponent->SetVariableFloat(FName("CloudAlpha"), Alpha);
	}
}

void AGS_RTSCamera::UpdateCloudSoundParameters()
{
	if (CloudWindAudioComponent && GetCameraComponent())
	{
		FVector CamLoc = GetCameraComponent()->GetComponentLocation();
		float CurrentZ = CamLoc.Z;

		// Calculate Volume Alpha
		float VolumeAlpha = FMath::GetMappedRangeValueClamped(
		    FVector2D(CloudHeightMin, CloudHeightMax),
		    FVector2D(0.0f, 1.0f),
		    CurrentZ);

		CloudWindAudioComponent->SetVolumeMultiplier(VolumeAlpha);
	}
}

// Called every frame
void AGS_RTSCamera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Only update effects if camera height changed significantly
	if (UCameraComponent* CameraComp = GetCameraComponent())
	{
		float CurrentZ = CameraComp->GetComponentLocation().Z;
		if (!FMath::IsNearlyEqual(CurrentZ, LastCameraZ, 1.0f)) // 1cm tolerance
		{
			LastCameraZ = CurrentZ;
			UpdateCloudNiagaraParameters();
			UpdateCloudSoundParameters();
		}
	}

	UpdateZoomBack(DeltaTime);
}

UCameraComponent* AGS_RTSCamera::GetCameraComponent() const
{
	if (CachedCameraComp)
	{
		return CachedCameraComp;
	}
	// Fallback or lazy load (FindComponentByClass is non-const, so cast away constness if needed, but FindComponentByClass is const-safe usually)
	// Just return result of FindComponent directly if cache missing, but better to update cache if possible (can't in const function without mutable)
	return FindComponentByClass<UCameraComponent>();
}

USpringArmComponent* AGS_RTSCamera::GetSpringArmComponent() const
{
	if (CachedSpringArmComp)
	{
		return CachedSpringArmComp;
	}
	return FindComponentByClass<USpringArmComponent>();
}

void AGS_RTSCamera::UpdateZoomBack(float DeltaTime)
{
	if (!bEnableZoomBack)
		return;

	UCameraComponent* CameraComp = GetCameraComponent();
	USpringArmComponent* SpringArmComp = GetSpringArmComponent();

	if (!CameraComp || !SpringArmComp)
		return;

	float CurrentHeight = CameraComp->GetComponentLocation().Z;

	// 히스테리시스 적용 (트리거 높이는 높게, 리셋 높이는 낮게)
	const float TriggerThreshold = CloudHeightMin + 100.0f;
	const float ResetThreshold = CloudHeightMin;

	// 1. 줌백 로직 (보간 진행 중)
	if (bIsZoomingBack)
	{
		float CurrentArmLength = SpringArmComp->TargetArmLength;
		float NewArmLength = FMath::FInterpTo(CurrentArmLength, ZoomBackSafeArmLength, DeltaTime, ZoomBackSpeed);
		SpringArmComp->TargetArmLength = NewArmLength;

		// 동기화 로직
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (FProperty* Prop = PC->GetClass()->FindPropertyByName(TEXT("ZoomFactor")))
			{
				if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Prop))
				{
					float NewFactor = (NewArmLength - 1500.0f) / 500.0f;
					NumericProp->SetFloatingPointPropertyValue(NumericProp->ContainerPtrToValuePtr<void>(PC), NewFactor);
				}
			}
		}

		// 종료 조건 (안전 높이 이하 도달 또는 보간 완료)
		if (CurrentHeight <= ResetThreshold || FMath::IsNearlyEqual(NewArmLength, ZoomBackSafeArmLength, 1.0f) || CurrentArmLength < ZoomBackSafeArmLength - 10.0f)
		{
			bIsZoomingBack = false;
			bIsWaitingToZoomBack = false;
			ZoomBackTimer = 0.0f;
		}
		return;
	}

	// 2. 구름 영역 진입 체크
	if (CurrentHeight > TriggerThreshold)
	{
		bIsWaitingToZoomBack = true;
		ZoomBackTimer += DeltaTime;

		if (ZoomBackTimer >= ZoomBackDelay)
		{
			bIsZoomingBack = true;
			bIsWaitingToZoomBack = false;
			ZoomBackTimer = 0.0f;
		}
	}
	else if (CurrentHeight <= ResetThreshold)
	{
		// 안전한 높이 아래로 내려왔을 때만 리셋
		bIsWaitingToZoomBack = false;
		bIsZoomingBack = false;
		ZoomBackTimer = 0.0f;
	}
}

bool AGS_RTSCamera::HasCameraChanged() const
{
	UCameraComponent* CameraComp = GetCameraComponent();
	USpringArmComponent* SpringArmComp = GetSpringArmComponent();

	if (!CameraComp)
	{
		return true;
	}

	FVector CurrentLocation = CameraComp->GetComponentLocation();
	FRotator CurrentRotation = CameraComp->GetComponentRotation();
	float CurrentArmLength = SpringArmComp ? SpringArmComp->TargetArmLength : 0.0f;
	float CurrentAspectRatio = CameraComp->AspectRatio;

	// Viewport Size Check (if AspectRatio is not fixed)
	int32 CurrentSizeX = 0;
	int32 CurrentSizeY = 0;
	if (CurrentAspectRatio <= 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				PC->GetViewportSize(CurrentSizeX, CurrentSizeY);
			}
		}
	}

	// 위치/회전/줌이 변경되었는지 체크 (오차 허용)
	const float LocationTolerance = 1.0f; // 1cm
	const float RotationTolerance = 0.1f; // 0.1도
	const float ArmLengthTolerance = 1.0f; // 1cm

	bool bLocationChanged = !CurrentLocation.Equals(LastCameraLocation, LocationTolerance);
	bool bRotationChanged = !CurrentRotation.Equals(LastCameraRotation, RotationTolerance);
	bool bArmLengthChanged = FMath::Abs(CurrentArmLength - LastArmLength) > ArmLengthTolerance;
	bool bAspectRatioChanged = !FMath::IsNearlyEqual(CurrentAspectRatio, LastAspectRatio);
	bool bViewportSizeChanged = (CurrentSizeX != LastViewportSizeX) || (CurrentSizeY != LastViewportSizeY);

	return bLocationChanged || bRotationChanged || bArmLengthChanged || bAspectRatioChanged || bViewportSizeChanged;
}

FBox2D AGS_RTSCamera::GetSimpleViewBounds() const
{
	// 캐시가 유효하고 카메라가 변경되지 않았으면 캐시 반환
	if (bViewBoundsCacheValid && !HasCameraChanged())
	{
		return CachedViewBounds;
	}

	// 카메라 컴포넌트 가져오기
	UCameraComponent* CameraComp = GetCameraComponent();
	USpringArmComponent* SpringArmComp = GetSpringArmComponent();

	if (!CameraComp)
	{
		// 컴포넌트가 없으면 기본값 반환
		FVector CameraLocation = GetActorLocation();
		return FBox2D(
		    FVector2D(CameraLocation.X - 1000.0f, CameraLocation.Y - 1000.0f),
		    FVector2D(CameraLocation.X + 1000.0f, CameraLocation.Y + 1000.0f));
	}

	// 4 Corner Raycasting Method for Accurate Bounds
	FVector CamLoc = CameraComp->GetComponentLocation();
	FRotator CamRot = CameraComp->GetComponentRotation();

	float HFOV = FMath::DegreesToRadians(CameraComp->FieldOfView);
	float AspectRatio = CameraComp->AspectRatio;

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;

	// If AspectRatio is not constrained, calculate it from Viewport
	if (AspectRatio <= 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
				if (ViewportSizeY > 0)
				{
					AspectRatio = (float)ViewportSizeX / (float)ViewportSizeY;
				}
			}
		}
	}

	// Fallback
	if (AspectRatio <= 0.0f)
		AspectRatio = 1.777f; // Default 16:9

	// Calculate VFOV based on HFOV and AspectRatio
	// tan(V/2) = tan(H/2) / AspectRatio
	float TanHalfHFOV = FMath::Tan(HFOV * 0.5f);
	float TanHalfVFOV = TanHalfHFOV / AspectRatio;

	// 4 Corner Rays in View Space (X=Forward, Y=Right, Z=Up)
	// Top Right: (1, TanH, TanV)
	FVector DirTL(1.0f, -TanHalfHFOV, TanHalfVFOV);
	FVector DirTR(1.0f, TanHalfHFOV, TanHalfVFOV);
	FVector DirBL(1.0f, -TanHalfHFOV, -TanHalfVFOV);
	FVector DirBR(1.0f, TanHalfHFOV, -TanHalfVFOV);

	// Rotate to World Space
	FVector WorldDirTL = CamRot.RotateVector(DirTL);
	FVector WorldDirTR = CamRot.RotateVector(DirTR);
	FVector WorldDirBL = CamRot.RotateVector(DirBL);
	FVector WorldDirBR = CamRot.RotateVector(DirBR);

	// Intersect with Z=0 Plane
	// t = -CamZ / DirZ
	auto IntersectGround = [&](const FVector& Dir) -> FVector2D
	{
		// Prevent divide by zero or looking up (Dir.Z should be negative)
		if (Dir.Z >= -0.01f)
		{
			// Fallback: project forward a fixed distance if looking parallel/up
			FVector Point = CamLoc + Dir * 5000.0f;
			return FVector2D(Point.X, Point.Y);
		}

		float t = -CamLoc.Z / Dir.Z;
		FVector Point = CamLoc + Dir * t;
		return FVector2D(Point.X, Point.Y);
	};

	FVector2D P1 = IntersectGround(WorldDirTL);
	FVector2D P2 = IntersectGround(WorldDirTR);
	FVector2D P3 = IntersectGround(WorldDirBL);
	FVector2D P4 = IntersectGround(WorldDirBR);

	// Compute AABB
	float MinX = FMath::Min(FMath::Min(P1.X, P2.X), FMath::Min(P3.X, P4.X));
	float MaxX = FMath::Max(FMath::Max(P1.X, P2.X), FMath::Max(P3.X, P4.X));
	float MinY = FMath::Min(FMath::Min(P1.Y, P2.Y), FMath::Min(P3.Y, P4.Y));
	float MaxY = FMath::Max(FMath::Max(P1.Y, P2.Y), FMath::Max(P3.Y, P4.Y));

	FBox2D ResultBounds = FBox2D(FVector2D(MinX, MinY), FVector2D(MaxX, MaxY));

	// 캐시 업데이트
	CachedViewBounds = ResultBounds;
	LastCameraLocation = CamLoc;
	LastCameraRotation = CamRot;
	LastArmLength = SpringArmComp ? SpringArmComp->TargetArmLength : 0.0f;
	LastAspectRatio = CameraComp->AspectRatio;
	LastViewportSizeX = ViewportSizeX;
	LastViewportSizeY = ViewportSizeY;
	bViewBoundsCacheValid = true;

	return ResultBounds;
}
