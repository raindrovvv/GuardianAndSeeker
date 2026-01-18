#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "GameFramework/Character.h"
#include "Character/E_Character.h"
#include "Component/GS_HitReactComp.h"
#include "CharacterDataAsset.h"
#include "Character/Component/GS_CameraShakeTypes.h"
#include "System/Utility/GS_AssetLoader.h"
#include "GS_Character.generated.h"

class AGS_Character;
enum class EKillFeedbackType : uint8;
enum class EPositiveEffectType : uint8;

class UGS_StatComp;
class UGS_SkillComp;
class UGS_DebuffComp;
class UGS_HitReactComp;
class UGS_CameraShakeComponent;
class UGS_AudioMixingComponent;
class UGS_HPTextWidgetComp;
class UGS_PlayerInfoWidget;
class UGS_HPText;
class UGS_HPWidget;
class AGS_Weapon;
class UDecalComponent;
class UNiagaraSystem;
class UGS_DamageNumberComponent;

USTRUCT(BlueprintType)
struct FImpactVFXInfo
{
	GENERATED_BODY()

	UPROPERTY()
	TSoftObjectPtr<UNiagaraSystem> VFXAsset = nullptr;

	UPROPERTY()
	FVector Scale = FVector::OneVector;

	UPROPERTY()
	uint8 Counter = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterDeath);

/** 데미지 기록 (어시스트용) */
USTRUCT()
struct FDamageRecord
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AGS_Character> Damager;

	UPROPERTY()
	float DamageAmount = 0.0f;

	UPROPERTY()
	float LastDamageTime = 0.0f;
};

/** 서포트 기록 (힐, 버프 어시스트용) */
USTRUCT()
struct FSupportRecord
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AGS_Character> Supporter;

	UPROPERTY()
	float SupportWeight = 0.0f;

	UPROPERTY()
	float LastSupportTime = 0.0f;
};

USTRUCT(BlueprintType)
struct FWeaponSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AGS_Weapon> WeaponClass = nullptr;

	UPROPERTY()
	AGS_Weapon* WeaponInstance = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName SocketName = NAME_None;
};


UCLASS()
class GAS_API AGS_Character : public ACharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	static constexpr float SLOW_DEBUFF_SPEED_THRESHOLD = 0.4f;

	AGS_Character(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginDestroy() override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	virtual void OnDamageStart();

	// HitReact
	bool CanHitReact = true;
	FTimerHandle HitReactTimerHandle;

	void DisableHitReact(float CooldownTime);
	void DisableHitReact(bool bAllowHitReact);

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 팀 ID (0: 중립, 1: 플레이어, 2: 몬스터) */
	UPROPERTY(EditAnywhere, Category = "Team")
	FGenericTeamId TeamId;

	// 죽음 사운드는 각 캐릭터 타입별 오디오 컴포넌트에서 처리됨
	// 시커: GS_SeekerAudioComponent, 가디언: GS_GuardianAudioComponent, 몬스터: GS_MonsterAudioComponent

	//variable
	float MaxSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	ECharacterType CharacterType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGS_HitReactComp> HitReactComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGS_CameraShakeComponent> CameraShakeComp;

	// EditAnywhere, BlueprintReadOnly, Category = "Effects|CameraShake"
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|CameraShake")
	FGS_CameraShakeInfo TakeDamageShake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|CameraShake")
	FGS_CameraShakeInfo AttackSuccessShake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|CameraShake")
	FGS_CameraShakeInfo LightDamageShake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|CameraShake")
	FGS_CameraShakeInfo NormalDamageShake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|CameraShake")
	FGS_CameraShakeInfo HeavyDamageShake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|CameraShake")
	float LightDamageThreshold = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|CameraShake")
	float NormalDamageThreshold = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|CameraShake")
	float HeavyDamageThreshold = 45.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Effects|CameraShake")
	float CameraKnockbackDistance = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Effects|CameraShake")
	float CameraKnockbackRecoverySpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Stat", meta = (AllowPrivateAccess))
	TObjectPtr<UGS_HPTextWidgetComp> HPTextWidgetComp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	UCharacterDataAsset* CharacterData;

	UFUNCTION(BlueprintCallable, Category = "Data")
	UTexture2D* GetPortrait() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "State", meta = (AllowPrivateAccess), Replicated)
	bool bLockRotationToController = false; // Idle 상태에도 bUseControllerRotationYaw 를 true 로 두기 위한 flag.

	UFUNCTION(BlueprintCallable, Category = "State")
	bool GetIsLockedRotationToController();

	UFUNCTION(BlueprintCallable, Category = "State")
	void SetIsLockedRotationToController(bool InputIsRotationRoController);

	UFUNCTION(BlueprintCallable, Category = "Data")
	FText GetMonsterName() const
	{
		return CharacterData ? CharacterData->CharacterName : FText::GetEmpty();
	}

	UFUNCTION(BlueprintCallable, Category = "Data")
	FText GetDescription() const
	{
		return CharacterData ? CharacterData->Description : FText::GetEmpty();
	}

	UFUNCTION(BlueprintCallable, Category = "Data")
	FText GetTypeName() const
	{
		return CharacterData ? CharacterData->TypeName : FText::GetEmpty();
	}

	//getter
	FORCEINLINE UGS_StatComp* GetStatComp() const { return StatComp; }
	FORCEINLINE UGS_DebuffComp* GetDebuffComp() const { return DebuffComp; }
	FORCEINLINE ECharacterType GetCharacterType() const { return CharacterType; }
	FORCEINLINE UGS_DamageNumberComponent* GetDamageNumberComponent() const { return DamageNumberComp; }

	//serverRPC
	/** 타격 정격(Hit-stop) 효과 적용 (멀티캐스트) */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ApplyHitStop(float Duration, float TimeDilation, bool bPlayShake = false);
	UFUNCTION(Server, Reliable)
	void ServerRPCMeleeAttack(AGS_Character* InDamagedCharacter);

	//clientRPC for camera shake
	UFUNCTION(Client, Unreliable)
	void Client_PlayTakeDamageShake(APlayerController* TargetPC, const FGS_CameraShakeInfo& ShakeInfo, float KnockbackMultiplier = 1.0f);

	UFUNCTION(Client, Unreliable)
	void Client_PlayAttackSuccessShake(APlayerController* TargetPC);

	UFUNCTION(Client, Unreliable)
	void Client_PlayAttackSuccessShakeWithInfo(APlayerController* TargetPC, const FGS_CameraShakeInfo& CustomShakeInfo);

	//character death play ragdoll
	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPCCharacterDeath();

	UFUNCTION()
	void WatchOtherPlayer();

	UFUNCTION()
	virtual void OnDeath();
	UFUNCTION()
	void DestroyAllWeapons();

	//HP widget
	void SetHPTextWidget(UGS_HPText* InHPTextWidget);
	void SetHPBarWidget(UGS_HPWidget* InHPBarWidget);
	void SetPlayerInfoWidget(UGS_PlayerInfoWidget* InPlayerInfoWidget);
	virtual FGenericTeamId GetGenericTeamId() const override;

	UFUNCTION(BlueprintPure, Category = "Team")
	bool IsEnemy(const AGS_Character* Other) const;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPCPlaySkillMontage(UAnimMontage* SkillMontage);

	UFUNCTION(NetMulticast, Unreliable)
	void MulicastRPCStopCurrentSkillMontage(UAnimMontage* CurrentSkillMontage);
	// Impact VFX 재생 (내부적으로 OnRep을 통해 동기화)
	UFUNCTION(BlueprintCallable, Category = "Effects")
	void PlayImpactVFX(UNiagaraSystem* VFXAsset, FVector Scale = FVector(1.0f, 1.0f, 1.0f));

	UFUNCTION(BlueprintCallable)
	AGS_Weapon* GetWeaponByIndex(int32 Index) const;

	UFUNCTION(BlueprintCallable)
	AGS_Weapon* GetWeaponBySocketName(FName SocketName);

	UFUNCTION(Server, Reliable)
	void Server_SetCharacterSpeed(float InRatio);

	UFUNCTION(BlueprintCallable)
	virtual void SetCanUseSkill(bool bCanUse) {}

	UFUNCTION()
	void SetCharacterSpeed(float InRatio);

	bool IsDead() const;

	UPROPERTY(BlueprintAssignable)
	FOnCharacterDeath OnDeathDelegate;

	// HitReact
	UFUNCTION(Server, Reliable)
	void Server_SetCanHitReact(bool bCanReact);

	UFUNCTION()
	void SetCanHitReact(bool bCanReact);

	UFUNCTION(BlueprintCallable, Category = "State")
	bool IsInvincible() const { return bIsInvincible; }

	/** 이 캐릭터가 처치되었을 때의 피드백 타입 반환 */
	virtual EKillFeedbackType GetKillFeedbackType() const;

	/** UI에 표시할 캐릭터 이름 반환 */
	virtual FString GetCharacterName() const;

	/** 힐을 받았을 때 호출 (어시스트 추적용) */
	void NotifyHealed(AGS_Character* Healer, float Amount);

	/** 버프를 받았을 때 호출 (어시스트 추적용) */
	void NotifyBuffed(AGS_Character* Buffer, EPositiveEffectType BuffType);

	/** 서포트 기록 조회 (읽기 전용) */
	const TArray<FSupportRecord>& GetSupportHistory() const { return SupportHistory; }

	/** 가드/방어 중인지 여부 (자식 클래스에서 오버라이드) */
	UFUNCTION(BlueprintCallable, Category = "State")
	virtual bool IsDefending() const { return false; }

	/** 
	 * 공격 성공 시 추가적인 특수 효과(VFX, 사운드 등)를 처리합니다.
	 * @param ComboIndex 현재 콤보 인덱스
	 * @param HitResult 타격 정보
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void OnAttackHitSuccess(int32 ComboIndex, const FHitResult& HitResult) {}

	void SetInvincible(bool bEnable);

	// VFX 거리 기반 컬링 (성능 최적화)
	// @param Location VFX를 재생할 월드 위치
	// @param MaxDistance 최대 재생 거리 (cm, 기본값 4000cm)
	// @return VFX를 재생해야 하면 true, 아니면 false
	bool ShouldPlayVFXAtLocation(const FVector& Location, float MaxDistance = 4000.0f) const;

	/** Significance Manager: 중요도 계산 (거리, 시점, 로컬 여부 등 고려) */
	virtual float CalculateSignificance(const FTransform& Viewpoint);

	/** Significance Manager: 중요도 변경에 따른 자원(애니메이션, 틱 등) 조절 */
	virtual void OnSignificanceChanged(float NewSignificance);

	/** 현재 중요도 값 반환 */
	FORCEINLINE float GetSignificance() const { return CurrentSignificance; }

	/** 캐릭터의 타격 재질 타입 (사운드 레이어 분기용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Sound")
	EImpactMaterialType ImpactMaterialType = EImpactMaterialType::Flesh;

	virtual EImpactMaterialType GetImpactMaterialType() const { return ImpactMaterialType; }

	/** 현재 누적된 카메라 낙아웃 거리 */
	float CurrentCameraKnockback = 0.0f;

	/** 카메라 낙아웃 효과 적용 */
	void ApplyCameraKnockback(float IntensityMultiplier = 1.0f);

	/** 모든 무기가 공유하여 사용할 사운드 믹싱(Ducking) 타이머 핸들 */
	FORCEINLINE FTimerHandle& GetAudioFocusTimerHandle() { return AudioFocusTimerHandle; }

protected:
	/** 모든 무기가 공유하여 사용할 사운드 믹싱(Ducking) 타이머 핸들 */
	FTimerHandle AudioFocusTimerHandle;

	/** 현재 중요도 상태 저장 (0.0 ~ 1.0) */
	float CurrentSignificance = 1.0f;

protected:
	/** Significance Manager 등록 로직 (가상 함수로 분리하여 중복 등록 방지) */
	virtual void RegisterSignificanceManager();

	/** 그림자 컬링 최적화 (거리 기반) - 모든 캐릭터 공통 */
	void UpdateShadowCulling();

protected:
	virtual void NotifyActorBeginCursorOver() override;
	virtual void NotifyActorEndCursorOver() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGS_DebuffComp> DebuffComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGS_StatComp> StatComp;

	/** 데미지 숫자 팝업 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGS_DamageNumberComponent> DamageNumberComp;

	/** 공통 오디오 컴포넌트 변수 (내부 로직용으로만 사용, 에디터 노출은 자식 클래스에서 타입별로 수행) */
	UPROPERTY()
	TObjectPtr<class UGS_AudioComponentBase> BaseAudioComponent;

	/** 발소리, 피격음 등 레이어드 사운드 제어를 위한 믹싱 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<class UGS_AudioMixingComponent> AudioMixingComponent;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TArray<FWeaponSlot> WeaponSlots;

	UPROPERTY(Replicated)
	EWeaponHandlingState WeaponHandlingState = EWeaponHandlingState::Wielding;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RTS")
	TObjectPtr<UDecalComponent> SelectionDecal;

	bool bIsHovered;

	virtual FLinearColor GetCurrentDecalColor();
	virtual void UpdateDecal();
	virtual bool ShowDecal();
	void ShowDecalWithColor(const FLinearColor& Color);
	virtual void OnHoverBegin();
	virtual void OnHoverEnd();

public:
	UFUNCTION()
	EWeaponHandlingState GetWeaponHandlingState();
	UFUNCTION()
	void SetWeaponHandlingState(EWeaponHandlingState InputWeaponHandlingState);

private:
	UPROPERTY(ReplicatedUsing = OnRep_CharacterSpeed)
	float CharacterSpeed;
	float DefaultCharacterSpeed;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead;

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicDecalMaterial;

	UFUNCTION()
	void OnRep_CharacterSpeed();

protected:
	UFUNCTION()
	virtual void OnRep_IsDead();

	/** Death 사운드 로컬 재생 (OnDeath, OnRep_IsDead에서 공통 사용) */
	virtual void PlayDeathSoundLocal();

	UFUNCTION()
	void OnRep_ImpactVFX();

private:
	UPROPERTY(ReplicatedUsing = OnRep_ImpactVFX)
	FImpactVFXInfo RepImpactVFX;

	// 중복 생성 방지를 위한 비동기 로드 핸들
	FAsyncLoadHandle PendingImpactVFXLoad;

private:
	void SpawnAndAttachWeapons();

	void SetHovered(bool bHovered);

	// 무적 상태
	UPROPERTY(Replicated)
	bool bIsInvincible = false;

	/** 데미지 기여자 추적 (어시스트 계산용) */
	TArray<FDamageRecord> DamageHistory;

	/** 서포트 기여자 추적 (힐/버프 어시스트용) */
	TArray<FSupportRecord> SupportHistory;

	/** 어시스트 유효 시간 (초) */
	float AssistWindowSeconds = 10.0f;

	/** 어시스트 인정을 위한 최소 데미지 비율 (최대 체력 대비) */
	float AssistThresholdRatio = 0.1f;
};
