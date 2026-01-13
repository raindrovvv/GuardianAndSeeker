// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/RTS/RTS_Skill/GS_RTSSkill_SummonNormal.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillComponent.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillData.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "AI/GS_AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "System/Utility/GS_AssetLoader.h"

UGS_RTSSkill_SummonNormal::UGS_RTSSkill_SummonNormal()
{
}

FVector UGS_RTSSkill_SummonNormal::ActivateSkill(UGS_RTSSkillComponent* SkillComponent, const FVector& TargetLocation)
{
	Super::ActivateSkill(SkillComponent, TargetLocation);

	// 서버에서만 실행
	if (!SkillComponent || !SkillComponent->GetOwner()->HasAuthority())
	{
		return TargetLocation;
	}

	return SpawnMonsterAtLocation(TargetLocation);
}

void UGS_RTSSkill_SummonNormal::PlayCastEffects(const FVector& TargetLocation)
{
	const UGS_RTSSkillData_Summon* SummonData = GetSummonData();
	if (!SummonData)
	{
		Super::PlayCastEffects(TargetLocation);
		return;
	}

	const FVector SpawnLocation = TargetLocation + FVector(0.f, 0.f, SummonData->SpawnHeightOffset);

	UNiagaraSystem* SummonVFX = CachedSummonVFX;
	if (!SummonVFX && !SummonData->SummonVFX.IsNull())
	{
		SummonVFX = UGS_AssetLoader::SyncLoadAsset(SummonData->SummonVFX);
	}

	if (SummonVFX)
	{
		PlaySkillVFX(SummonVFX, SpawnLocation);
	}

	UAkAudioEvent* TPSSound = CachedSummonSound_TPS;
	UAkAudioEvent* RTSSound = CachedSummonSound_RTS;

	if (SummonData && (!TPSSound || !RTSSound))
	{
		if (!TPSSound)
			TPSSound = UGS_AssetLoader::SyncLoadAsset(SummonData->SummonSound_TPS);
		if (!RTSSound)
			RTSSound = UGS_AssetLoader::SyncLoadAsset(SummonData->SummonSound_RTS);
	}

	UAkAudioEvent* SoundToPlay = SelectSoundEvent(TPSSound, RTSSound);
	if (SoundToPlay)
	{
		PlaySkillSound(SoundToPlay, SpawnLocation);
	}
}

void UGS_RTSSkill_SummonNormal::PreloadAssets()
{
	Super::PreloadAssets();

	const UGS_RTSSkillData_Summon* SummonData = GetSummonData();
	if (!SummonData)
		return;

	TArray<FSoftObjectPath> Paths;
	if (!SummonData->SummonVFX.IsNull())
		Paths.Add(SummonData->SummonVFX.ToSoftObjectPath());
	if (!SummonData->SummonSound_TPS.IsNull())
		Paths.Add(SummonData->SummonSound_TPS.ToSoftObjectPath());
	if (!SummonData->SummonSound_RTS.IsNull())
		Paths.Add(SummonData->SummonSound_RTS.ToSoftObjectPath());

	for (const auto& MonsterClassPtr : SummonData->MonsterClasses)
	{
		if (!MonsterClassPtr.IsNull())
			Paths.Add(MonsterClassPtr.ToSoftObjectPath());
	}

	if (Paths.Num() > 0)
	{
		TWeakObjectPtr<UGS_RTSSkill_SummonNormal> WeakThis(this);
		UGS_AssetLoader::AsyncLoadMultipleAssets(Paths, [WeakThis]()
		                                         {
			if (UGS_RTSSkill_SummonNormal* StrongThis = WeakThis.Get())
			{
				const UGS_RTSSkillData_Summon* SData = StrongThis->GetSummonData();
				if (SData)
				{
					StrongThis->CachedSummonVFX = SData->SummonVFX.Get();
					StrongThis->CachedSummonSound_TPS = SData->SummonSound_TPS.Get();
					StrongThis->CachedSummonSound_RTS = SData->SummonSound_RTS.Get();

					StrongThis->CachedMonsterClasses.Empty();
					for (const auto& MClassPtr : SData->MonsterClasses)
					{
						if (auto* LoadedClass = MClassPtr.Get())
						{
							StrongThis->CachedMonsterClasses.Add(LoadedClass);
						}
					}
				}
			} });
	}
}

FVector UGS_RTSSkill_SummonNormal::SpawnMonsterAtLocation(const FVector& Location)
{
	UWorld* World = GetSkillWorld();
	if (!World)
	{
		return Location;
	}

	const UGS_RTSSkillData_Summon* SummonData = GetSummonData();
	if (!SummonData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonNormal: Missing summon data asset"));
		return Location;
	}

	if (SummonData->MonsterClasses.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonNormal: No monster classes configured!"));
		return Location;
	}

	FVector ValidLocation;
	if (!FindValidSpawnLocation(Location, ValidLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonNormal: Could not find valid spawn location"));
		return Location;
	}

	const int32 RandomIndex = FMath::RandRange(0, SummonData->MonsterClasses.Num() - 1);

	TSubclassOf<AGS_Monster> MonsterClass;
	if (CachedMonsterClasses.IsValidIndex(RandomIndex))
	{
		MonsterClass = CachedMonsterClasses[RandomIndex];
	}
	else if (!SummonData->MonsterClasses[RandomIndex].IsNull())
	{
		MonsterClass = UGS_AssetLoader::SyncLoadAsset(SummonData->MonsterClasses[RandomIndex]);
	}

	if (!MonsterClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonNormal: Invalid monster class at index %d"), RandomIndex);
		return Location;
	}

	const FVector SpawnLocation = ValidLocation + FVector(0.f, 0.f, SummonData->SpawnHeightOffset);
	const FRotator SpawnRotation = FRotator::ZeroRotator;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AGS_Monster* SpawnedMonster = World->SpawnActor<AGS_Monster>(
	    MonsterClass,
	    SpawnLocation,
	    SpawnRotation,
	    SpawnParams);

	if (SpawnedMonster)
	{
		// 몬스터 팀으로 설정 (TeamId = 2)
		// IsEnemy 로직에서 몬스터는 시커(TeamId=1)만 공격하도록 설정됨
		SpawnedMonster->TeamId = FGenericTeamId(2);

		SpawnedMonster->SpawnDefaultController();

		// 소환 직후 초기 타겟 클리어 (아군 몬스터를 타겟하지 않도록)
		if (AGS_AIController* AIController = Cast<AGS_AIController>(SpawnedMonster->GetController()))
		{
			AIController->ClearCurrentTarget();

			if (UBlackboardComponent* BBComp = AIController->GetBlackboardComponent())
			{
				BBComp->ClearValue(AGS_AIController::TargetActorKey);
				BBComp->SetValueAsBool(AGS_AIController::TargetLockedKey, false);
			}
		}

		return SpawnLocation;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_SummonNormal: Failed to spawn monster"));
	}

	return Location;
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
		    FVector(500.f, 500.f, 500.f) // 검색 범위
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
