// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GS_VFX_FunctionLibrary.generated.h"


class UNiagaraSystem;

/**
 *
 */
UCLASS()
class GAS_API UGS_VFX_FunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VFX")
	static void PlayBloodEffect(UObject* WorldContextObject,
								UNiagaraSystem* BloodEffectSystem,
								const FVector& Location, const FRotator& Rotation,
								float Scale = 1.0f);

	/**
	 * 주어진 위치에서 VFX를 재생해야 하는지 결정합니다. (거리 및 프러스텀 컬링)
	 * @param WorldContextObject 월드 컨텍스트
	 * @param Location VFX 재생 위치
	 * @param MaxDistance 최대 가시 거리
	 * @param bCheckFrustum 시야각(Frustum) 체크 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "VFX",
			  meta = (WorldContext = "WorldContextObject"))
	static bool ShouldPlayVFXAtLocation(const UObject* WorldContextObject,
										const FVector& Location,
										float MaxDistance = 5000.0f,
										bool bCheckFrustum = true);
};
