// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EEmberRewardType.generated.h"

/**
 * 불씨 보물상자에서 획득 가능한 보상 타입
 * 프로메테우스가 시커에게 보내는 보급품 종류
 */
UENUM(BlueprintType)
enum class EEmberRewardType : uint8
{
	None			UMETA(DisplayName = "None"),
	AttackBuff		UMETA(DisplayName = "공격력 버프"),
	SpeedBuff		UMETA(DisplayName = "이동속도 버프"),
	DefenseBuff		UMETA(DisplayName = "방어력 버프"),
	HealthRestore	UMETA(DisplayName = "체력 회복"),
	AllStatsBuff	UMETA(DisplayName = "전체 스탯 버프"),
};

/**
 * 불씨 상자 상태
 */
UENUM(BlueprintType)
enum class EEmberChestState : uint8
{
	Materializing	UMETA(DisplayName = "실체화 중"),
	Idle			UMETA(DisplayName = "대기"),
	Collected		UMETA(DisplayName = "획득됨"),
};
