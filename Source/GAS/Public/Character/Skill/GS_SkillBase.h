#pragma once

#include "CoreMinimal.h"
#include "GS_SkillComp.h"
#include "NiagaraSystem.h"
#include "Animation/AnimMontage.h"
#include "GS_SkillBase.generated.h"

class AGS_Player;
class UGS_SkillComp;

UCLASS()
class GAS_API UGS_SkillBase : public UObject
{
	GENERATED_BODY()

public:
	ESkillSlot CurrentSkillType;

	float Cooltime;
	float Damage;

	UPROPERTY(EditDefaultsOnly)
	TArray<TSoftObjectPtr<UAnimMontage>> SkillAnimMontages;

	UPROPERTY(EditDefaultsOnly)
	TSoftObjectPtr<UTexture2D> SkillImage;

	UPROPERTY(EditDefaultsOnly)
	int16 AllowSkillsMask;

	UPROPERTY(EditDefaultsOnly)
	FControlValue AllowControlValue;


	// VFX 관련 속성들 (데이터 테이블에서 설정됨) - Soft Reference로 메모리 최적화
	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> SkillCastVFX;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> SkillRangeVFX;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> SkillImpactVFX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> SkillEnvImpactVFX;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> SkillEndVFX;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> SkillLoopVFX;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	FVector SkillVFXScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	float SkillVFXDuration = 3.0f;

	// VFX 위치 오프셋
	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	FVector CastVFXOffset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	FVector RangeVFXOffset = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX")
	FVector ImpactVFXOffset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX")
	FVector EnvImpactVFXOffset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX")
	FVector EndVFXOffset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	FVector LoopVFXOffset = FVector::ZeroVector;

	UTexture2D* GetSkillImage();

	// 쿨타임 관리
	float GetCoolTime();

	// 스킬 초기화
	void InitSkill(AGS_Player* InOwner, UGS_SkillComp* InOwningComp, ESkillSlot InSlot);

	// 로컬 VFX 재생 함수들
	void PlayCastVFX(FVector Location, FRotator Rotation);
	void PlayRangeVFX(FVector Location, float Radius);
	void PlayImpactVFX(FVector Location); // 월드 위치에 생성
	void PlayImpactVFXOnTarget(AActor* Target); // 타겟에 부착
	// 환경(벽, 바닥) 충돌 VFX 재생
	void PlayEnvImpactVFX(FVector Location, FRotator Rotation);
	void PlayEndVFX(FVector Location, FRotator Rotation);

	// Cast VFX 정리 함수
	void StopCastVFX();
	void Internal_StopCastVFX();

	// VFX + 몽타주 에셋 프리로드 (InitSkill에서 호출)
	void PreloadSkillAssets();

	// 캐시된 몽타주 가져오기 (없으면 LoadSynchronous 폴백)
	UAnimMontage* GetCachedMontage(int32 Index);

	// 스킬 작동
	virtual void ActiveSkill(); // 스킬 시작(서버 권한에서만 호출)
	virtual void OnSkillCanceledByDebuff(); // 스킬 중도 종료(Mute 디버프)
	virtual void OnSkillAnimationEnd(); // 애니메이션 종료 시 호출
	virtual void ExecuteSkillEffect();
	virtual void DeactiveSkill(); // 스킬 종료
	virtual void OnSkillCommand(); // 보조 스킬 발동
	virtual bool CanActive() const; // 사용 가능한지 반환
	virtual bool GetIsActive() const; // 사용중인지 반환
	virtual void InterruptSkill(); // 다른 스킬 사용으로 인한 스킬 중단
	virtual void SetIsActive(bool bInIsActive);

	// 쿨타임
	void SetCoolingDown(bool bInCoolingDown) { bIsCoolingDown = bInCoolingDown; }

	// Delegate Binding 함수
	virtual void InitializeDelegate();

	// 데이터 테이블에서 현재 스킬 정보 가져오기
	const FSkillInfo* GetCurrentSkillInfo() const;

protected:
	bool bIsActive = false;
	bool bIsCoolingDown;

	// 스킬 소유자
	UPROPERTY()
	TObjectPtr<AGS_Player> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<UGS_SkillComp> OwningComp;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> ActiveCastVFXComponent;

	// 프리로드된 VFX 캐시 (메모리 최적화를 위한 로드된 에셋 저장)
	UPROPERTY()
	TObjectPtr<UNiagaraSystem> CachedCastVFX;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> CachedRangeVFX;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> CachedImpactVFX;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> CachedEnvImpactVFX;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> CachedEndVFX;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> CachedLoopVFX;

	// 프리로드된 오디오 캐시
	// (명시적 캐싱 변수를 두어 메모리에서 해제되지 않도록 보장)
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedSkillStartSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedSkillEndSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedSkillLoopSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedSkillLoopStopSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedRTSSkillStartSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedRTSSkillEndSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedRTSSkillLoopSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedRTSSkillLoopStopSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedWallCollisionSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedMonsterCollisionSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedGuardianCollisionSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedRTSWallCollisionSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedRTSMonsterCollisionSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedRTSGuardianCollisionSound;

	// 프리로드된 애니메이션 몽타주 캐시 (메모리 최적화를 위한 로드된 에셋 저장)
	UPROPERTY()
	TArray<TObjectPtr<UAnimMontage>> CachedAnimMontages;

	void StartCoolDown();

	// 스킬 오디오 재생 헬퍼 함수들
	void PlaySkillStartSound() const;
	void PlaySkillEndSound() const;
	virtual void BeginDestroy() override;
};
