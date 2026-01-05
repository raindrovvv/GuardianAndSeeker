// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Math/Box2D.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "GS_RTSCamera.generated.h"

UCLASS()
class GAS_API AGS_RTSCamera : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AGS_RTSCamera();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void HideWallAndCeiling();

	// 기존 블루프린트 컴포넌트들에 접근하기 위한 헬퍼 함수들
	UFUNCTION(BlueprintCallable, Category = "Camera")
	UCameraComponent* GetCameraComponent() const;

	UFUNCTION(BlueprintCallable, Category = "Camera")
	USpringArmComponent* GetSpringArmComponent() const;

	// 간단한 뷰포트 경계 계산 (기존 컴포넌트 활용)
	UFUNCTION(BlueprintCallable, Category = "Camera")
	FBox2D GetSimpleViewBounds() const;

	UFUNCTION(BlueprintPure, Category = "Camera|ZoomBack")
	bool IsZoomingBack() const { return bIsZoomingBack; }

	// --- Cloud Fog Effect ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|CloudFog")
	UMaterialInterface* CloudMaterialBase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|CloudFog")
	float CloudHeightMin = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|CloudFog")
	float CloudHeightMax = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|CloudFog")
	FLinearColor CloudFogColor = FLinearColor(0.8f, 0.9f, 1.0f, 1.0f);

	// --- Niagara Cloud Effect ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|CloudFog")
	class UNiagaraSystem* CloudNiagaraSystem;

	// --- Cloud Wind Sound ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|CloudFog")
	USoundBase* CloudWindSound;

	// --- Zoom-Back Effect ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|ZoomBack")
	bool bEnableZoomBack = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|ZoomBack")
	float ZoomBackDelay = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|ZoomBack")
	float ZoomBackSpeed = 2.0f;

	// 구름이 완전히 가리기 전 되돌아갈 안전한 높이 (알파 0.1 지점 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|ZoomBack")
	float ZoomBackSafeArmLength = 1500.0f;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* CloudMaterialInstance;

	UPROPERTY(Transient)
	class UNiagaraComponent* CloudNiagaraComponent;

	UPROPERTY(Transient)
	UAudioComponent* CloudWindAudioComponent;

	void UpdateCloudMaterialParameters();
	void UpdateCloudNiagaraParameters();
	void UpdateCloudSoundParameters();
	void UpdateZoomBack(float DeltaTime);

private:
	// 캐싱된 뷰 경계
	mutable FBox2D CachedViewBounds;
	mutable FVector LastCameraLocation;
	mutable FRotator LastCameraRotation;
	mutable float LastArmLength;
	mutable float LastAspectRatio;
	mutable int32 LastViewportSizeX;
	mutable int32 LastViewportSizeY;
	mutable bool bViewBoundsCacheValid = false;

	// 캐시 무효화 체크
	bool HasCameraChanged() const;

	// Optimization for Tick
	float LastCameraZ = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CachedCameraComp;

	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CachedSpringArmComp;

	// Zoom-Back State
	bool bIsWaitingToZoomBack = false;
	bool bIsZoomingBack = false;
	float ZoomBackTimer = 0.0f;
};
