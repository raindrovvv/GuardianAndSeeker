// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/RTS/RTS_Skill/GS_RTSSkill_SummonElite.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillComponent.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillData.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Component/GS_StatComp.h"
#include "AI/GS_AIController.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"

UGS_RTSSkill_SummonElite::UGS_RTSSkill_SummonElite()
{
}

void UGS_RTSSkill_SummonElite::ActivateSkill(UGS_RTSSkillComponent* SkillComponent, const FVector& TargetLocation)
{
	Super::ActivateSkill(SkillComponent, TargetLocation);

	// 서버에서만 실행
	if (!SkillComponent || !SkillComponent->GetOwner()->HasAuthority())
	{
		return;
	}

	SpawnEliteMonsterAtLocation(TargetLocation);
}

void UGS_RTSSkill_SummonElite::SpawnEliteMonsterAtLocation(const FVector& Location)
{
	UWorld* World = GetSkillWorld();
	if (!World)
	{
		return;
	}

	const UGS_RTSSkillData_Summon* SummonData = GetSummonData();
	if (!SummonData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonElite: Missing summon data asset"));
		return;
	}

	if (SummonData->MonsterClasses.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonElite: No elite monster classes configured!"));
		return;
	}

	FVector ValidLocation;
	if (!FindValidSpawnLocation(Location, ValidLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonElite: Could not find valid spawn location"));
		return;
	}

	const int32 RandomIndex = FMath::RandRange(0, SummonData->MonsterClasses.Num() - 1);
	TSubclassOf<AGS_Monster> EliteMonsterClass = SummonData->MonsterClasses[RandomIndex];

	if (!EliteMonsterClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonElite: Invalid elite monster class at index %d"), RandomIndex);
		return;
	}

	const FVector SpawnLocation = ValidLocation + FVector(0.f, 0.f, SummonData->SpawnHeightOffset);
	const FRotator SpawnRotation = FRotator::ZeroRotator;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AGS_Monster* SpawnedMonster = World->SpawnActor<AGS_Monster>(
		EliteMonsterClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	if (SpawnedMonster)
	{
		SpawnedMonster->SpawnDefaultController();

		if (UGS_StatComp* StatComp = SpawnedMonster->GetStatComp())
		{
			const float StatMultiplier = FMath::Max(SummonData->StatMultiplier, 1.f);

			const float NewMaxHealth = StatComp->GetMaxHealth() * StatMultiplier;
			StatComp->SetMaxHealth(NewMaxHealth);
			StatComp->SetCurrentHealth(NewMaxHealth, true);

			const float NewAttackPower = StatComp->GetAttackPower() * StatMultiplier;
			StatComp->SetAttackPower(NewAttackPower);

			UE_LOG(LogTemp, Log, TEXT("Elite monster stats boosted by %.1fx"), StatMultiplier);
		}

		if (SummonData->SummonVFX)
		{
			PlaySkillVFX(SummonData->SummonVFX, SpawnLocation);
		}

		if (SummonData->SummonSound)
		{
			PlaySkillSound(SummonData->SummonSound, SpawnLocation);
		}

		UE_LOG(LogTemp, Log, TEXT("UGS_RTSSkill_SummonElite: Spawned elite monster %s at %s"),
			*SpawnedMonster->GetName(), *SpawnLocation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonElite: Failed to spawn elite monster"));
	}
}

bool UGS_RTSSkill_SummonElite::FindValidSpawnLocation(const FVector& DesiredLocation, FVector& OutValidLocation) const
{
	UWorld* World = GetSkillWorld();
	if (!World)
	{
		return false;
	}

	// NavMesh를 사용하여 유효한 위치 찾기
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (NavSys)
	{
		FNavLocation NavLocation;
		bool bFound = NavSys->ProjectPointToNavigation(
			DesiredLocation,
			NavLocation,
			FVector(500.f, 500.f, 500.f)
		);

		if (bFound)
		{
			OutValidLocation = NavLocation.Location;
			return true;
		}
	}

	// NavMesh 검색 실패 시 라인트레이스로 지면 찾기
	FHitResult HitResult;
	FVector TraceStart = DesiredLocation + FVector(0.f, 0.f, 1000.f);
	FVector TraceEnd = DesiredLocation - FVector(0.f, 0.f, 1000.f);

	FCollisionQueryParams QueryParams;
	if (OwnerSkillComponent.IsValid())
	{
		QueryParams.AddIgnoredActor(OwnerSkillComponent->GetOwner());
	}

	if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		OutValidLocation = HitResult.Location;
		return true;
	}

	OutValidLocation = DesiredLocation;
	return true;
}

const UGS_RTSSkillData_Summon* UGS_RTSSkill_SummonElite::GetSummonData() const
{
	return Cast<UGS_RTSSkillData_Summon>(GetSkillData());
}
