// Fill out your copyright notice in the Description page of Project Settings.

#include "Props/Item/EmberChest/GS_EmberChest.h"
#include "Props/Item/EmberChest/GS_EmberChestDataAsset.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Component/GS_StatComp.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "AkGameplayStatics.h"
#include "AkAudioEvent.h"

AGS_EmberChest::AGS_EmberChest()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// 충돌 구체 생성
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->SetSphereRadius(150.0f);
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionSphere->SetGenerateOverlapEvents(true);

	// 나이아가라 VFX 컴포넌트 생성
	ChestVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ChestVFX"));
	ChestVFX->SetupAttachment(RootComponent);
	ChestVFX->bAutoActivate = false;

	// 초기 상태
	CurrentState = EEmberChestState::Materializing;
	AssignedRewardType = EEmberRewardType::None;
}

void AGS_EmberChest::BeginPlay()
{
	Super::BeginPlay();

	// 충돌 반경 설정 (DataAsset에서 가져오기)
	if (ChestDataAsset)
	{
		CollisionSphere->SetSphereRadius(ChestDataAsset->InteractionRadius);
	}

	// 오버랩 이벤트 바인딩
	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AGS_EmberChest::OnOverlapBegin);
	CollisionSphere->OnComponentEndOverlap.AddDynamic(this, &AGS_EmberChest::OnOverlapEnd);

	// 서버에서만 상태 초기화
	if (HasAuthority())
	{
		SetChestState(EEmberChestState::Materializing);

		// 실체화 타이머 시작
		float MaterializingDuration = ChestDataAsset ? ChestDataAsset->MaterializingDuration : 2.0f;
		GetWorldTimerManager().SetTimer(
			MaterializingTimerHandle,
			this,
			&AGS_EmberChest::OnMaterializingComplete,
			MaterializingDuration,
			false
		);
	}
}

void AGS_EmberChest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 타이머 정리
	GetWorldTimerManager().ClearTimer(MaterializingTimerHandle);
	GetWorldTimerManager().ClearTimer(LifetimeTimerHandle);

	// 버프 제거 타이머 정리
	for (auto& Pair : BuffRemovalTimers)
	{
		GetWorldTimerManager().ClearTimer(Pair.Value);
	}
	BuffRemovalTimers.Empty();

	Super::EndPlay(EndPlayReason);
}

void AGS_EmberChest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGS_EmberChest, CurrentState);
	DOREPLIFETIME(AGS_EmberChest, AssignedRewardType);
}

void AGS_EmberChest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGS_EmberChest::SetReward(const FEmberRewardConfig& InRewardConfig)
{
	CachedRewardConfig = InRewardConfig;
	AssignedRewardType = InRewardConfig.RewardType;
}

void AGS_EmberChest::StartLifetimeTimer(float Lifetime)
{
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(
			LifetimeTimerHandle,
			this,
			&AGS_EmberChest::OnLifetimeExpired,
			Lifetime,
			false
		);
	}
}

void AGS_EmberChest::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 시커만 상호작용 범위에 들어옴
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(OtherActor);
	if (!Seeker)
	{
		return;
	}

	// 대기 상태에서만 상호작용 가능
	if (CurrentState != EEmberChestState::Idle)
	{
		return;
	}

	// 로컬 플레이어에게 상호작용 가능 알림 (컨트롤러에서 처리)
}

void AGS_EmberChest::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(OtherActor);
	if (!Seeker)
	{
		return;
	}

	// 상호작용 중인 시커가 범위를 볷어남 -> 취소
	if (CurrentInteractor.Get() == Seeker)
	{
		EndInteract_Implementation(Seeker, false);
	}
}

void AGS_EmberChest::SetChestState(EEmberChestState NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentState = NewState;
	MulticastPlayStateVFX(NewState);
}

void AGS_EmberChest::OnRep_ChestState()
{
	// 클라이언트에서 상태 변경 시 VFX 업데이트
	MulticastPlayStateVFX(CurrentState);
}

void AGS_EmberChest::OnMaterializingComplete()
{
	if (HasAuthority())
	{
		SetChestState(EEmberChestState::Idle);
	}
}

void AGS_EmberChest::ServerGrantReward_Implementation(AGS_Seeker* Seeker)
{
	// 기본 유효성 검사
	if (!Seeker || CurrentState != EEmberChestState::Idle || !IsValid(this))
	{
		return;
	}

	// 거리 검증 (치트 방지) - 상호작용 반경 + 오차 범위(50cm)
	float ValidRadius = 150.0f;
	if (ChestDataAsset)
	{
		ValidRadius = ChestDataAsset->InteractionRadius;
	}
	// 오차 허용 (네트워크 지연 고려)
	const float Tolerance = 100.0f; 
	const float MaxDistanceSq = FMath::Square(ValidRadius + Tolerance);

	if (FVector::DistSquared(GetActorLocation(), Seeker->GetActorLocation()) > MaxDistanceSq)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EmberChest] ServerGrantReward: Distance check failed for %s. (Distance: %f, Max: %f)"), 
			*Seeker->GetName(), FVector::Dist(GetActorLocation(), Seeker->GetActorLocation()), ValidRadius + Tolerance);
		return;
	}

	// 상태를 획득됨으로 변경
	SetChestState(EEmberChestState::Collected);

	// 획득 VFX 재생
	MulticastPlayCollectedVFX();

	// 보상 타입에 따라 처리
	switch (AssignedRewardType)
	{
	case EEmberRewardType::AttackBuff:
	case EEmberRewardType::SpeedBuff:
	case EEmberRewardType::DefenseBuff:
	case EEmberRewardType::AllStatsBuff:
		ApplyBuff(Seeker);
		break;

	case EEmberRewardType::HealthRestore:
		ApplyHealthRestore(Seeker);
		break;

	default:
		break;
	}

	// 일정 시간 후 액터 파괴 (VFX 재생 시간 확보)
	SetLifeSpan(2.0f);
}

void AGS_EmberChest::ApplyBuff(AGS_Seeker* Seeker)
{
	if (!Seeker)
	{
		return;
	}

	UGS_StatComp* StatComp = Seeker->FindComponentByClass<UGS_StatComp>();
	if (!StatComp)
	{
		return;
	}

	// 버프 스탯 적용
	StatComp->ChangeStat(CachedRewardConfig.BuffStats);

	// 지속 시간이 있으면 타이머로 제거
	if (CachedRewardConfig.Duration > 0.0f)
	{
		FTimerHandle& TimerHandle = BuffRemovalTimers.FindOrAdd(Seeker);
		
		// 람다 캡처를 위해 로컬 변수에 복사
		FGS_StatRow BuffStatsToReset = CachedRewardConfig.BuffStats;

		FTimerDelegate TimerDelegate;
		TimerDelegate.BindLambda([StatComp, BuffStatsToReset]()
		{
			if (IsValid(StatComp))
			{
				StatComp->ResetStat(BuffStatsToReset);
			}
		});

		GetWorldTimerManager().SetTimer(
			TimerHandle,
			TimerDelegate,
			CachedRewardConfig.Duration,
			false
		);
	}

	UE_LOG(LogTemp, Log, TEXT("[EmberChest] %s에게 버프 적용: %s (%.1f초)"),
		*Seeker->GetName(),
		*UEnum::GetValueAsString(AssignedRewardType),
		CachedRewardConfig.Duration);
}

void AGS_EmberChest::ApplyHealthRestore(AGS_Seeker* Seeker)
{
	if (!Seeker)
	{
		return;
	}

	UGS_StatComp* StatComp = Seeker->FindComponentByClass<UGS_StatComp>();
	if (!StatComp)
	{
		return;
	}

	// HP 값을 회복량으로 사용
	float HealAmount = CachedRewardConfig.BuffStats.HP;
	StatComp->ServerRPCHeal(HealAmount);

	UE_LOG(LogTemp, Log, TEXT("[EmberChest] %s에게 체력 회복: %.1f"),
		*Seeker->GetName(), HealAmount);
}

void AGS_EmberChest::MulticastPlayStateVFX_Implementation(EEmberChestState State)
{
	if (!ChestDataAsset || !ChestVFX)
	{
		return;
	}

	UNiagaraSystem* VFXToPlay = nullptr;

	switch (State)
	{
	case EEmberChestState::Materializing:
		VFXToPlay = ChestDataAsset->MaterializingVFX;
		break;

	case EEmberChestState::Idle:
		VFXToPlay = ChestDataAsset->IdleVFX;
		break;

	case EEmberChestState::Collected:
		// 획득 시에는 별도의 VFX 재생
		VFXToPlay = nullptr;
		ChestVFX->Deactivate();
		break;
	}

	if (VFXToPlay)
	{
		ChestVFX->SetAsset(VFXToPlay);
		ChestVFX->Activate(true);
	}
}

void AGS_EmberChest::MulticastPlayCollectedVFX_Implementation()
{
	if (!ChestDataAsset)
	{
		return;
	}

	// 획득 위치에 일회성 VFX 스폰
	if (ChestDataAsset->CollectedVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			ChestDataAsset->CollectedVFX,
			GetActorLocation(),
			FRotator::ZeroRotator,
			FVector(1.0f),
			true,
			true,
			ENCPoolMethod::None,
			true
		);
	}

	// Wwise 사운드 재생
	if (CollectedSound)
	{
		UAkGameplayStatics::PostEventAtLocation(
			CollectedSound,
			GetActorLocation(),
			FRotator::ZeroRotator,
			GetWorld()
		);
	}
}


void AGS_EmberChest::OnLifetimeExpired()
{
	if (HasAuthority() && CurrentState != EEmberChestState::Collected)
	{
		// 시간 초과로 사라짐 - VFX 재생 후 파괴
		SetChestState(EEmberChestState::Collected);
		SetLifeSpan(1.0f);
	}
}

// ============================================
// IInteractable 인터페이스 구현
// ============================================

bool AGS_EmberChest::CanInteract_Implementation(AActor* Interactor) const
{
	// 시커만 상호작용 가능
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(Interactor);
	if (!Seeker)
	{
		return false;
	}

	// 대기 상태에서만 상호작용 가능
	if (CurrentState != EEmberChestState::Idle)
	{
		return false;
	}

	// 이미 다른 시커가 상호작용 중이면 불가
	if (CurrentInteractor.IsValid() && CurrentInteractor.Get() != Seeker)
	{
		return false;
	}

	return true;
}

float AGS_EmberChest::GetInteractionDuration_Implementation() const
{
	return InteractionDuration;
}

void AGS_EmberChest::BeginInteract_Implementation(AActor* Interactor)
{
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(Interactor);
	if (!Seeker || !CanInteract_Implementation(Interactor))
	{
		return;
	}

	CurrentInteractor = Seeker;

	CurrentInteractor = Seeker;
}

void AGS_EmberChest::EndInteract_Implementation(AActor* Interactor, bool bCompleted)
{
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(Interactor);
	if (!Seeker)
	{
		return;
	}

	// 서버에서 호출 시 CurrentInteractor 체크 스킵 (Server RPC로 호출됨)
	if (!HasAuthority())
	{
		// 클라이언트에서는 CurrentInteractor 체크
		if (CurrentInteractor.Get() != Seeker)
		{
			return;
		}
	}

	CurrentInteractor.Reset();

	if (bCompleted)
	{
		// 상호작용 완료 -> 보상 지급 (서버에서만)
		if (HasAuthority())
		{
			ServerGrantReward(Seeker);
		}
	}
}

FText AGS_EmberChest::GetInteractionText_Implementation() const
{
	return InteractionText;
}

int32 AGS_EmberChest::GetInteractionPriority_Implementation() const
{
	// 우선순위: 아군 구조(100) > 보물상자(50) > 기타
	return 50;
}

