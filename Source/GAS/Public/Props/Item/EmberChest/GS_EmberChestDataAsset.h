// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Props/Item/EmberChest/EEmberRewardType.h"
#include "Character/Component/GS_StatRow.h"
#include "GS_EmberChestDataAsset.generated.h"

class UNiagaraSystem;

/**
 * 개별 보상 설정
 * 각 보상 타입별 수치와 확률을 정의
 */
USTRUCT(BlueprintType)
struct FEmberRewardConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
	EEmberRewardType RewardType = EEmberRewardType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward", meta = (ToolTip = "버프 수치 (예: 공격력 +10이면 10)"))
	FGS_StatRow BuffStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward", meta = (ToolTip = "버프 지속 시간 (초), 0이면 영구"))
	float Duration = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward", meta = (ToolTip = "스폰 확률 가중치 (높을수록 자주 등장)"))
	float SpawnWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward", meta = (ToolTip = "보상 획득 시 재생할 VFX"))
	UNiagaraSystem* RewardVFX = nullptr;
};

/**
 * 불씨 보물상자 데이터 에셋
 * 기획자가 에디터에서 모든 밸런스 수치를 조정할 수 있음
 */
UCLASS(BlueprintType)
class GAS_API UGS_EmberChestDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ========================
	// 스폰 설정
	// ========================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn", meta = (ToolTip = "최소 스폰 간격 (초)"))
	float SpawnIntervalMin = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn", meta = (ToolTip = "최대 스폰 간격 (초)"))
	float SpawnIntervalMax = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn", meta = (ToolTip = "동시에 존재할 수 있는 최대 상자 수"))
	int32 MaxActiveChests = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn", meta = (ToolTip = "상자가 사라지기까지의 시간 (초)"))
	float ChestLifetime = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn", meta = (ToolTip = "NavMesh 랜덤 스폰 반경"))
	float SpawnRadius = 5000.0f;

	// ========================
	// 보상 풀
	// ========================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards", meta = (ToolTip = "가능한 보상 목록"))
	TArray<FEmberRewardConfig> RewardPool;

	// ========================
	// VFX 설정
	// ========================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX", meta = (ToolTip = "실체화 VFX (푸른 불티가 모여드는 효과)"))
	UNiagaraSystem* MaterializingVFX = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX", meta = (ToolTip = "대기 상태 VFX (푸른 불꽃이 일렁이는 효과)"))
	UNiagaraSystem* IdleVFX = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX", meta = (ToolTip = "획득 시 VFX"))
	UNiagaraSystem* CollectedVFX = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX", meta = (ToolTip = "실체화에 걸리는 시간 (초)"))
	float MaterializingDuration = 2.0f;

	// ========================
	// 상호작용 설정
	// ========================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (ToolTip = "상호작용 가능 반경"))
	float InteractionRadius = 150.0f;

	// ========================
	// 헬퍼 함수
	// ========================

	/** 가중치 기반으로 랜덤 보상 선택 */
	UFUNCTION(BlueprintCallable, Category = "Ember Chest")
	FEmberRewardConfig GetRandomReward() const;
};
