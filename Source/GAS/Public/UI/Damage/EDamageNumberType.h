// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EDamageNumberType.generated.h"

/**
 * 데미지 숫자 타입 열거형
 * 데미지 타입에 따라 다른 시각적 스타일 적용
 */
UENUM(BlueprintType)
enum class EDamageNumberType : uint8
{
	/** 일반 데미지 - 흰색, 기본 크기 */
	Normal UMETA(DisplayName = "Normal"),

	/** 크리티컬 데미지 - 빨간/주황색, 1.5x 크기 */
	Critical UMETA(DisplayName = "Critical"),

	/** 도트(지속) 데미지 - 보라색, 0.7x 크기 */
	DoT UMETA(DisplayName = "DoT"),

	/** 힐 - 녹색 */
	Heal UMETA(DisplayName = "Heal")
};
