// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/GS_AISeeker.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Seeker/GS_Ares.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Character/Player/Seeker/GS_Merci.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Character/Skill/GS_SkillBase.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "System/Utility/GS_AssetLoader.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Skill/ESkill.h"
#include "DrawDebugHelpers.h"

AGS_AISeeker::AGS_AISeeker()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // 성능 최적화를 위해 틱 빈도 감소

	// 기본 AI 컨트롤러 클래스 설정 (블루프린트에서 오버라이드 가능)
	AIControllerClass = AGS_SeekerAIController::StaticClass();
}

// ========================================
// 라이프사이클 함수
// ========================================

void AGS_AISeeker::BeginPlay()
{
	Super::BeginPlay();

	// 서버에서만 시커 스폰
	if (HasAuthority())
	{
		// 비동기 에셋 로드 후 스폰
		TArray<FSoftObjectPath> ToLoad;

		// 스폰할 시커 클래스 경로 추가
		FSoftObjectPath SeekerPath;
		switch (SeekerType)
		{
		case ESeekerAIType::Ares:
			SeekerPath = AresClass.ToSoftObjectPath();
			break;
		case ESeekerAIType::Chan:
			SeekerPath = ChanClass.ToSoftObjectPath();
			break;
		case ESeekerAIType::Merci:
			SeekerPath = MerciClass.ToSoftObjectPath();
			break;
		}

		if (SeekerPath.IsValid())
			ToLoad.Add(SeekerPath);
		if (AIControllerClass.ToSoftObjectPath().IsValid())
			ToLoad.Add(AIControllerClass.ToSoftObjectPath());

		if (ToLoad.Num() > 0)
		{
			TWeakObjectPtr<AGS_AISeeker> WeakThis(this);
			UGS_AssetLoader::AsyncLoadMultipleAssets(ToLoad, [WeakThis]()
			                                         {
				if (AGS_AISeeker* StrongThis = WeakThis.Get())
				{
					StrongThis->SpawnSeeker();
				} });
		}
		else
		{
			SpawnSeeker();
		}
	}
}

void AGS_AISeeker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(AresDashTimer);
		TimerManager.ClearTimer(MerciMovingSkillTimer);
		TimerManager.ClearTimer(ChanShieldTimer);
	}

	// 델리게이트 해제
	if (SpawnedSeeker)
	{
		SpawnedSeeker->OnDeathDelegate.RemoveAll(this);
	}

	// 시커 파괴
	if (SpawnedSeeker && HasAuthority())
	{
		SpawnedSeeker->Destroy();
		SpawnedSeeker = nullptr;
	}

	// 캐시 정리
	CachedSkillComp = nullptr;
	CachedStatComp = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AGS_AISeeker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 디버그 정보 표시
	if (bShowDebugInfo)
	{
		DrawDebugInfo();
	}
}

#if WITH_EDITOR
void AGS_AISeeker::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.Property != nullptr)
	                         ? PropertyChangedEvent.Property->GetFName()
	                         : NAME_None;

	// SeekerType 변경 시 추가 처리 (필요시 확장)
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AGS_AISeeker, SeekerType))
	{
		// 에디터에서 타입 변경 시 추가 로직
	}
}
#endif

// ========================================
// 시커 스폰 및 설정
// ========================================

/**
 * 선택된 타입의 시커 캐릭터를 스폰합니다.
 * 네비게이션 메시 위의 유효한 위치에 스폰됩니다.
 */
void AGS_AISeeker::SpawnSeeker()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 타입에 맞는 시커 클래스 가져오기
	TSubclassOf<AGS_Seeker> SeekerClass = GetSeekerClassByType();
	if (!SeekerClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AGS_AISeeker::SpawnSeeker - 타입 %d에 대한 유효한 시커 클래스가 없습니다"), static_cast<int32>(SeekerType));
		return;
	}

	// 스폰 파라미터 설정
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	FVector SpawnLocation = GetActorLocation();

	// 네비게이션 메시에서 유효한 위치 찾기
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (NavSystem)
	{
		FNavLocation NavLocation;
		// 그룹 스폰 시 같은 위치에 겹치는 것을 방지하기 위해 반경 600 사용
		if (NavSystem->GetRandomReachablePointInRadius(SpawnLocation, 600.0f, NavLocation))
		{
			SpawnLocation = NavLocation.Location;
			// 바닥 위로 안전한 마진 추가 (캡슐 절반 높이 88 근처인 90으로 설정)
			SpawnLocation.Z += 90.0f;
		}
	}

	// 시커 스폰
	SpawnedSeeker = World->SpawnActor<AGS_Seeker>(
	    SeekerClass,
	    SpawnLocation,
	    GetActorRotation(),
	    SpawnParams);

	if (SpawnedSeeker)
	{
		// 사망 델리게이트 바인딩
		SpawnedSeeker->OnDeathDelegate.AddDynamic(this, &AGS_AISeeker::HandleSeekerDeath);

		// AI 컨트롤러 설정
		SetupAIController();

		// 컴포넌트 캐싱 (성능 최적화)
		CachedSkillComp = SpawnedSeeker->FindComponentByClass<UGS_SkillComp>();
		CachedStatComp = SpawnedSeeker->GetStatComp();

		// 스폰 완료 이벤트 발생
		OnSeekerSpawned.Broadcast(SpawnedSeeker);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AGS_AISeeker::SpawnSeeker - 시커 스폰 실패"));
	}
}

/**
 * AI 컨트롤러를 생성하고 시커에게 빙의시킵니다.
 * 시커 타입에 따라 공격 사거리 등을 자동 조정합니다.
 */
void AGS_AISeeker::SetupAIController()
{
	if (!SpawnedSeeker || !AIControllerClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// AI 컨트롤러 스폰
	FActorSpawnParameters ControllerSpawnParams;
	ControllerSpawnParams.Owner = this;

	TSubclassOf<AGS_SeekerAIController> LoadedControllerClass = UGS_AssetLoader::SyncLoadClass(AIControllerClass);

	if (!LoadedControllerClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AGS_AISeeker::SetupAIController - AIControllerClass 로드 실패"));
		return;
	}

	SeekerController = World->SpawnActor<AGS_SeekerAIController>(
	    LoadedControllerClass,
	    FVector::ZeroVector,
	    FRotator::ZeroRotator,
	    ControllerSpawnParams);

	if (SeekerController)
	{
		// 힐 임계값 설정
		SeekerController->HealThreshold = HealThreshold;

		// 공격 사거리 설정 (캐릭터 타입별 최소값 보장)
		float FinalRange = AttackRange;
		if (SeekerType == ESeekerAIType::Merci)
		{
			// 메르시는 원거리이므로 최소 1200
			if (FinalRange < 1000.0f)
			{
				FinalRange = 1200.0f;
			}
		}
		else
		{
			// 근접 캐릭터는 최소 350
			if (FinalRange < 250.0f)
			{
				FinalRange = 350.0f;
			}
		}
		SeekerController->AttackRange = FinalRange;
		SeekerController->TrapDetectionRadius = TrapDetectionRadius;

		UE_LOG(LogTemp, Log, TEXT("[AISeeker] %s 스폰 위치: %s"),
		       *GetNameSafe(SpawnedSeeker),
		       *SpawnedSeeker->GetActorLocation().ToString());

		// 시커에게 빙의
		SeekerController->Possess(SpawnedSeeker);
	}
}

/**
 * SeekerType에 따라 적절한 시커 클래스를 반환합니다.
 * 블루프린트 클래스가 설정되어 있으면 그것을 사용하고,
 * 아니면 기본 C++ 클래스를 사용합니다.
 */
TSubclassOf<AGS_Seeker> AGS_AISeeker::GetSeekerClassByType() const
{
	TSoftClassPtr<AGS_Seeker> SelectedSoftClass;

	switch (SeekerType)
	{
	case ESeekerAIType::Ares:
		SelectedSoftClass = TSoftClassPtr<AGS_Seeker>(AresClass.ToSoftObjectPath());
		break;

	case ESeekerAIType::Chan:
		SelectedSoftClass = TSoftClassPtr<AGS_Seeker>(ChanClass.ToSoftObjectPath());
		break;

	case ESeekerAIType::Merci:
		SelectedSoftClass = TSoftClassPtr<AGS_Seeker>(MerciClass.ToSoftObjectPath());
		break;

	default:
		SelectedSoftClass = TSoftClassPtr<AGS_Seeker>(AresClass.ToSoftObjectPath());
		break;
	}

	if (SelectedSoftClass.IsNull())
	{
		return AGS_Ares::StaticClass();
	}

	TSubclassOf<AGS_Seeker> LoadedClass = UGS_AssetLoader::SyncLoadClass(SelectedSoftClass);

	return LoadedClass ? LoadedClass : TSubclassOf<AGS_Seeker>(AGS_Ares::StaticClass());
}

/**
 * 시커 사망 시 호출되는 핸들러
 */
void AGS_AISeeker::HandleSeekerDeath()
{
	OnSeekerDeath.Broadcast();
	UE_LOG(LogTemp, Log, TEXT("AGS_AISeeker: 시커 사망"));
}

// ========================================
// 게터 함수들
// ========================================

AGS_SeekerAIController* AGS_AISeeker::GetSeekerAIController() const
{
	return SeekerController;
}

UGS_SkillComp* AGS_AISeeker::GetSkillComp() const
{
	return CachedSkillComp;
}

UGS_StatComp* AGS_AISeeker::GetStatComp() const
{
	return CachedStatComp;
}

// ========================================
// 전투 스킬 콤보 계산
// ========================================

/**
 * 현재 전투 상황에 맞는 최적의 스킬 콤보를 계산합니다.
 * 
 * 캐릭터별 전술:
 * 
 * ⚔️ Ares (검사):
 * - 아군이 공격받고 있으면 개입 대쉬
 * - 거리가 멀면 접근 대쉬
 * - 가까우면 궁극기 + 콤보
 * 
 * 🏹 Merci (궁수):
 * - 적이 가깝고 아군이 없으면 안개 화살로 탈출
 * - 그 외 궁극기 + 원거리 콤보
 * 
 * 🛡️ Chan (탱커):
 * - 자신 또는 아군이 위협받으면 방패
 * - 그 외 궁극기 + 콤보
 */
TArray<ESkillSlot> AGS_AISeeker::GetOptimalCombo(float DistanceToEnemy) const
{
	TArray<ESkillSlot> Combo;
	if (!SpawnedSeeker)
		return Combo;

	// 컨트롤러에서 전술 정보 가져오기
	bool bIsTacticalMode = false;
	if (SeekerController)
	{
		bIsTacticalMode = (SeekerController->GetHighestPriorityThreat() != nullptr);
	}

	switch (SeekerType)
	{
	case ESeekerAIType::Ares:
	{
		// ⚔️ 아레스: 거리 좁히기 또는 아군 구출
		bool bAllyInTrouble = false;
		AActor* Attacker = nullptr;
		bool bAllyNearby = false;
		if (SeekerController)
			SeekerController->GetTeamTacticalIntel(1200.0f, bAllyInTrouble, Attacker, bAllyNearby);

		// 아군이 공격받고 있으면 개입 대쉬
		if (bAllyInTrouble && Attacker && CanUseSkill(static_cast<int32>(ESkillSlot::Moving)))
		{
			Combo.Add(ESkillSlot::Moving);
		}
		// 거리가 멀면 접근 대쉬
		else if (DistanceToEnemy > 600.0f && CanUseSkill(static_cast<int32>(ESkillSlot::Moving)))
		{
			Combo.Add(ESkillSlot::Moving);
		}

		// 가까우면 궁극기
		if (DistanceToEnemy < 300.0f && CanUseSkill(static_cast<int32>(ESkillSlot::Ultimate)))
		{
			Combo.Add(ESkillSlot::Ultimate);
		}

		// 기본 콤보 및 조준 스킬
		Combo.Add(ESkillSlot::Combo);
		if (CanUseSkill(static_cast<int32>(ESkillSlot::Aiming)))
			Combo.Add(ESkillSlot::Aiming);
		break;
	}

	case ESeekerAIType::Merci:
	{
		// 🏹 메르시: 고립 시 안개, 팀전술 시 카이팅
		bool bAllyInTrouble = false;
		AActor* Attacker = nullptr;
		bool bAllyNearby = false;
		if (SeekerController)
			SeekerController->GetTeamTacticalIntel(800.0f, bAllyInTrouble, Attacker, bAllyNearby);

		// 적이 가깝고 보호해줄 아군이 없으면 안개 화살로 탈출
		if (DistanceToEnemy < 600.0f && !bAllyNearby && CanUseSkill(static_cast<int32>(ESkillSlot::Moving)))
		{
			Combo.Add(ESkillSlot::Moving);
		}

		// 궁극기 사용 가능하면 추가
		if (CanUseSkill(static_cast<int32>(ESkillSlot::Ultimate)))
			Combo.Add(ESkillSlot::Ultimate);

		// 기본 콤보 및 조준 스킬
		Combo.Add(ESkillSlot::Combo);
		if (CanUseSkill(static_cast<int32>(ESkillSlot::Aiming)))
			Combo.Add(ESkillSlot::Aiming);
		break;
	}

	case ESeekerAIType::Chan:
	{
		// 🛡️ 첸: 팀 보호자
		bool bAllyInTrouble = false;
		AActor* Attacker = nullptr;
		bool bAllyNearby = false;
		if (SeekerController)
			SeekerController->GetTeamTacticalIntel(600.0f, bAllyInTrouble, Attacker, bAllyNearby);

		// 자신이 위협받거나 아군이 위기면 방패
		if ((DistanceToEnemy < 400.0f || bAllyInTrouble) && CanUseSkill(static_cast<int32>(ESkillSlot::Ready)))
		{
			Combo.Add(ESkillSlot::Ready);
		}

		// 궁극기 사용 가능하면 추가
		if (CanUseSkill(static_cast<int32>(ESkillSlot::Ultimate)))
			Combo.Add(ESkillSlot::Ultimate);

		// 기본 콤보 및 조준 스킬
		Combo.Add(ESkillSlot::Combo);
		if (CanUseSkill(static_cast<int32>(ESkillSlot::Aiming)))
			Combo.Add(ESkillSlot::Aiming);
		break;
	}
	}

	return Combo;
}

// ========================================
// 공격 수행
// ========================================

/**
 * 상황에 맞는 최적의 공격을 수행합니다.
 * GetOptimalCombo()로 콤보를 계산하고 우선순위에 따라 실행합니다.
 */
void AGS_AISeeker::PerformAttack()
{
	if (!SpawnedSeeker || SpawnedSeeker->IsDead())
	{
		return;
	}

	// 타겟과의 거리 계산
	AActor* Target = SeekerController ? SeekerController->GetFocusActor() : nullptr;
	float Distance = Target ? FVector::Dist(SpawnedSeeker->GetActorLocation(), Target->GetActorLocation()) : 0.0f;

	// 상황에 맞는 콤보 계산
	TArray<ESkillSlot> Combo = GetOptimalCombo(Distance);
	ESeekerBehavior CurrentState = SeekerController ? SeekerController->GetCurrentBehavior() : ESeekerBehavior::Idle;

	// 방어/전술 모드일 때 우선순위 높은 스킬 먼저
	if (CurrentState == ESeekerBehavior::Tactical || CurrentState == ESeekerBehavior::Evade)
	{
		for (ESkillSlot Slot : Combo)
		{
			// 이동(안개/대쉬), 준비(방패), 구르기 우선
			if ((Slot == ESkillSlot::Moving || Slot == ESkillSlot::Ready || Slot == ESkillSlot::Rolling) && CanUseSkill(static_cast<int32>(Slot)))
			{
				PerformSkill(static_cast<int32>(Slot));
				return;
			}
		}
	}

	// 일반 스킬 선택 (45% 확률로 스킬 우선)
	float SkillChance = FMath::RandRange(0.0f, 1.0f);
	bool bPreferSkills = (SkillChance < 0.45f);

	// 스킬 우선 시 스킬부터 시도
	if (bPreferSkills)
	{
		for (ESkillSlot Slot : Combo)
		{
			if (Slot != ESkillSlot::Combo && CanUseSkill(static_cast<int32>(Slot)))
			{
				PerformSkill(static_cast<int32>(Slot));
				return;
			}
		}
	}

	// 기본 공격 또는 남은 스킬 사용
	for (ESkillSlot Slot : Combo)
	{
		if (Slot == ESkillSlot::Combo)
		{
			// 메르시는 활 당기기/발사 로직
			if (SeekerType == ESeekerAIType::Merci)
			{
				AGS_Merci* Merci = Cast<AGS_Merci>(SpawnedSeeker);
				if (Merci)
				{
					if (Merci->GetAimState())
					{
						// 조준 중이면 발사
						Merci->ReleaseArrow(Merci->NormalArrowClass, 0.0f, 1);
					}
					else if (!Merci->GetDrawState())
					{
						// 활 당기기 시작
						Merci->Server_DrawBow(Merci->ComboSkillDrawMontage);
					}
				}
			}
			else
			{
				// 근접 캐릭터 콤보 공격
				SpawnedSeeker->Server_ExecuteComboAttack();
			}
			return;
		}
		else
		{
			if (CanUseSkill(static_cast<int32>(Slot)))
			{
				PerformSkill(static_cast<int32>(Slot));
				return;
			}
		}
	}
}

// ========================================
// 스킬 수행
// ========================================

/**
 * 특정 스킬 슬롯의 스킬을 사용합니다.
 * 캐릭터별 특수 처리:
 * - Ares Moving: 0.8초 후 대쉬 실행
 * - Merci Moving: 1.0초 홀드 후 발사
 * - Chan Ready: 3.0초 후 방패 해제
 */
void AGS_AISeeker::PerformSkill(int32 SkillIndex)
{
	UGS_SkillComp* SkillComp = GetSkillComp();
	if (!SkillComp || !SpawnedSeeker || SpawnedSeeker->IsDead())
	{
		return;
	}

	ESkillSlot SkillSlot = static_cast<ESkillSlot>(SkillIndex);

	// 스킬 활성화
	SkillComp->Server_TryActivateSkill(SkillSlot);

	// 아레스 대쉬 특수 처리 (누르고 있다가 해제)
	if (SeekerType == ESeekerAIType::Ares && SkillSlot == ESkillSlot::Moving)
	{
		GetWorldTimerManager().SetTimer(AresDashTimer, this, &AGS_AISeeker::ExecuteAresDash, 0.8f, false);
	}
	// 메르시 안개 화살 특수 처리 (홀드 후 발사)
	else if (SeekerType == ESeekerAIType::Merci && SkillSlot == ESkillSlot::Moving)
	{
		GetWorldTimerManager().SetTimer(MerciMovingSkillTimer, this, &AGS_AISeeker::ExecuteMerciMovingSkill, 1.0f, false);
	}
	// 첸 방패 특수 처리 (제한 시간 후 해제)
	else if (SeekerType == ESeekerAIType::Chan && SkillSlot == ESkillSlot::Ready)
	{
		GetWorldTimerManager().SetTimer(ChanShieldTimer, this, &AGS_AISeeker::StopChanShield, 3.0f, false);
	}
}

/** 아레스 대쉬 타이머 콜백 */
void AGS_AISeeker::ExecuteAresDash()
{
	if (UGS_SkillComp* SkillComp = GetSkillComp())
	{
		SkillComp->Server_TrySkillCommand(ESkillSlot::Moving);
	}
}

/** 첸 방패 해제 타이머 콜백 */
void AGS_AISeeker::StopChanShield()
{
	if (UGS_SkillComp* SkillComp = GetSkillComp())
	{
		SkillComp->Server_TryDeactiveSkill(ESkillSlot::Ready);
	}
}

/** 메르시 이동 스킬 타이머 콜백 */
void AGS_AISeeker::ExecuteMerciMovingSkill()
{
	if (UGS_SkillComp* SkillComp = GetSkillComp())
	{
		SkillComp->Server_TrySkillCommand(ESkillSlot::Moving);
	}
}

// ========================================
// 메르시 화살 제어
// ========================================

/** 랜덤 특수 화살(도끼/분열)로 변경 */
void AGS_AISeeker::SwitchToRandomSpecialArrow()
{
	AGS_Merci* Merci = Cast<AGS_Merci>(SpawnedSeeker);
	if (!Merci)
		return;

	// 50% 확률로 도끼(1) 또는 분열(2) 화살 선택
	int32 TargetType = (FMath::RandRange(0, 1) == 0) ? 1 : 2;
	Merci->Server_ChangeArrowType(TargetType);
}

/** 일반 화살로 복귀 */
void AGS_AISeeker::ResetToNormalArrow()
{
	AGS_Merci* Merci = Cast<AGS_Merci>(SpawnedSeeker);
	if (!Merci)
		return;

	Merci->Server_ChangeArrowType(0);
}

// ========================================
// 힐 및 구르기
// ========================================

/** 힐 포션 사용 */
void AGS_AISeeker::PerformHeal()
{
	UGS_SkillComp* SkillComp = GetSkillComp();
	if (!SkillComp || !SpawnedSeeker || SpawnedSeeker->IsDead())
	{
		return;
	}

	SkillComp->Server_TryActivateSkill(ESkillSlot::HealPotion);
}

/**
 * 구르기 수행
 * 지정된 방향으로 구르기를 시도하며, 몬스터가 막고 있으면
 * 자동으로 대체 방향을 찾습니다.
 */
void AGS_AISeeker::PerformRoll(FVector Direction)
{
	// 서버 권한 체크
	if (!HasAuthority())
	{
		return;
	}

	if (!SpawnedSeeker || SpawnedSeeker->IsDead())
	{
		return;
	}

	// 방향이 없으면 뒤로 구르기
	if (Direction.IsNearlyZero())
	{
		Direction = -SpawnedSeeker->GetActorForwardVector();
	}
	Direction.Normalize();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector StartLoc = SpawnedSeeker->GetActorLocation();

	// 람다: 해당 방향에 몬스터가 있는지 체크
	TWeakObjectPtr<AGS_AISeeker> WeakThis(this);
	TWeakObjectPtr<AGS_Seeker> WeakSeeker(SpawnedSeeker);
	auto IsDirectionBlockedByMonster = [WeakThis, WeakSeeker, StartLoc, World](const FVector& Dir) -> bool
	{
		if (!WeakThis.IsValid() || !WeakSeeker.IsValid() || !World)
		{
			return false;
		}

		FVector EndLoc = StartLoc + Dir * 400.0f; // 구르기 거리

		FHitResult HitResult;
		TArray<AActor*> ActorsToIgnore;
		ActorsToIgnore.Add(WeakSeeker.Get());

		TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
		ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

		bool bHit = UKismetSystemLibrary::SphereTraceSingleForObjects(
		    World,
		    StartLoc,
		    EndLoc,
		    80.0f, // 캐릭터 캡슐과 비슷한 반경
		    ObjectTypes,
		    false,
		    ActorsToIgnore,
		    EDrawDebugTrace::None,
		    HitResult,
		    true);

		if (bHit)
		{
			if (Cast<AGS_Monster>(HitResult.GetActor()))
			{
				return true;
			}
		}

		return false;
	};

	// 원래 방향이 막혀있으면 대체 방향 찾기
	if (IsDirectionBlockedByMonster(Direction))
	{
		const float AlternateAngles[] = {45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f};
		bool bFoundSafePath = false;

		for (float Angle : AlternateAngles)
		{
			FVector AltDirection = Direction.RotateAngleAxis(Angle, FVector::UpVector);

			if (!IsDirectionBlockedByMonster(AltDirection))
			{
				Direction = AltDirection;
				bFoundSafePath = true;
				break;
			}
		}

		// 모든 방향이 막혀있으면 반대 방향
		if (!bFoundSafePath)
		{
			Direction = Direction * -1.0f;
		}
	}

	// 구르기 실행
	UGS_SkillComp* SkillComp = GetSkillComp();
	if (SkillComp)
	{
		FRotator RollRotation = Direction.Rotation();
		SpawnedSeeker->SetActorRotation(RollRotation);

		SkillComp->Server_TryActivateSkill(ESkillSlot::Rolling);
	}
}

/** 모든 액션 정지 */
void AGS_AISeeker::StopAllActions()
{
	if (!SpawnedSeeker)
	{
		return;
	}

	// 상태 리셋
	SpawnedSeeker->StateReset();

	// 모든 몽타주 정지
	if (UAnimInstance* AnimInstance = SpawnedSeeker->GetMesh()->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.2f);
	}

	// 스킬 인터럽트
	if (UGS_SkillComp* SkillComp = GetSkillComp())
	{
		SkillComp->SkillsInterrupt();
		SkillComp->ResetAllowedSkillsMask();
	}

	// 이동 정지
	if (SeekerController)
	{
		SeekerController->StopMovement();
	}
}

// ========================================
// 상태 확인 함수들
// ========================================

float AGS_AISeeker::GetHealthPercent() const
{
	UGS_StatComp* StatComp = GetStatComp();
	if (StatComp)
	{
		return StatComp->GetCurrentHealth() / StatComp->GetMaxHealth();
	}
	return 0.0f;
}

bool AGS_AISeeker::IsAlive() const
{
	if (SpawnedSeeker)
	{
		return !SpawnedSeeker->IsDead();
	}
	return false;
}

bool AGS_AISeeker::CanUseSkill(int32 SkillIndex) const
{
	UGS_SkillComp* SkillComp = GetSkillComp();
	if (SkillComp && SpawnedSeeker && !SpawnedSeeker->IsDead())
	{
		ESkillSlot Slot = static_cast<ESkillSlot>(SkillIndex);
		if (UGS_SkillBase* Skill = SkillComp->GetSkillFromSkillMap(Slot))
		{
			return SkillComp->IsSkillAllowed(Slot) && Skill->CanActive();
		}
	}
	return false;
}

bool AGS_AISeeker::CanHeal() const
{
	return CanUseSkill(static_cast<int32>(ESkillSlot::HealPotion));
}

void AGS_AISeeker::NotifyGoalReached()
{
	OnReachedGoal.Broadcast();
}

// ========================================
// 디버그 시각화
// ========================================

void AGS_AISeeker::DrawDebugInfo() const
{
#if ENABLE_DRAW_DEBUG
	if (!SpawnedSeeker)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector Location = SpawnedSeeker->GetActorLocation();

	// 시커 타입 텍스트
	FString TypeText;
	switch (SeekerType)
	{
	case ESeekerAIType::Ares:
		TypeText = TEXT("Ares");
		break;
	case ESeekerAIType::Chan:
		TypeText = TEXT("Chan");
		break;
	case ESeekerAIType::Merci:
		TypeText = TEXT("Merci");
		break;
	}

	// 체력 퍼센트 텍스트
	FString HealthText = FString::Printf(TEXT("HP: %.1f%%"), GetHealthPercent() * 100.0f);

	// 디버그 드로잉
	DrawDebugString(World, Location + FVector(0, 0, 150), TypeText, nullptr, FColor::Cyan, 0.0f, true);
	DrawDebugString(World, Location + FVector(0, 0, 130), HealthText, nullptr, FColor::Green, 0.0f, true);
	DrawDebugSphere(World, Location, TrapDetectionRadius, 16, FColor::Yellow, false, 0.0f, 0, 1.0f);
	DrawDebugSphere(World, Location, AttackRange, 16, FColor::Red, false, 0.0f, 0, 1.0f);
#endif
}
