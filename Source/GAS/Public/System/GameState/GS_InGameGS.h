// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "GS_InGameGS.generated.h"

UCLASS()
class GAS_API AGS_InGameGS : public AGameState
{
	GENERATED_BODY()

public:
	AGS_InGameGS();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Timer")
	FText GetFormattedTime() const;
	UFUNCTION(BlueprintPure, Category = "Timer")
	float GetRemainingTime() const;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Timer")
	float TotalGameTime;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentTime, BlueprintReadOnly, Category = "Timer")
	float CurrentTime;

	float LastServerTimeUpdate;

	// GameMode가 이 함수들을 호출하여 서버가 생성한 방 개수를 설정합니다.
	void SetDungeonData(int32 InTotalRoomCount);

	// ----- Monster Optimization -----
	// Current active monsters in the world (maintained on both server and client)
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Optimization")
	TArray<class AGS_Monster*> LiveMonsters;

	void RegisterMonster(class AGS_Monster* Monster);
	void UnregisterMonster(class AGS_Monster* Monster);

	// ============================================
	// Ember Chest Spawner
	// ============================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ember Chest")
	class UGS_EmberChestSpawner* EmberChestSpawner;

	/** 보스 음악 상태 설정 (서버 전용) */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetBossMusicState(bool bActive,
						   class UAkAudioEvent* StartEvent = nullptr,
						   class UAkAudioEvent* StopEvent = nullptr);

	// ============================================
	// Room/Door Actor Caching (Performance Optimization)
	// ============================================
	/** 클라이언트에서 스폰된 Room/Door 액터 카운트 (TActorIterator 대체) */
	UPROPERTY(Transient)
	int32 CachedRoomCount;

	/** 전역 업적: 퍼스트 블러드 발생 여부 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Achievements")
	bool bHasFirstBlood = false;

	/** 전역 업적: 가디언 슬레이어 발생 여부 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Achievements")
	bool bHasGuardianSlayer = false;

	/**
	 * 전역 업적 트리거 시도 (서버 전용)
	 * @return 성공적으로 처음 트리거된 경우 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Achievements")
	bool TryTriggerFirstBlood();

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	bool TryTriggerGuardianSlayer();


protected:
	// 나중에 로딩 시스템의 기반이 될, 서버가 생성한 총 방의 개수입니다.
	UPROPERTY(Replicated)
	int32 TotalRoomCount;
	// 이 값이 true로 복제되면 클라이언트들이 방 생성이 완료되었는지 검사를 시작합니다.
	UPROPERTY(ReplicatedUsing = OnRep_DungeonDataReplicated)
	bool bDungeonDataReady;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 보스 음악 복제 변수
	UPROPERTY(ReplicatedUsing = OnRep_BossMusicActive)
	bool bIsBossMusicActive;

	UPROPERTY(Replicated)
	class UAkAudioEvent* CurrentBossMusicStartEvent;

	UPROPERTY(Replicated)
	class UAkAudioEvent* CurrentBossMusicStopEvent;

	UFUNCTION()
	void OnRep_BossMusicActive();

	void UpdateGameTime();

	UFUNCTION()
	void OnRep_CurrentTime();

	FTimerHandle GameTimeHandle;

	// bDungeonDataReady가 클라이언트에 복제될 때 호출될 함수입니다.
	UFUNCTION()
	void OnRep_DungeonDataReplicated();

	// 클라이언트 월드에 모든 방이 스폰되었는지 확인하는 함수입니다.
	void Client_VerifyRoomSpawning();

	/** Room/Door 액터 스폰 감지 핸들러 */
	void OnActorSpawned(AActor* SpawnedActor);

	/** 검증 재시도 타이머 핸들 */
	FTimerHandle RoomVerifyTimerHandle;
};
