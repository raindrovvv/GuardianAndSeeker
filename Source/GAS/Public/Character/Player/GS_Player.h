#pragma once

#include "CoreMinimal.h"
#include "Character/GS_Character.h"
#include "Components/TimelineComponent.h"
#include "AkComponent.h"
#include "GS_Player.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UGS_SteamNameWidgetComp;
class FAkAudioDevice;
class UInputAction;

USTRUCT(BlueprintType)
struct FSkillInputControl
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	bool CanInputLC = true; // Left Click
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	bool CanInputRC = true; // Right Click
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	bool CanInputRoll = true; // SpaceBar;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	bool CanInputCtrl = true; // Ctrl
};

UCLASS()
class GAS_API AGS_Player : public AGS_Character
{
	GENERATED_BODY()

public:
	AGS_Player(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// component
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Components")
	TObjectPtr<USpringArmComponent> SpringArmComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Components")
	TObjectPtr<UCameraComponent> CameraComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UGS_SteamNameWidgetComp> SteamNameWidgetComp;

	// 시야방해
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Components", meta = (AllowPrivateAccess = "true"))
	class UPostProcessComponent* PostProcessComponent;

	UPROPERTY()
	UMaterialInstanceDynamic* BlurMID;

	UPROPERTY(EditAnywhere, Category = "Vision")
	UMaterialInterface* PostProcessMat;

	UPROPERTY()
	UCurveFloat* ObscureCurve; // 외부에서 세팅할 수 있음

	// variable
	UPROPERTY()
	float WalkSpeed;

	UPROPERTY()
	float RunSpeed;

	// 오디오 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	UAkComponent* AkComponent;

	// 카메라 위치 오디오 리스너 컴포넌트 (TPS 표준)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	UAkComponent* CameraAudioListenerComponent;

	UFUNCTION(Client, Reliable)
	void Client_StartVisionObscured();

	void StartVisionObscured();

	UFUNCTION(Client, Reliable)
	void Client_StopVisionObscured();

	void StopVisionObscured();

	UFUNCTION()
	void HandleTimelineProgress(float Value);

	UFUNCTION()
	void OnTimelineFinished();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetUseControllerRotationYaw(bool UseControlRotationYaw);

	// 사운드 재생 함수들
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlaySound(UAkAudioEvent* SoundEvent);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlaySoundWithCallback(UAkAudioEvent* SoundEvent, const FOnAkPostEventCallback& Callback);

	// 오디오 관련 함수들
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetupLocalAudioListener();

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetupCameraAudioListener();

	UFUNCTION(BlueprintCallable, Category = "Audio")
	bool IsLocalPlayer() const;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlaySkillMontage(UAnimMontage* Montage, FName Section = NAME_None, int32 PlayRate = 1.0f);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StopSkillMontage(UAnimMontage* Montage);

	virtual void OnDeath() override;

	// Collision Set
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse);

	// Skll Input Control
	void SetSkillInputControl(bool CanLeftClick, bool CanRightClick, bool CanRollClick, bool CanCtrlClick = true);
	FSkillInputControl GetSkillInputControl();

	FORCEINLINE UGS_SkillComp* GetSkillComp() const { return SkillComp; }
	virtual void SetCanUseSkill(bool bCanUse) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ================
	// 시점 전환 시스템 (F5)
	// ================

	/** F5 키 입력 시 호출: 1인칭/3인칭 시점 전환 토글 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void TogglePerspective();

	/** 현재 1인칭 시점인지 확인 */
	UFUNCTION(BlueprintPure, Category = "Camera")
	bool IsFirstPerson() const { return bIsFirstPerson; }

	/** 서버에 현재 시점 상태를 동기화 (회전 로직 연동용) */
	UFUNCTION(Server, Reliable)
	void Server_SetPerspectiveState(bool bFirstPerson);

	/** 시점 전환 Input Action (Enhanced Input) */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_TogglePerspective;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void BeginDestroy() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGS_SkillComp> SkillComp;

	void UpdateSteamNameWidgetRotation();

	/** 플레이어 타입에 따른 최적 컬링 거리 반환 (자식 클래스에서 오버라이드) */
	virtual float GetOptimalCullDistance() const;

	/** Significance Manager: 중요도 계산 콜백 */
	virtual float CalculateSignificance(const FTransform& Viewpoint) override;

	// ================
	// 시점 전환 시스템 (protected)
	// ================

	/** 현재 1인칭 모드인지 여부 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bIsFirstPerson = false;

	/** 시점 전환 보간 진행 중 여부 */
	bool bIsPerspectiveTransitioning = false;

	/** TPS 모드 시 저장된 SpringArm 길이 */
	UPROPERTY()
	float SavedTPSArmLength = 400.f;

	/** TPS 모드 시 저장된 SpringArm Socket Offset */
	UPROPERTY()
	FVector SavedTPSSocketOffset = FVector::ZeroVector;

	/** TPS 모드 시 저장된 FOV */
	UPROPERTY()
	float SavedTPSFOV = 90.f;

	/** TPS 모드 시 저장된 컨트롤러 회전 사용 여부 */
	UPROPERTY()
	bool bSavedUseControllerRotationYaw = false;

	/** 1인칭 모드 시 SpringArm 길이 (캐릭터 눈 위치) */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float FirstPersonArmLength = 0.f;

	/** 1인칭 모드 시 눈 높이 오프셋 (캐릭터 머리 위치) */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	FVector FirstPersonEyeOffset = FVector(50.f, 0.f, -50.f);

	/** 1인칭 모드 시 FOV (좁을수록 확대됨) */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "60.0", ClampMax = "120.0"))
	float FirstPersonFOV = 75.f;

	/** 1인칭 모드 시 Near Clip Plane (작을수록 가까운 오브젝트도 렌더링) */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float FirstPersonNearClipPlane = 5.f;

	/** TPS 모드 시 저장된 Near Clip Plane */
	UPROPERTY()
	float SavedNearClipPlane = 10.f;

	/** 시점 전환 보간 속도 */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float PerspectiveTransitionSpeed = 10.f;

	/** 시점 전환 보간 처리 (Tick에서 호출) */
	void UpdatePerspectiveTransition(float DeltaTime);

	/** 모든 메시의 머티리얼 디더링 파라미터 업데이트 */
	void UpdateCharacterDither(bool bEnabled);

	/** 머티리얼 디더링 거리 파라미터 이름 */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	FName DitherParamName = FName("DitherDistance");

	/** 3인칭 기본 디더링 거리 */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float DefaultDitherDistance = 220.0f;

private:
	// Input Control Flag
	UPROPERTY(Replicated)
	FSkillInputControl SkillInputControl;

	FTimeline ObscureTimeline;

	bool bIsObscuring;


	// 오디오 디바이스 캐싱
	FAkAudioDevice* CachedAudioDevice = nullptr;
};
