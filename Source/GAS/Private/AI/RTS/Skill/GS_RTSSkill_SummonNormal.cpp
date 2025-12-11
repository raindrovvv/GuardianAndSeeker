// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/RTS/Skill/GS_RTSSkill_SummonNormal.h"
#include "AI/RTS/Skill/GS_RTSSkillComponent.h"
#include "AI/RTS/Skill/GS_RTSSkillData.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "AI/GS_AIController.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"

UGS_RTSSkill_SummonNormal::UGS_RTSSkill_SummonNormal()
{
}

void UGS_RTSSkill_SummonNormal::ActivateSkill(UGS_RTSSkillComponent* SkillComponent, const FVector& TargetLocation)
{
	Super::ActivateSkill(SkillComponent, TargetLocation);

	// 서버에서만 실행
	if (!SkillComponent || !SkillComponent->GetOwner()->HasAuthority())
	{
		return;
	}

	SpawnMonsterAtLocation(TargetLocation);
}

void UGS_RTSSkill_SummonNormal::SpawnMonsterAtLocation(const FVector& Location)
{
	UWorld* World = GetSkillWorld();
	if (!World)
	{
		return;
	}

	const UGS_RTSSkillData_Summon* SummonData = GetSummonData();
	if (!SummonData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonNormal: Missing summon data asset"));
		return;
	}

	if (SummonData->MonsterClasses.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonNormal: No monster classes configured!"));
		return;
	}

	FVector ValidLocation;
	if (!FindValidSpawnLocation(Location, ValidLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonNormal: Could not find valid spawn location"));
		return;
	}

	const int32 RandomIndex = FMath::RandRange(0, SummonData->MonsterClasses.Num() - 1);
	TSubclassOf<AGS_Monster> MonsterClass = SummonData->MonsterClasses[RandomIndex];

	if (!MonsterClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonNormal: Invalid monster class at index %d"), RandomIndex);
		return;
	}

	const FVector SpawnLocation = ValidLocation + FVector(0.f, 0.f, SummonData->SpawnHeightOffset);
	const FRotator SpawnRotation = FRotator::ZeroRotator;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AGS_Monster* SpawnedMonster = World->SpawnActor<AGS_Monster>(
		MonsterClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	if (SpawnedMonster)
	{
		SpawnedMonster->SpawnDefaultController();

		if (SummonData->SummonVFX)
		{
			PlaySkillVFX(SummonData->SummonVFX, SpawnLocation);
		}

		if (SummonData->SummonSound)
		{
			PlaySkillSound(SummonData->SummonSound, SpawnLocation);
		}

		UE_LOG(LogTemp, Log, TEXT("UGS_RTSSkill_SummonNormal: Spawned monster %s at %s"),
			*SpawnedMonster->GetName(), *SpawnLocation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonNormal: Failed to spawn monster"));
	}
}

bool UGS_RTSSkill_SummonNormal::FindValidSpawnLocation(const FVector& DesiredLocation, FVector& OutValidLocation) const
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
			FVector(500.f, 500.f, 500.f)  // 검색 범위
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

	// 기본값으로 원래 위치 사용
	OutValidLocation = DesiredLocation;
	return true;
}

const UGS_RTSSkillData_Summon* UGS_RTSSkill_SummonNormal::GetSummonData() const
{
	return Cast<UGS_RTSSkillData_Summon>(GetSkillData());
}
