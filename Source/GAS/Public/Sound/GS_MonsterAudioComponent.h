#pragma once

#include "CoreMinimal.h"
#include "Sound/GS_AudioComponentBase.h"
#include "AkGameplayStatics.h"
#include "AkComponent.h"
#include "Engine/TimerHandle.h"
#include "Net/UnrealNetwork.h"
#include "AkAudioEvent.h"
#include "GS_MonsterAudioComponent.generated.h"

class AGS_Monster;
class AGS_Seeker;
class AGS_RTSController;

/**
 * 몬스터의 거리별 사운드를 관리하는 컴포넌트
 * RTS와 TPS 모드 모두에서 작동하는 몬스터 울음소리 시스템
 */
UENUM(BlueprintType)
enum class EMonsterAudioState : uint8
{
	Idle UMETA(DisplayName = "평상시"),
	Combat UMETA(DisplayName = "전투"),
	Hurt UMETA(DisplayName = "피해받음"),
	Death UMETA(DisplayName = "죽음")
};

USTRUCT(BlueprintType)
struct FMonsterAudioConfig
{
	GENERATED_BODY()

	// 사운드 이벤트들
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Events")
	TSoftObjectPtr<UAkAudioEvent> IdleSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Events")
	TSoftObjectPtr<UAkAudioEvent> CombatSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Events")
	TSoftObjectPtr<UAkAudioEvent> HurtSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Events")
	TSoftObjectPtr<UAkAudioEvent> SwingSound;

	// RTS 커맨드 사운드 (RTS 전용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Events|RTS Command", meta = (DisplayName = "Selection Click Sound"))
	TSoftObjectPtr<UAkAudioEvent> SelectionClickSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Events|RTS Command", meta = (DisplayName = "RTS Move Command Sound"))
	TSoftObjectPtr<UAkAudioEvent> RTSMoveCommandSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Events|RTS Command", meta = (DisplayName = "RTS Attack Command Sound"))
	TSoftObjectPtr<UAkAudioEvent> RTSAttackCommandSound;

	// 게임 로직용 거리 설정 (Wwise Attenuation과 별개)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Logic", meta = (ClampMin = "0.0"))
	float AlertDistance = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "0.0"))
	float MaxAudioDistance = 1500.0f;

	FMonsterAudioConfig()
	{
		IdleSound = nullptr;
		CombatSound = nullptr;
		HurtSound = nullptr;
		SwingSound = nullptr;
		SelectionClickSound = nullptr;
		RTSMoveCommandSound = nullptr;
		RTSAttackCommandSound = nullptr;
	}
};

UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent), HideCategories = ("BaseAudioComponent"))
class GAS_API UGS_MonsterAudioComponent : public UGS_AudioComponentBase
{
	GENERATED_BODY()

public:
	UGS_MonsterAudioComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void InitializeAudioRTPCs() override;

public:
	// ==========
	// 사운드 설정 (에디터에서 설정)
	// ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Audio|Core Settings", meta = (DisplayName = "Audio Configuration"))
	FMonsterAudioConfig AudioConfig;

	// 사운드 재생 간격 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Audio|TPS", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float IdleSoundInterval = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Audio|TPS", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float CombatSoundInterval = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Audio|Swing", meta = (ClampMin = "0.0"))
	float SwingResetTime = 0.2f;

	/** 몬스터 상태 변경 시 호출 */
	UFUNCTION(BlueprintCallable, Category = "Monster Audio")
	void SetMonsterAudioState(EMonsterAudioState NewState);

	/** 즉시 사운드 재생 (Idle/Combat만 사용) */
	UFUNCTION(BlueprintCallable, Category = "Monster Audio")
	void PlaySound(EMonsterAudioState SoundType, bool bForcePlay = false);

	/** 피해받을 때 사운드 (로컬 전용 - RepNotify에서 자동 호출) */
	UFUNCTION(BlueprintCallable, Category = "Monster Audio")
	void PlayHurtSoundLocal();


	/** 현재 오디오 상태 반환 */
	UFUNCTION(BlueprintPure, Category = "Monster Audio")
	EMonsterAudioState GetCurrentAudioState() const { return CurrentAudioState; }

	// 스윙 사운드 재생
	UFUNCTION(BlueprintCallable, Category = "Monster Audio|Swing")
	void PlaySwingSound();

	// 스윙 사운드 중단
	/*UFUNCTION(BlueprintCallable, Category = "Monster Audio|Swing")
    void StopSwingSound();

    // Combat 사운드 중단
    UFUNCTION(BlueprintCallable, Category = "Monster Audio|Combat")
    void StopCombatSound();*/

	// RTS 커맨드 사운드
	UFUNCTION(BlueprintCallable, Category = "Monster Audio|RTS")
	void PlayRTSCommandSound(ERTSCommandSoundType CommandType);

	// Replication 설정
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 몬스터 에셋 프리로딩 */
	void PreloadMonsterAssets();

protected:
	// 프리로드된 오디오 캐시
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedIdleSound;
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedCombatSound;
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedHurtSound;
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedSwingSound;
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedSelectionClickSound;
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedRTSMoveCommandSound;
	UPROPERTY()
	TObjectPtr<UAkAudioEvent> CachedRTSAttackCommandSound;

protected:
	// BaseClass override
	virtual void CheckForStateChanges() override;
	virtual float GetMaxAudioDistance() const override;

private:
	UPROPERTY()
	TObjectPtr<AGS_Monster> OwnerMonster;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentAudioState)
	EMonsterAudioState CurrentAudioState;

	UPROPERTY()
	EMonsterAudioState PreviousAudioState;

	FTimerHandle IdleSoundTimer;
	FTimerHandle CombatSoundTimer;

	/** 가장 가까운 시커 찾기 */
	AGS_Seeker* FindNearestSeeker() const;

	/** 시커와의 거리 계산 */
	float CalculateDistanceToNearestSeeker() const;

	/** 자동 사운드 재생 (타이머 콜백) */
	void PlayIdleSound();
	void PlayCombatSound();

	/** 타이머 관리 */
	void StartSoundTimer();
	void StopSoundTimer();
	void UpdateSoundTimer();

	/** Wwise 이벤트 실제 재생 (Wwise가 거리 감쇠 자동 처리) */
	UAkAudioEvent* GetSoundEvent(EMonsterAudioState SoundType) const;

	UFUNCTION()
	void OnRep_CurrentAudioState();

	// 클라이언트 사운드 재생 트리거용 멀티캐스트 RPC
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_TriggerSound(EMonsterAudioState SoundTypeToTrigger, bool bIsImmediate);

	// 스윙 사운드 멀티캐스트 RPC
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySwingSound();

	// 스윙 사운드 중단 멀티캐스트 RPC
	/*UFUNCTION(NetMulticast, Reliable)
    void Multicast_StopSwingSound();

    // Combat 사운드 중단 멀티캐스트 RPC
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_StopCombatSound();*/

	TMap<EMonsterAudioState, float> LocalLastSoundPlayTimes;

	UPROPERTY(Transient)
	TMap<EMonsterAudioState, float> ServerLastBroadcastTime;

	// 스윙 사운드 쿨다운 기록용 변수
	UPROPERTY(Transient)
	float ServerLastSwingBroadcastTime = -1000.0f;

	UPROPERTY(Transient)
	float LocalLastSwingPlayTime = -1000.0f;

	// 공격 관련 사운드 PlayingID 추적
	/*UPROPERTY(Transient)
    uint32 CurrentSwingPlayingID = 0;

    UPROPERTY(Transient)
    uint32 CurrentCombatPlayingID = 0;*/

	// ===================
	// FindNearestSeeker 캐싱 시스템
	// ===================

	/** 캐싱된 가장 가까운 시커 */
	UPROPERTY(Transient)
	mutable TWeakObjectPtr<AGS_Seeker> CachedNearestSeeker;

	/** 마지막 Seeker 검색 시간 */
	UPROPERTY(Transient)
	mutable float LastSeekerSearchTime = -1000.0f;

	/** Seeker 캐시 유효 시간 (초) */
	static constexpr float SeekerCacheValidDuration = 0.5f;
};