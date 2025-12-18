// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GS_EmberChestSpawner.generated.h"

class AGS_EmberChest;
class UGS_EmberChestDataAsset;

/**
 * 불씨 보물상자 스폰 관리자
 * GameState에 부착하여 서버에서 상자 스폰을 관리
 * NavMesh 기반 랜덤 위치에 주기적으로 상자 생성
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAS_API UGS_EmberChestSpawner : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_EmberChestSpawner();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ========================
	// 설정
	// ========================

	/** 데이터 에셋 참조 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ember Chest Spawner")
	UGS_EmberChestDataAsset* SpawnerDataAsset;

	/** 스폰할 보물상자 블루프린트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ember Chest Spawner")
	TSubclassOf<AGS_EmberChest> EmberChestClass;

	/** 스폰 중심점 (빈 값이면 월드 원점 기준) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ember Chest Spawner")
	FVector SpawnOrigin;

	/** 스폰 시스템 활성화 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ember Chest Spawner")
	bool bSpawningEnabled = true;

	// ========================
	// 공개 함수
	// ========================

	/** 스폰 시작 */
	UFUNCTION(BlueprintCallable, Category = "Ember Chest Spawner")
	void StartSpawning();

	/** 스폰 중지 */
	UFUNCTION(BlueprintCallable, Category = "Ember Chest Spawner")
	void StopSpawning();

	/** 즉시 상자 스폰 (디버그/테스트용) */
	UFUNCTION(BlueprintCallable, Category = "Ember Chest Spawner")
	AGS_EmberChest* SpawnChestAtLocation(FVector Location);

	/** 현재 활성 상자 수 */
	UFUNCTION(BlueprintCallable, Category = "Ember Chest Spawner")
	int32 GetActiveChestCount() const { return ActiveChests.Num(); }

protected:
	// ========================
	// 스폰 로직
	// ========================

	/** 다음 스폰 예약 */
	void ScheduleNextSpawn();

	/** 스폰 타이머 콜백 */
	void OnSpawnTimerFired();

	/** 랜덤 스폰 위치 찾기 (NavMesh 기반) */
	bool FindRandomSpawnLocation(FVector& OutLocation) const;

	/** 상자 스폰 실행 */
	AGS_EmberChest* SpawnChestInternal(FVector Location);

	/** 상자 파괴 시 호출되는 콜백 */
	UFUNCTION()
	void OnChestDestroyed(AActor* DestroyedActor);

	/** 무효한 상자 참조 정리 */
	void CleanupInvalidChests();

private:
	/** 현재 활성화된 상자들 */
	UPROPERTY()
	TArray<AGS_EmberChest*> ActiveChests;

	/** 스폰 타이머 핸들 */
	FTimerHandle SpawnTimerHandle;
};
