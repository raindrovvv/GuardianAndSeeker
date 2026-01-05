#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/Component/GS_CameraShakeTypes.h"
#include "Character/Debuff/EDebuffType.h"
#include "Character/Component/GS_VFXComponent.h"
#include "Engine/DataAsset.h"
#include "GS_DrakharVFXComponent.generated.h"

class AGS_Drakhar;
class UNiagaraSystem;
class UNiagaraComponent;
class UArrowComponent;
class UGS_CameraShakeComponent;
class UGS_FootManagerComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Drakhar 디버프 VFX 설정을 위한 Data Asset
UCLASS(BlueprintType)
class GAS_API UGS_DrakharDebuffVFXDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 디버프 타입별 VFX 시스템 매핑 (디버프 적용 시)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	TMap<EDebuffType, UNiagaraSystem*> DebuffVFXMap;

	// 디버프 타입별 만료 VFX 시스템 매핑 (디버프 해제 시)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	TMap<EDebuffType, UNiagaraSystem*> DebuffExpireVFXMap;

	// 디버프 타입별 VFX 지속 시간 매핑
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	TMap<EDebuffType, float> DebuffVFXDurationMap;

	// VFX 스케일 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	TMap<EDebuffType, FVector> DebuffVFXScaleMap;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GAS_API UGS_DrakharVFXComponent : public UGS_VFXComponent
{
	GENERATED_BODY()

public:
	UGS_DrakharVFXComponent();

	// 파라미터명 상수화
	static const FName Param_DashDirection;
	static const FName Param_DashSpeed;
	static const FName Param_Scale;
	static const FName Param_CrackIntensity;
	static const FName Param_CrackRadius;
	static const FName Param_DustIntensity;
	static const FName Param_DustRadius;
	static const FName Param_WindStrength;

	// ======================
	// Drakhar 전용 VFX 제어 함수
	// ======================

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UPROPERTY(BlueprintReadOnly, Category = "VFX|Drakhar")
	UNiagaraComponent* ActiveWingRushVFXComponent;

	UPROPERTY(BlueprintReadOnly, Category = "VFX|Drakhar")
	UNiagaraComponent* ActiveDustVFXComponent;

	UPROPERTY(BlueprintReadOnly, Category = "VFX|Earthquake")
	UNiagaraComponent* ActiveGroundCrackVFXComponent;

	UPROPERTY(BlueprintReadOnly, Category = "VFX|Earthquake")
	UNiagaraComponent* ActiveDustCloudVFXComponent;

	UPROPERTY(BlueprintReadOnly, Category = "VFX|Drakhar")
	UNiagaraComponent* ActiveFlyingDustVFXComponent;

	void OnFlyStart();
	void OnFlyEnd();

	void OnUltimateStart();
	void OnEarthquakeStart();

	void StartWingRushVFX();
	void StopWingRushVFX();

	void StartDustVFX();
	void StopDustVFX();

	void StartGroundCrackVFX();
	void StopGroundCrackVFX();

	void StartDustCloudVFX();
	void StopDustCloudVFX();

	void PlayAttackHitVFX(FVector ImpactPoint);
	void PlayEarthquakeImpactVFX(const FVector& ImpactLocation);
	void PlayFeverEarthquakeImpactVFX(const FVector& ImpactLocation);

	void HandleDraconicProjectileImpact(const FVector& ImpactLocation, const FVector& ImpactNormal, bool bHitCharacter);

	void OnFeverModeChanged(bool bIsFeverMode);

	void ApplyFeverModeOverlay();
	void RemoveFeverModeOverlay();

protected:
	UPROPERTY()
	TObjectPtr<UGS_FootManagerComponent> FootManagerComponent;

	// Flying Dust VFX 위치 업데이트 타이머
	FTimerHandle FlyingDustUpdateTimerHandle;

	// 타이머/클린업 지연
	UPROPERTY(EditDefaultsOnly, Category = "VFX|Timing", meta = (ClampMin = "0.01"))
	float FlyingDustUpdateInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "VFX|Timing", meta = (ClampMin = "0.01"))
	float WingRushCleanupDelay = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "VFX|Timing", meta = (ClampMin = "0.01"))
	float DustCleanupDelay = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "VFX|Timing", meta = (ClampMin = "0.01"))
	float GroundCrackCleanupDelay = 3.0f;

	// Flying Dust VFX 위치 업데이트 (Timer 콜백)
	void UpdateFlyingDustVFXLocation();

	UPROPERTY()
	TObjectPtr<UArrowComponent> WingRushVFXSpawnPoint;

	UPROPERTY()
	TObjectPtr<UArrowComponent> EarthquakeVFXSpawnPoint;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FeverModeOverlayMID;

private:
	UPROPERTY()
	AGS_Drakhar* OwnerDrakhar;
};
