#pragma once

#include "CoreMinimal.h"
#include "Sound/GS_AudioComponentBase.h"
#include "AkAudioEvent.h"
#include "GS_DrakharAudioComponent.generated.h"

class UAkAudioEvent;
class AGS_Drakhar;

/**
 * 드라카르(가디언) 전용 오디오 컴포넌트
 * GS_AudioComponentBase를 상속받아 공통 기능 활용
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent), HideCategories = ("BaseAudioComponent"))
class GAS_API UGS_DrakharAudioComponent : public UGS_AudioComponentBase
{
	GENERATED_BODY()

public:
	UGS_DrakharAudioComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 드라카르 에셋 프리로딩 */
	void PreloadDrakharAssets();

public:
	// === Wwise 사운드 재생 함수 ===
	void PlayComboAttackSound();
	void PlayDashSkillSound();
	void PlayEarthquakeSkillSound();
	void PlayDraconicFurySkillSound();
	void PlayDraconicProjectileSound(const FVector& Location);
	void PlayAttackHitSound();
	void PlayFeverModeStartSound(bool bForcePlay = true); // 피버모드는 중요하므로 기본적으로 강제 재생
	void PlayFeverModeEndSound();
	void PlayFeverModeStateSound(bool bForcePlay = true); // 피버모드는 중요하므로 기본적으로 강제 재생
	void StopFeverModeStateSound();
	void PlayComboFinisherSound();
	void PlayLandingSound();

	// === 로컬 전용 사운드 재생 (RPC 없음 - RepNotify에서 호출) ===
	void PlayFeverModeStartSoundLocal();
	void PlayFeverModeEndSoundLocal();
	void PlayFeverModeStateSoundLocal();
	void StopFeverModeStateSoundLocal();
	void PlayHurtSoundLocal();
	void PlayDraconicProjectileImpactSoundLocal(const FVector& ImpactLocation, bool bHitCharacter);

	// === Getter 함수 ===
	FORCEINLINE int32& GetFeverModeStateSoundPlayingID() { return FeverModeStateSoundPlayingID; }
	FORCEINLINE int32 GetFeverModeStateFadeOutDuration() const { return FeverModeStateFadeOutDuration; }

private:
	// === 멀티캐스트 RPC 함수 ===
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayComboAttackSound();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDashSkillSound();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayEarthquakeSkillSound();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDraconicFurySkillSound();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDraconicProjectileSound(const FVector& Location);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayAttackHitSound();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFeverModeStartSound();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFeverModeEndSound();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFeverModeStateSound();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_StopFeverModeStateSound();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayComboFinisherSound();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayLandingSound();

	UPROPERTY()
	TObjectPtr<AGS_Drakhar> OwnerDrakhar;

	// 사운드 중복 재생 방지
	bool bDraconicFurySoundPlayed;
	bool bHurtSoundPlayed;
	bool bDashSkillSoundPlayed;

	// 피버모드 스테이트 사운드 Playing ID 저장
	int32 FeverModeStateSoundPlayingID;

	// 쿨다운 값
	UPROPERTY(EditDefaultsOnly, Category = "Audio|Cooldown", meta = (ClampMin = "0.1"))
	float DraconicFurySoundCooldown = 7.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Audio|Cooldown", meta = (ClampMin = "0.1"))
	float HurtSoundCooldown = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Audio|Cooldown", meta = (ClampMin = "0.1"))
	float DashSkillSoundCooldown = 1.5f;

	// 페이드아웃 시간 (ms)
	UPROPERTY(EditDefaultsOnly, Category = "Audio|FadeOut", meta = (ClampMin = "0", ClampMax = "5000"))
	int32 FeverModeStateFadeOutDuration = 500;

	// === 타이머 핸들 ===
	FTimerHandle DraconicFurySoundCooldownTimer;
	FTimerHandle HurtSoundCooldownTimer;
	FTimerHandle DashSkillSoundCooldownTimer;

	// === 타이머 콜백 함수 ===
	UFUNCTION()
	void ResetDraconicFurySoundCooldown();

	UFUNCTION()
	void ResetHurtSoundCooldown();

	UFUNCTION()
	void ResetDashSkillSoundCooldown();

	// === Wwise 관련 헬퍼 함수 ===
	void PlaySoundEvent(UAkAudioEvent* SoundEvent, const FVector& Location = FVector::ZeroVector);

protected:
	// 프리로드된 오디오 캐시
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedComboAttackSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedDashSkillSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedEarthquakeSkillSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedDraconicFurySkillSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedDraconicProjectileSound;
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedDraconicProjectileImpactSound;
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedDraconicProjectileExplosionSound;
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedAttackHitSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedComboFinisherSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedFeverModeStartSound;
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedFeverModeEndSound;
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedFeverModeStateSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedHurtSound;

	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedLandingSound;
};