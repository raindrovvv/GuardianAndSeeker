// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Player/Monster/GS_Monster.h"
#include "AI/GS_AIController.h"
#include "AkComponent.h"
#include "Animation/Character/GS_MonsterAnimInstance.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sound/GS_AudioManager.h"

// #include "Character/GS_Character.h"
#include "SignificanceManager.h"
#include "AI/RTS/GS_RTSAttackNotificationManager.h"
#include "AI/RTS/GS_RTSController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Component/GS_VFXComponent.h"
#include "Character/Skill/Monster/GS_MonsterSkillComp.h"
#include "Components/DecalComponent.h"
#include "Components/WidgetComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Sound/GS_MonsterAudioComponent.h"
#include "System/GameState/GS_InGameGS.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"
#include "TimerManager.h"
#include "UI/Character/GS_HPTextWidgetComp.h"

AGS_Monster::AGS_Monster()
{
	AIControllerClass = AGS_AIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	MonsterSkillComp =
	    CreateDefaultSubobject<UGS_MonsterSkillComp>(TEXT("MonsterSkillComp"));

	SkillCooldownWidgetComp =
	    CreateDefaultSubobject<UWidgetComponent>(TEXT("SkillCooldownWidgetComp"));
	SkillCooldownWidgetComp->SetupAttachment(RootComponent);
	SkillCooldownWidgetComp->SetVisibility(false);
	SkillCooldownWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	SkillCooldownWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkillCooldownWidgetComp->SetCollisionResponseToAllChannels(ECR_Ignore);

	AkComponent = CreateDefaultSubobject<UAkComponent>("AkComponent");
	AkComponent->SetupAttachment(RootComponent);

	// 몬스터 오디오 컴포넌트 생성
	MonsterAudioComponent = CreateDefaultSubobject<UGS_MonsterAudioComponent>(
	    "MonsterAudioComponent");

	// VFX 컴포넌트 생성 (디버프 등 모든 VFX)
	VFXComponent = CreateDefaultSubobject<UGS_VFXComponent>("VFXComponent");

	// UI 컴포넌트 생성 및 초기화
	TargetedUIComponent =
	    CreateDefaultSubobject<UWidgetComponent>(TEXT("TargetedUI"));
	TargetedUIComponent->SetupAttachment(RootComponent);
	TargetedUIComponent->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	TargetedUIComponent->SetWidgetSpace(EWidgetSpace::Screen);
	TargetedUIComponent->SetDrawSize(FVector2D(50.0f, 50.f));
	TargetedUIComponent->SetVisibility(false);

	TeamId = FGenericTeamId(2);
	Tags.Add("Monster");

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// RTS 선택을 위한 콜리전 설정 (모든 몬스터에 적용)
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(
		    ECC_GameTraceChannel1, ECR_Block); // Interactable
	}

	bCommandLocked = false;
	bSelectionLocked = false;
	bIsSelected = false;
	bIsTargetUIActive = false;
	bReplicates = true;

	// 네트워크 최적화 초기화
	NetUpdateFrequency = GS_Rendering::NET_UPDATE_FREQ_CLOSE;
	MinNetUpdateFrequency = GS_Rendering::NET_UPDATE_FREQ_MIN;
	LastNetUpdateFrequency = NetUpdateFrequency;
	HitReactComp = CreateDefaultSubobject<UGS_HitReactComp>(TEXT("HitReactComp_Monster"));

	// === Shadow Proxy (Capsule Shadows) 활성화 ===
	if (GetMesh())
	{
		GetMesh()->SetCastCapsuleDirectShadow(true);
		GetMesh()->SetCastCapsuleIndirectShadow(true);
	}
}

void AGS_Monster::BeginPlay()
{
	Super::BeginPlay();

	if (IsRunningDedicatedServer())
	{
		PrimaryActorTick.bCanEverTick = false;
		SetActorTickEnabled(false);
	}
	else
	{
		// 클라이언트에서는 거리 기반 UI 컬링 등을 위해 틱 활성화
		SetActorTickEnabled(true);
	}

	/*	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry =
		        World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->RegisterMonster(this);
		}
	}*/

	// === 데디케이티드 서버 크래시 방지 ===
	// 생성자에서 만든 AkComponent가 리스너 없는 서버에서 Tick하면 크래시 발생
	if (IsRunningDedicatedServer() || GetNetMode() == NM_DedicatedServer)
	{
		if (IsValid(AkComponent))
		{
			AkComponent->Stop();
			AkComponent->SetComponentTickEnabled(false);
			AkComponent->UnregisterComponent();
			AkComponent->DestroyComponent();
			AkComponent = nullptr;
		}
		// 주의: MonsterAudioComponent는 GS_AudioComponentBase를 상속하므로
		// 해당 클래스의 BeginPlay에서 이미 처리됨
	}

	if (IsValid(MonsterSkillComp))
	{
		MonsterSkillComp->OnMonsterSkillCooldownChanged.AddDynamic(
		    this, &AGS_Monster::HandleSkillCooldownChanged);
	}

	// HP 위젯 틱 간격 최적화 (매 프레임 대신 0.1초마다 체크)
	if (IsValid(HPTextWidgetComp))
	{
		HPTextWidgetComp->SetComponentTickInterval(0.1f);
	}

	// Bind to HP change for attack detection
	if (StatComp)
	{
		LastKnownHP = StatComp->GetCurrentHealth();
		StatComp->OnCurrentHPChanged.AddUObject(this,
		                                        &AGS_Monster::HandleHPChanged);
	}

	// Bind to owner's RTSController for attack notifications

	// Bind to local RTSController for attack notifications (UI/Sound)
	if (GetWorld())
	{
		// Find local player controller (RTS Player)
		if (AGS_RTSController* RTSController = Cast<AGS_RTSController>(
		        UGameplayStatics::GetPlayerController(this, 0)))
		{
			if (RTSController->AttackNotificationManager)
			{
				OnMonsterAttacked.AddUniqueDynamic(
				    RTSController->AttackNotificationManager,
				    &UGS_RTSAttackNotificationManager::OnUnitAttacked);
				// UE_LOG(LogTemp, Log, TEXT("[Monster:%s] Attack notification delegate
				// bound to local RTSController"), *GetName());
			}
		}
	}

	// AkComponent Occlusion 비활성화
	if (IsValid(AkComponent))
	{
		AkComponent->OcclusionRefreshInterval = 0.0f;
	}

	// Register to GameState and Subsystem for optimization
	if (UWorld* World = GetWorld())
	{
		if (AGS_InGameGS* GS = World->GetGameState<AGS_InGameGS>())
		{
			GS->RegisterMonster(this);
		}

		if (UGS_ActorRegistrySubsystem* Registry =
		        World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->RegisterMonster(this);
		}
	}

	// === Skeletal Mesh Distance Culling 설정 (클라이언트만) ===
	if (!IsRunningDedicatedServer() && GetMesh())
	{
		USkeletalMeshComponent* MeshComp = GetMesh();
		float CullDistance =
		    GS_Rendering::CalculateCullDistance(this, GetOptimalCullDistance());
		int32 MinLOD = GS_Rendering::CalculateMinLOD(this);

		MeshComp->SetCullDistance(CullDistance);
		MeshComp->SetCachedMaxDrawDistance(CullDistance);
		MeshComp->bAllowCullDistanceVolume = true;
		MeshComp->SetBoundsScale(GS_Rendering::DEFAULT_BOUNDS_SCALE);
		MeshComp->MinLodModel = MinLOD;

		// === Animation Optimization (Client) ===
		MeshComp->bEnableUpdateRateOptimizations = true;
		// 몽타주 재생 중에는 화면 밖이라도 틱을 유지하여 공격 판정(AnimNotify) 보장
		MeshComp->VisibilityBasedAnimTickOption =
		    EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;

		UE_LOG(LogTemp, Log,
		       TEXT("[Monster:%s] Rendering Optimization - Cull Distance: %.1f"),
		       *GetName(), CullDistance);
	}

	// === Animation Optimization (Server) ===
	if (IsRunningDedicatedServer() && GetMesh())
	{
		// 서버는 항상 틱을 수행하여 판정 및 로직 보장
		GetMesh()->VisibilityBasedAnimTickOption =
		    EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// === 네트워크 최적화 타이머 설정 (서버만) ===
	if (HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(
		    NetworkOptimizationTimerHandle, this,
		    &AGS_Monster::UpdateNetworkOptimization, 0.2f, // 0.2초마다 체크
		    true);
	}

	// === 그림자 컬링 초기 설정 (클라이언트만) ===
	if (!IsRunningDedicatedServer())
	{
		UpdateShadowCulling();
	}
}

void AGS_Monster::RegisterSignificanceManager()
{
	// === Significance Manager 등록 (클라이언트만) ===
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (USignificanceManager* SM = USignificanceManager::Get(GetWorld()))
		{
			SM->RegisterObject(this, "Monster", [this](USignificanceManager::FManagedObjectInfo* ObjectInfo, const FTransform& Viewpoint) -> float
			                   { return this->CalculateSignificance(Viewpoint); }, USignificanceManager::EPostSignificanceType::Sequential, [this](USignificanceManager::FManagedObjectInfo* ObjectInfo, float OldValue, float NewValue, bool bExternal)
			                   { this->OnSignificanceChanged(NewValue); });
		}
	}
}

void AGS_Monster::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	MonsterAnim = Cast<UGS_MonsterAnimInstance>(GetMesh()->GetAnimInstance());
}

void AGS_Monster::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGS_Monster, bCommandLocked);
	DOREPLIFETIME(AGS_Monster, bSelectionLocked);
}

void AGS_Monster::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(MonsterAudioComponent))
	{
		MonsterAudioComponent->SetComponentTickEnabled(false);
	}

	if (IsValid(AkComponent))
	{
		AkComponent->SetComponentTickEnabled(false);
		AkComponent->Stop();
	}

	// 델리게이트 해제 (객체 파괴 시 안정성)
	if (IsValid(MonsterSkillComp))
	{
		MonsterSkillComp->OnMonsterSkillCooldownChanged.RemoveAll(this);
	}

	if (StatComp)
	{
		StatComp->OnCurrentHPChanged.RemoveAll(this);
	}

	// 공격 알림 델리게이트 해제
	OnMonsterAttacked.RemoveAll(this);

	// Unregister from GameState and Subsystem
	if (UWorld* World = GetWorld())
	{
		if (AGS_InGameGS* GS = World->GetGameState<AGS_InGameGS>())
		{
			GS->UnregisterMonster(this);
		}

		if (UGS_ActorRegistrySubsystem* Registry =
		        World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->UnregisterMonster(this);
		}

		// Significance Manager 해제는 부모 클래스(GS_Character::EndPlay)에서 수행됨
	}

	// Stability: Ensure widget components are properly cleaned up
	if (IsValid(SkillCooldownWidgetComp))
	{
		SkillCooldownWidgetComp->SetWidget(nullptr);
		SkillCooldownWidgetComp->SetVisibility(false);
		SkillCooldownWidgetComp->DestroyComponent();
	}

	if (IsValid(TargetedUIComponent))
	{
		TargetedUIComponent->SetWidget(nullptr);
		TargetedUIComponent->SetVisibility(false);
		TargetedUIComponent->DestroyComponent();
	}

	Super::EndPlay(EndPlayReason);
}

void AGS_Monster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateShadowCulling();

	// Monster HP Bar visibility (Client only)
	if (GetNetMode() == NM_DedicatedServer || !IsValid(HPTextWidgetComp))
	{
		return;
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
		return;

	APlayerCameraManager* CameraManager = PC->PlayerCameraManager;
	if (!CameraManager)
		return;

	bool bIsVisible = false;
	bool bIsRTSMode = GS_Rendering::IsRTSMode(this);
	FVector CameraLocation = CameraManager->GetCameraLocation();

	if (bIsRTSMode)
	{
		// RTS 모드: 거리 기반 컬링
		float DistSq = FVector::DistSquared(CameraLocation, GetActorLocation());
		float MaxCullDist = GS_Rendering::CalculateCullDistance(this, GS_Rendering::HP_WIDGET_CULL_DISTANCE);
		bIsVisible = (DistSq < FMath::Square(MaxCullDist));
	}
	else
	{
		// TPS 모드 (시커 시점): 근접 전투 중이거나 일정 거리(30m) 내에 있으면 표시
		bool bIsInRange = false;
		if (APawn* LocalPawn = PC->GetPawn())
		{
			float DistSqToSeeker = FVector::DistSquared(GetActorLocation(), LocalPawn->GetActorLocation());
			// 30m 내에 있으면 보이도록 설정 (시커 거리 안전망)
			bIsInRange = (DistSqToSeeker < FMath::Square(3000.0f));

			const float CombatTriggerRadiusSq = FMath::Square(800.0f * 1.1f);
			if (DistSqToSeeker > CombatTriggerRadiusSq)
			{
				bIsInSeekerCombatTrigger = false;
			}
		}

		if (bIsInSeekerCombatTrigger || bIsInRange)
		{
			bIsVisible = true;
			FVector MonsterLocation = GetActorLocation();
			FVector WidgetLocation = HPTextWidgetComp->GetComponentLocation();

			FHitResult HitResult;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(HPWidgetVisibility), true, this);
			Params.AddIgnoredActor(PC->GetPawn());

			// 1. 머리 및 허리 위치 체크 (둘 다 가려져야만 숨김 처리하여 깜빡임 방지)
			bool bHeadBlocked = false;
			if (GetWorld()->LineTraceSingleByChannel(HitResult, CameraLocation, WidgetLocation, ECC_Visibility, Params))
			{
				bool bIsPawn = (HitResult.GetActor() && HitResult.GetActor()->IsA<APawn>());
				bool bIsFloor = (HitResult.ImpactNormal.Z >= 0.7f);
				if (!bIsPawn && !bIsFloor)
				{
					bHeadBlocked = true;
				}
			}

			bool bWaistBlocked = false;
			if (GetWorld()->LineTraceSingleByChannel(HitResult, CameraLocation, MonsterLocation + FVector(0.f, 0.f, 50.f), ECC_Visibility, Params))
			{
				bool bIsPawn = (HitResult.GetActor() && HitResult.GetActor()->IsA<APawn>());
				bool bIsFloor = (HitResult.ImpactNormal.Z >= 0.7f);
				if (!bIsPawn && !bIsFloor)
				{
					bWaistBlocked = true;
				}
			}

			// 머리와 허리 둘 다 가려진 경우에만 비활성화
			if (bHeadBlocked && bWaistBlocked)
			{
				bIsVisible = false;
			}
		}
	}

	// 가시성 업데이트 (위젯이 꺼져있더라도 Tick은 돌아서 다시 켜질지 판단해야 함)
	if (HPTextWidgetComp->IsVisible() != bIsVisible)
	{
		HPTextWidgetComp->SetVisibility(bIsVisible);

		// 최적화: 보일 때는 0.1초(기본값), 안 보일 때는 0.3초마다 체크하여 CPU 부하 감소
		HPTextWidgetComp->SetComponentTickInterval(bIsVisible ? 0.1f : 0.3f);
	}
}

void AGS_Monster::OnDeath()
{
	// Death 사운드는 부모 클래스(GS_Character::OnDeath)에서 통합 처리됨
	Super::OnDeath();

	// Unbind attack notification delegate
	if (AController* OwnerController = GetController())
	{
		if (AGS_RTSController* RTSController =
		        Cast<AGS_RTSController>(OwnerController))
		{
			if (RTSController->AttackNotificationManager)
			{
				OnMonsterAttacked.RemoveDynamic(
				    RTSController->AttackNotificationManager,
				    &UGS_RTSAttackNotificationManager::OnUnitAttacked);
			}
		}
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->DisableMovement();
	}

	// 즉시 콜리전 비활성화하여 공격이 더 이상 들어오지 않도록 처리
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	}

	// 주변의 모든 Seeker에게 이 몬스터 제거 알림
	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry =
		        World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			const TArray<TWeakObjectPtr<AGS_Seeker>>& SeekerPtrs =
			    Registry->GetSeekers();

			for (const TWeakObjectPtr<AGS_Seeker>& SeekerPtr : SeekerPtrs)
			{
				if (AGS_Seeker* Seeker = SeekerPtr.Get())
				{
					Seeker->RemoveCombatMonster(this);
				}
			}
		}
	}

	DetachFromControllerPendingDestroy();
	Multicast_OnDeath();

	FTimerHandle DestroyTimerHandle;
	GetWorldTimerManager().SetTimer(
	    DestroyTimerHandle, this, &AGS_Monster::HandleDelayedDestroy, 2.f, false);
}

void AGS_Monster::HandleDelayedDestroy()
{
	Destroy();
}

void AGS_Monster::Multicast_OnDeath_Implementation()
{
	OnMonsterDead.Broadcast(this);
}

void AGS_Monster::UseSkill()
{
}

void AGS_Monster::ApplyStiffness()
{
}

void AGS_Monster::EndStiffness()
{
}

void AGS_Monster::ShowTargetUI(bool bIsActive)
{
	if (bIsTargetUIActive == bIsActive)
		return;

	if (TargetedUIComponent)
	{
		bIsTargetUIActive = bIsActive;
		TargetedUIComponent->SetVisibility(bIsActive);
	}
}

void AGS_Monster::SetCanUseSkill(bool bCanUse)
{
	if (MonsterSkillComp)
	{
		MonsterSkillComp->SetCanUseSkill(bCanUse);
	}
}

void AGS_Monster::HandleSkillCooldownChanged(float InCurrentCoolTime,
                                             float InMaxCoolTime)
{
	if (SkillCooldownWidgetComp)
	{
		if (InCurrentCoolTime > 0.0f)
		{
			SkillCooldownWidgetComp->SetVisibility(true);
		}
		else
		{
			SkillCooldownWidgetComp->SetVisibility(false);
		}
	}
}

void AGS_Monster::Attack()
{
	if (HasAuthority())
	{
		// 공격 사운드 재생 (서버에서만 호출, Multicast로 전파)
		if (MonsterAudioComponent)
		{
			MonsterAudioComponent->PlaySwingSound();
		}

		// 공격 모션 재생 (모든 클라이언트)
		Multicast_PlayAttackMontage();
	}
}

void AGS_Monster::Multicast_PlayAttackMontage_Implementation()
{
	// Soft Reference 로드
	if (!AttackMontage.IsNull())
	{
		if (UAnimMontage* LoadedMontage = AttackMontage.LoadSynchronous())
		{
			MonsterAnim->Montage_Play(LoadedMontage);
		}
	}
}

void AGS_Monster::SetSelected(bool bSelected, bool bPlaySound)
{
	bIsSelected = bSelected;
	UpdateDecal();

	if (bSelected && bPlaySound && MonsterAudioComponent)
	{
		MonsterAudioComponent->PlayRTSCommandSound(ERTSCommandSoundType::Selection);
	}
}

FLinearColor AGS_Monster::GetCurrentDecalColor()
{
	if (bIsSelected)
	{
		return FLinearColor::Green;
	}
	else if (bIsHovered)
	{
		return FLinearColor::Yellow;
	}
	else
	{
		return FLinearColor::Yellow;
	}
}

void AGS_Monster::UpdateDecal()
{
	if (!SelectionDecal || !ShowDecal())
	{
		SelectionDecal->SetVisibility(false);
		return;
	}

	if (bIsSelected || bIsHovered)
	{
		ShowDecalWithColor(GetCurrentDecalColor());
	}
	else
	{
		SelectionDecal->SetVisibility(false);
	}
}

bool AGS_Monster::ShowDecal()
{
	return true;
}

void AGS_Monster::HandleHPChanged(UGS_StatComp* InStatComp)
{
	if (!InStatComp)
	{
		return;
	}

	float CurrentHP = InStatComp->GetCurrentHealth();

	// Only notify on damage (HP decrease), not healing
	if (CurrentHP < LastKnownHP && !IsDead())
	{
		UE_LOG(LogTemp, Log,
		       TEXT("[Monster:%s] HP decreased %.1f -> %.1f, Broadcasting attack "
		            "notification!"),
		       *GetName(), LastKnownHP, CurrentHP);
		OnMonsterAttacked.Broadcast(this, GetActorLocation());
	}

	LastKnownHP = CurrentHP;
}

float AGS_Monster::GetOptimalCullDistance() const
{
	// 기본값: 중간 크기 몬스터 컬링 거리
	return GS_Rendering::MONSTER_MEDIUM_CULL_DISTANCE;
}

void AGS_Monster::UpdateNetworkOptimization()
{
	// 서버에서만 실행
	if (!HasAuthority())
		return;

	// 거리 기반 네트워크 업데이트 빈도 계산
	float NewFrequency =
	    GS_Rendering::CalculateNetUpdateFrequency(this, GetActorLocation());

	// === 전투 상태 체크: HP가 낮거나 AI 타겟이 있으면 최상위 빈도 보장 ===
	bool bIsAggressiveState = false;
	if (StatComp)
	{
		float HealthRatio = StatComp->GetCurrentHealth() / StatComp->GetMaxHealth();

		// HP가 조금이라도 깎였다면 (99% 이하) 전투 상태로 간주
		if (HealthRatio < 0.99f)
		{
			bIsAggressiveState = true;
		}
	}

	// AI가 타겟을 추적 중이면 전투 중으로 간주
	if (AGS_AIController* AIController =
	        Cast<AGS_AIController>(GetController()))
	{
		if (UBlackboardComponent* Blackboard =
		        AIController->GetBlackboardComponent())
		{
			if (Blackboard->GetValueAsObject(AGS_AIController::TargetActorKey) !=
			    nullptr)
			{
				bIsAggressiveState = true;
			}
		}
	}

	if (bIsAggressiveState)
	{
		NewFrequency =
		    FMath::Max(NewFrequency, GS_Rendering::NET_UPDATE_FREQ_COMBAT);
	}

	// === 이동 상태 체크: 이동 중이면 최소 중거리 빈도 보장 ===
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (MoveComp->Velocity.SizeSquared() >
		    100.0f) // 움직이고 있다면 (약 10cm/s 이상)
		{
			NewFrequency =
			    FMath::Max(NewFrequency, GS_Rendering::NET_UPDATE_FREQ_MEDIUM);

			// 이동 중에는 스무딩 방식을 지수적으로 강제하여 더 부드럽게 보이도록 설정
			if (MoveComp->NetworkSmoothingMode != ENetworkSmoothingMode::Exponential)
			{
				MoveComp->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
			}
		}
	}

	// 변경이 있을 때만 업데이트 (불필요한 연산 방지)
	if (FMath::Abs(NewFrequency - LastNetUpdateFrequency) > 0.1f)
	{
		NetUpdateFrequency = NewFrequency;
		LastNetUpdateFrequency = NewFrequency;

		UE_LOG(
		    LogTemp, Verbose,
		    TEXT("[Monster:%s] Network Optimization - NetUpdateFrequency: %.1fHz"),
		    *GetName(), NewFrequency);
	}
}

void AGS_Monster::UpdateShadowCulling()
{
	// 클라이언트에서만 실행
	if (IsRunningDedicatedServer())
		return;

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
		return;

	// 카메라 위치 가져오기
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
			{
				FVector CameraLocation = CameraManager->GetCameraLocation();
				float Distance = FVector::Dist(GetActorLocation(), CameraLocation);

				// 거리 기반 그림자 설정
				if (Distance > GS_Rendering::SHADOW_DISABLE_DISTANCE)
				{
					// 80m 이상: 동적/정적 그림자 모두 끄되, 캡슐 그림자는 유지 (Grounded 느낌)
					MeshComp->SetCastShadow(false);
					MeshComp->bCastDynamicShadow = false;
				}
				else if (Distance > GS_Rendering::DYNAMIC_SHADOW_DISABLE_DISTANCE)
				{
					// 40-80m: 정적 그림자 및 캡슐 그림자 유지
					MeshComp->SetCastShadow(true);
					MeshComp->bCastDynamicShadow = false;
				}
				else
				{
					// 40m 이내: 고품질 동적 그림자 활성화
					MeshComp->SetCastShadow(true);
					MeshComp->bCastDynamicShadow = true;
				}
			}
		}
	}
}

float AGS_Monster::CalculateSignificance(const FTransform& Viewpoint)
{
	if (IsDead())
		return 0.0f;

	float Score = 0.1f;
	FVector ActorLoc = GetActorLocation();
	FVector ViewLoc = Viewpoint.GetLocation();
	float DistSq = FVector::DistSquared(ActorLoc, ViewLoc);

	// 가디언(RTS) 시점
	if (GS_Rendering::IsRTSMode(this))
	{
		// 선택된 유닛은 최상위 중요도
		if (bIsSelected)
			return 1.0f;

		// 화면 컬링 거리 (RTS 배율 적용) 내에 있는지 확인
		float MaxCullDist = GS_Rendering::CalculateCullDistance(
		    this, GS_Rendering::MONSTER_MEDIUM_CULL_DISTANCE);
		float MaxCullDistSq = FMath::Square(MaxCullDist);

		if (DistSq < MaxCullDistSq)
		{
			Score = 0.8f; // 화면 근처 유닛
		}
		else
		{
			Score = 0.2f; // 먼 유닛
		}
	}
	// 시커(TPS) 시점
	else
	{
		// 30m 거리 기준으로 점수 선형 감쇠
		float CombatRangeSq = FMath::Square(3000.0f);
		Score = FMath::Clamp(1.0f - (DistSq / CombatRangeSq), 0.1f, 1.0f);
	}

	return Score;
}

void AGS_Monster::OnSignificanceChanged(float NewSignificance)
{
	// 1. 부모 클래스의 최적화 로직 수행 (이동, 네트워크 등)
	Super::OnSignificanceChanged(NewSignificance);

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
		return;

	// 2. 중요도에 따라 애니메이션 품질 조절
	MeshComp->bEnableUpdateRateOptimizations = (NewSignificance < 0.8f);

	// 2. 중요도에 따른 애니메이션 틱 옵션 조정
	if (NewSignificance > 0.6f)
	{
		// 중요할 때: 항상 재생 및 본 갱신
		MeshComp->VisibilityBasedAnimTickOption =
		    EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	else if (NewSignificance > 0.2f)
	{
		// 보통일 때: 화면에 보일 때만 갱신
		MeshComp->VisibilityBasedAnimTickOption =
		    EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	}
	else
	{
		// 거의 보이지 않거나 멀 때: 몽타주만 재생 (성능 위주)
		MeshComp->VisibilityBasedAnimTickOption =
		    EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
	}

	// 4. UI 및 틱 최적화
	// 중요도가 작으면 UI 틱 및 가시성 강제 비활성화 (성능 최적화)
	// 가시성 임계값(0.5)은 Tick()의 거리 기반 가시성 로직과 협력함
	bool bUIEnabled = (NewSignificance > 0.4f);

	auto UpdateWidgetOptimization = [&](UWidgetComponent* Widget)
	{
		if (Widget)
		{
			if (!bUIEnabled)
			{
				Widget->SetVisibility(false);
				Widget->SetComponentTickEnabled(false);
			}
			else
			{
				Widget->SetComponentTickEnabled(true);
				// 주의: 여기서 직접 SetVisibility(true)를 하지 않음.
				// 실제 가시성은 Tick()에서 거리 및 장애물 체크를 거쳐 최종 결정됨.
			}
		}
	};

	UpdateWidgetOptimization(HPTextWidgetComp);
	UpdateWidgetOptimization(TargetedUIComponent);
	UpdateWidgetOptimization(SkillCooldownWidgetComp);

	// 5. 틱 활성화 가시성 임계값 결정 (중요도에 따라 틱 간격도 조절하여 성능 최적화)
	if (PrimaryActorTick.bCanEverTick)
	{
		bool bShouldTick = (NewSignificance > 0.05f);
		SetActorTickEnabled(bShouldTick);

		if (bShouldTick)
		{
			// 중요도가 낮을수록(멀수록) 틱 간격을 늘려 레이트레이싱 빈도 감소
			float NewTickInterval = (NewSignificance > 0.8f) ? 0.0f : (NewSignificance > 0.4f ? 0.1f : 0.3f);
			SetActorTickInterval(NewTickInterval);
		}
	}
}
