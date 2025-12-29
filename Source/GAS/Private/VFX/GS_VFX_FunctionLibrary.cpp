// Copyright Epic Games, Inc. All Rights Reserved.

#include "VFX/GS_VFX_FunctionLibrary.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

void UGS_VFX_FunctionLibrary::PlayBloodEffect(UObject* WorldContextObject,
											  UNiagaraSystem* BloodEffectSystem,
											  const FVector& Location,
											  const FRotator& Rotation,
											  float Scale)
{
	UWorld* World = GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UNiagaraSystem* EffectToPlay = BloodEffectSystem;

	// BloodEffectSystem이 없으면 기본 혈흔 이펙트 사용
	if (!EffectToPlay)
	{
		static UNiagaraSystem* DefaultBloodEffect = LoadObject<UNiagaraSystem>(
			nullptr, TEXT("/Game/VFX/RealisticBlood/Burst/Niagara/"
						  "NS_BloodBurst_High.NS_BloodBurst_High"));
		EffectToPlay = DefaultBloodEffect;
	}

	if (EffectToPlay)
	{
		// 스케일 적용
		FVector BloodScale = FVector(Scale);

		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, EffectToPlay, Location, Rotation, BloodScale, true, true);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("BloodEffect could not be loaded or spawned"));
	}
}

bool UGS_VFX_FunctionLibrary::ShouldPlayVFXAtLocation(const UObject* WorldContextObject,
													  const FVector& Location,
													  float MaxDistance,
													  bool bCheckFrustum)
{
	if (!WorldContextObject)
	{
		return false;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World || World->IsNetMode(NM_DedicatedServer))
	{
		return false;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	// 1. 거리 체크
	float DistSq = FVector::DistSquared(CameraLocation, Location);
	if (DistSq > FMath::Square(MaxDistance))
	{
		return false;
	}

	// 2. 프러스텀(Frustum) 체크
	if (bCheckFrustum)
	{
		FVector DirToVFX = (Location - CameraLocation).GetSafeNormal();
		float DotProduct = FVector::DotProduct(CameraRotation.Vector(), DirToVFX);

		// 점 곱이 0보다 작으면 카메라 뒤에 있음 (FOV 고려 없이 대략적인 체크)
		if (DotProduct < 0.0f)
		{
			return false;
		}
	}

	return true;
}
