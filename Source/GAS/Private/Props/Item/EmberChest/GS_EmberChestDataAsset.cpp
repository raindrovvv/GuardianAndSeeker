// Fill out your copyright notice in the Description page of Project Settings.

#include "Props/Item/EmberChest/GS_EmberChestDataAsset.h"

FEmberRewardConfig UGS_EmberChestDataAsset::GetRandomReward() const
{
	if (RewardPool.Num() == 0)
	{
		return FEmberRewardConfig();
	}

	// 가중치 합계 계산
	float TotalWeight = 0.0f;
	for (const FEmberRewardConfig& Config : RewardPool)
	{
		TotalWeight += Config.SpawnWeight;
	}

	if (TotalWeight <= 0.0f)
	{
		return RewardPool[0];
	}

	// 가중치 기반 랜덤 선택
	float RandomValue = FMath::FRand() * TotalWeight;
	float AccumulatedWeight = 0.0f;

	for (const FEmberRewardConfig& Config : RewardPool)
	{
		AccumulatedWeight += Config.SpawnWeight;
		if (RandomValue <= AccumulatedWeight)
		{
			return Config;
		}
	}

	// Fallback
	return RewardPool.Last();
}
