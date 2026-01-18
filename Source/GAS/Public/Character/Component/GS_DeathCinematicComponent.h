// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GS_DeathCinematicComponent.generated.h"

class UPostProcessComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USpringArmComponent;

/**
 * 죽음 판정 시 슬로우 모션 및 화면 효과 컴포넌트
 * 
 * - 로컬 플레이어에게만 적용 (멀티플레이 호환)
 * - OnDeath() 호출 시 발동
 * - CustomTimeDilation으로 해당 캐릭터만 슬로우 모션
 * - PostProcess로 화면 연출 (비네팅, 채도 감소)
 */
UCLASS(ClassGroup = (Effects), meta = (BlueprintSpawnableComponent))
class GAS_API UGS_DeathCinematicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_DeathCinematicComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ===== 슬로우 모션 설정 =====

	/** 슬로우 모션 지속 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "DeathCinematic|SlowMotion", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float SlowMotionDuration = 2.0f;

	/** Time Dilation 비율 (0.1 = 10% 속도) */
	UPROPERTY(EditDefaultsOnly, Category = "DeathCinematic|SlowMotion", meta = (ClampMin = "0.05", ClampMax = "0.5"))
	float TimeDilationFactor = 0.15f;

	// ===== 화면 효과 설정 =====

	/** PostProcess 효과 강도 (최대치) */
	UPROPERTY(EditDefaultsOnly, Category = "DeathCinematic|Visual", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxEffectStrength = 0.8f;

	/** 효과 페이드 인 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "DeathCinematic|Visual", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float FadeInDuration = 0.1f;

	/** 효과 페이드 아웃 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "DeathCinematic|Visual", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float FadeOutDuration = 0.8f;

	// ===== 머티리얼 파라미터 =====

	UPROPERTY(EditDefaultsOnly, Category = "DeathCinematic|Material")
	FName StrengthParamName = TEXT("Strength");

	UPROPERTY(EditDefaultsOnly, Category = "DeathCinematic|Material")
	FName DesaturationParamName = TEXT("Desaturation");

	// ===== 카메라 회전 설정 =====

	/** 카메라 회전 활성화 여부 */
	UPROPERTY(EditDefaultsOnly, Category = "DeathCinematic|Camera")
	bool bEnableCameraRotation = true;

	/** 카메라 회전 각도 (Yaw) - 180도면 앞면을 보여줌 */
	UPROPERTY(EditDefaultsOnly, Category = "DeathCinematic|Camera", meta = (EditCondition = "bEnableCameraRotation"))
	float CameraRotationAngle = 180.0f;

	/** 카메라 Pitch 기울기 - 음수면 아래를 내려다봄 (쓰러진 캐릭터 시점) */
	UPROPERTY(EditDefaultsOnly, Category = "DeathCinematic|Camera", meta = (EditCondition = "bEnableCameraRotation", ClampMin = "-45.0", ClampMax = "45.0"))
	float CameraPitchAngle = -25.0f;

	/** 카메라 회전에 걸리는 시간 (실제 시간 기준) */
	UPROPERTY(EditDefaultsOnly, Category = "DeathCinematic|Camera", meta = (EditCondition = "bEnableCameraRotation", ClampMin = "0.1", ClampMax = "3.0"))
	float CameraRotationDuration = 1.5f;

	// ===== 공개 함수 =====

	/** 초기화 (소유자 및 PostProcess 컴포넌트 설정) */
	void InitializeForOwner(AActor* InOwner, UPostProcessComponent* InPostProcessComp,
	                        UMaterialInterface* InMaterialOverride = nullptr,
	                        USpringArmComponent* InSpringArm = nullptr);

	/** 죽음 연출 재생 (로컬 플레이어 체크는 호출부에서 수행) */
	UFUNCTION(BlueprintCallable, Category = "DeathCinematic")
	void PlayDeathCinematic();

	/** 연출 즉시 중단 */
	UFUNCTION(BlueprintCallable, Category = "DeathCinematic")
	void StopCinematic();

	/** 현재 연출 중인지 확인 */
	UFUNCTION(BlueprintPure, Category = "DeathCinematic")
	bool IsPlaying() const
	{
		return bIsPlaying;
	}

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 동적 머티리얼 인스턴스 생성 보장 */
	void EnsureMID();

	/** 틱 업데이트 */
	void TickUpdate(float DeltaTime);

	/** 효과 강도 적용 */
	void ApplyEffect(float Strength);

	/** 카메라 회전 시작 */
	void StartCameraRotation();

	/** 카메라 회전 업데이트 */
	void UpdateCameraRotation(float RealDeltaTime);

private:
	/** 캐싱된 소유자 캐릭터 (매 프레임 Cast 방지) */
	TWeakObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY()
	TWeakObjectPtr<UPostProcessComponent> ManagedPostProcessComp;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> EffectMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	bool bIsPlaying = false;
	float CurrentStrength = 0.0f;
	float TargetStrength = 0.0f;
	float ElapsedTime = 0.0f;

	/** 초기화 시점에 저장한 로컬 플레이어 여부 (죽을 때 컨트롤러가 분리되므로 미리 저장) */
	bool bIsOwnedByLocalPlayer = false;

	// ===== 카메라 회전 관련 =====

	/** 관리하는 SpringArm 컴포넌트 */
	UPROPERTY()
	TWeakObjectPtr<USpringArmComponent> ManagedSpringArm;

	/** 원래 SpringArm 전체 회전 값 (Pitch/Roll 유지용) */
	FRotator OriginalSpringArmRotation = FRotator::ZeroRotator;

	/** 원래 SpringArm Yaw 값 */
	float OriginalSpringArmYaw = 0.0f;

	/** 목표 SpringArm Yaw 값 */
	float TargetSpringArmYaw = 0.0f;

	/** 카메라 회전 경과 시간 */
	float CameraRotationElapsed = 0.0f;

	/** 카메라 회전 중인지 여부 */
	bool bIsCameraRotating = false;

	/** 원래 bUsePawnControlRotation 값 (복구용) */
	bool bOriginalUsePawnControlRotation = true;

	/** 원래 ViewTarget (복원용) */
	TWeakObjectPtr<AActor> OriginalViewTarget;
};
