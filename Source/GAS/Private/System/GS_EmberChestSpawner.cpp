// Fill out your copyright notice in the Description page of Project Settings.

#include "System/GS_EmberChestSpawner.h"
#include "Props/Item/EmberChest/GS_EmberChest.h"
#include "Props/Item/EmberChest/GS_EmberChestDataAsset.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

UGS_EmberChestSpawner::UGS_EmberChestSpawner()
{
	PrimaryComponentTick.bCanEverTick = false;
	SpawnOrigin = FVector::ZeroVector;
}

void UGS_EmberChestSpawner::BeginPlay()
{
	Super::BeginPlay();

	// 서버에서만 스폰 관리
	if (GetOwner() && GetOwner()->HasAuthority() && bSpawningEnabled)
	{
		// SpawnOrigin이 설정되지 않았으면 자동으로 찾기
		if (SpawnOrigin.IsNearlyZero())
		{
			UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
			if (NavSystem)
			{
				// NavMesh 바운드 중심 사용
				FBox NavBounds = NavSystem->GetNavigableWorldBounds();
				if (NavBounds.IsValid)
				{
					SpawnOrigin = NavBounds.GetCenter();
				}
			}
		}

		StartSpawning();
	}
}


void UGS_EmberChestSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSpawning();

	// 게임 종료가 아닌 경우에만 상자 정리 (PIE 종료 등에선 엔진이 자동 정리)
	if (EndPlayReason != EEndPlayReason::EndPlayInEditor && 
		EndPlayReason != EEndPlayReason::Quit)
	{
		for (AGS_EmberChest* Chest : ActiveChests)
		{
			if (IsValid(Chest))
			{
				Chest->Destroy();
			}
		}
	}
	
	ActiveChests.Empty();

	Super::EndPlay(EndPlayReason);
}

void UGS_EmberChestSpawner::StartSpawning()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!SpawnerDataAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EmberChestSpawner] SpawnerDataAsset이 설정되지 않았습니다!"));
		return;
	}

	if (!EmberChestClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EmberChestSpawner] EmberChestClass가 설정되지 않았습니다!"));
		return;
	}

	bSpawningEnabled = true;
	ScheduleNextSpawn();
}

void UGS_EmberChestSpawner::StopSpawning()
{
	bSpawningEnabled = false;

	// 월드 유효성 체크
	if (UWorld* World = GetWorld())
	{
		if (World->GetTimerManager().IsTimerActive(SpawnTimerHandle))
		{
			World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		}
	}
}


void UGS_EmberChestSpawner::ScheduleNextSpawn()
{
	if (!bSpawningEnabled || !SpawnerDataAsset)
	{
		return;
	}

	// 랜덤 간격 계산
	float Interval = FMath::RandRange(
		SpawnerDataAsset->SpawnIntervalMin,
		SpawnerDataAsset->SpawnIntervalMax
	);

	GetWorld()->GetTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&UGS_EmberChestSpawner::OnSpawnTimerFired,
		Interval,
		false
	);
}

void UGS_EmberChestSpawner::OnSpawnTimerFired()
{
	// 무효한 상자 정리
	CleanupInvalidChests();

	// 최대 수 체크
	if (SpawnerDataAsset && ActiveChests.Num() >= SpawnerDataAsset->MaxActiveChests)
	{
		ScheduleNextSpawn();
		return;
	}

	// 스폰 위치 찾기
	FVector SpawnLocation;
	if (FindRandomSpawnLocation(SpawnLocation))
	{
		SpawnChestInternal(SpawnLocation);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[EmberChestSpawner] 유효한 스폰 위치를 찾지 못했습니다."));
	}

	// 다음 스폰 예약
	ScheduleNextSpawn();
}

bool UGS_EmberChestSpawner::FindRandomSpawnLocation(FVector& OutLocation) const
{
	if (!SpawnerDataAsset)
	{
		return false;
	}

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EmberChestSpawner] NavigationSystem을 찾을 수 없습니다."));
		return false;
	}

	// NavMesh 전체 바운드 가져오기
	FBox NavBounds = NavSystem->GetNavigableWorldBounds();
	if (!NavBounds.IsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EmberChestSpawner] 유효한 NavMesh 바운드를 찾을 수 없습니다."));
		return false;
	}

	// 최대 시도 횟수
	const int32 MaxAttempts = 30;
	const float MinDistanceFromPrevious = 500.0f; // 이전 스폰 위치와 최소 거리

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		// NavMesh 바운드 내에서 완전히 랜덤한 위치 생성
		FVector RandomPoint(
			FMath::FRandRange(NavBounds.Min.X, NavBounds.Max.X),
			FMath::FRandRange(NavBounds.Min.Y, NavBounds.Max.Y),
			FMath::FRandRange(NavBounds.Min.Z, NavBounds.Max.Z)
		);

		// 가장 가까운 NavMesh 위치 찾기
		FNavLocation NavLocation;
		if (NavSystem->ProjectPointToNavigation(RandomPoint, NavLocation, FVector(500.0f, 500.0f, 1000.0f)))
		{
			FVector CandidateLocation = NavLocation.Location + FVector(0.0f, 0.0f, 50.0f);

			// 이전 스폰 위치들과 거리 체크
			bool bTooClose = false;
			for (const AGS_EmberChest* ExistingChest : ActiveChests)
			{
				if (IsValid(ExistingChest))
				{
					float Distance = FVector::Dist(CandidateLocation, ExistingChest->GetActorLocation());
					if (Distance < MinDistanceFromPrevious)
					{
						bTooClose = true;
						break;
					}
				}
			}

			if (!bTooClose)
			{
				OutLocation = CandidateLocation;
				return true;
			}
		}
	}

	// 최대 시도 후에도 못 찾으면 거리 체크 없이 아무 위치나
	FVector RandomPoint(
		FMath::FRandRange(NavBounds.Min.X, NavBounds.Max.X),
		FMath::FRandRange(NavBounds.Min.Y, NavBounds.Max.Y),
		FMath::FRandRange(NavBounds.Min.Z, NavBounds.Max.Z)
	);

	FNavLocation NavLocation;
	if (NavSystem->ProjectPointToNavigation(RandomPoint, NavLocation, FVector(500.0f, 500.0f, 1000.0f)))
	{
		OutLocation = NavLocation.Location + FVector(0.0f, 0.0f, 50.0f);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[EmberChestSpawner] %d번 시도 후에도 유효한 스폰 위치를 찾지 못했습니다."), MaxAttempts);
	return false;
}

AGS_EmberChest* UGS_EmberChestSpawner::SpawnChestInternal(FVector Location)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return nullptr;
	}

	if (!EmberChestClass || !SpawnerDataAsset)
	{
		return nullptr;
	}

	// 상자 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AGS_EmberChest* NewChest = GetWorld()->SpawnActor<AGS_EmberChest>(
		EmberChestClass,
		Location,
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (NewChest)
	{
		// 랜덤 보상 할당
		FEmberRewardConfig RandomReward = SpawnerDataAsset->GetRandomReward();
		NewChest->SetReward(RandomReward);

		// 수명 타이머 시작
		NewChest->StartLifetimeTimer(SpawnerDataAsset->ChestLifetime);

		// 파괴 시 콜백 등록
		NewChest->OnDestroyed.AddDynamic(this, &UGS_EmberChestSpawner::OnChestDestroyed);

		// 활성 목록에 추가
		ActiveChests.Add(NewChest);
	}

	return NewChest;
}

AGS_EmberChest* UGS_EmberChestSpawner::SpawnChestAtLocation(FVector Location)
{
	return SpawnChestInternal(Location);
}

void UGS_EmberChestSpawner::OnChestDestroyed(AActor* DestroyedActor)
{
	if (AGS_EmberChest* Chest = Cast<AGS_EmberChest>(DestroyedActor))
	{
		ActiveChests.Remove(Chest);
	}
}

void UGS_EmberChestSpawner::CleanupInvalidChests()
{
	ActiveChests.RemoveAll([](AGS_EmberChest* Chest)
	{
		return !IsValid(Chest);
	});
}
