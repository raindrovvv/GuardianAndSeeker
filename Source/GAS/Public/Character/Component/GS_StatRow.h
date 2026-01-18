// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GS_StatRow.generated.h"


USTRUCT(BlueprintType)
struct GAS_API FGS_StatRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float HP = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")

	float ATK = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")

	float DEF = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")

	float AGL = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")

	float ATS = 0.f;

	/** 크리티컬 확률 (0.0 ~ 1.0, 예: 0.1 = 10%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float CritRate = 0.1f;

	/** 크리티컬 배율 (예: 1.5 = 150%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float CritDamage = 1.5f;

	FGS_StatRow operator+(const FGS_StatRow& Other) const
	{
		FGS_StatRow Result;
		Result.HP = HP + Other.HP;
		Result.ATK = ATK + Other.ATK;
		Result.DEF = DEF + Other.DEF;
		Result.AGL = AGL + Other.AGL;
		Result.ATS = ATS + Other.ATS;
		// CritRate는 합산 후 상한선 적용 (100% 초과 방지)
		Result.CritRate = FMath::Min(CritRate + Other.CritRate, 1.0f);
		// CritDamage는 배율이므로 보너스만 합산 (예: 1.5 + 0.2 보너스 = 1.7)
		// Other가 0이면 기본값 유지, 아니면 합산
		Result.CritDamage = CritDamage + (Other.CritDamage > 0.f ? Other.CritDamage : 0.f);
		return Result;
	}
};