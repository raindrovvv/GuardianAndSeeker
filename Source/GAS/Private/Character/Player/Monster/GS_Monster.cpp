// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Player/Monster/GS_Monster.h"
#include "AI/GS_AIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AkComponent.h"
#include "Animation/Character/GS_MonsterAnimInstance.h"
#include "Net/UnrealNetwork.h"
#include "Sound/GS_AudioManager.h"
#include "Character/Player/Seeker/GS_Seeker.h"
// #include "Character/GS_Character.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Character/Skill/Monster/GS_MonsterSkillComp.h"
#include "Sound/GS_MonsterAudioComponent.h"
#include "Character/Component/GS_VFXComponent.h"
#include "Character/Component/GS_StatComp.h"
#include "Components/DecalComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/Character/GS_HPTextWidgetComp.h"
// #include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "AI/RTS/GS_RTSController.h"
#include "AI/RTS/GS_RTSAttackNotificationManager.h"
#include "System/GameState/GS_InGameGS.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"
#include "Rendering/GS_RenderingConstants.h"


AGS_Monster::AGS_Monster()
{
	AIControllerClass = AGS_AIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	MonsterSkillComp = CreateDefaultSubobject<UGS_MonsterSkillComp>(TEXT("MonsterSkillComp"));

	SkillCooldownWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("SkillCooldownWidgetComp"));
	SkillCooldownWidgetComp->SetupAttachment(RootComponent);
	SkillCooldownWidgetComp->SetVisibility(false);
	SkillCooldownWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	SkillCooldownWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkillCooldownWidgetComp->SetCollisionResponseToAllChannels(ECR_Ignore);

	AkComponent = CreateDefaultSubobject<UAkComponent>("AkComponent");
	AkComponent->SetupAttachment(RootComponent);
	
	// 몬스터 오디오 컴포넌트 생성
	MonsterAudioComponent = CreateDefaultSubobject<UGS_MonsterAudioComponent>("MonsterAudioComponent");

	// VFX 컴포넌트 생성 (디버프 등 모든 VFX)
	VFXComponent = CreateDefaultSubobject<UGS_VFXComponent>("VFXComponent");

	// UI 컴포넌트 생성 및 초기화
	TargetedUIComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("TargetedUI"));
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
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block); // Interactable
	}
	
	bCommandLocked = false;
	bSelectionLocked = false;
	bIsSelected = false;
	bIsTargetUIActive = false;
	bReplicates = true;
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

	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->RegisterMonster(this);
		}
	}

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
		MonsterSkillComp->OnMonsterSkillCooldownChanged.AddDynamic(this, &AGS_Monster::HandleSkillCooldownChanged);
	}

	// Bind to HP change for attack detection
	if (StatComp)
	{
		LastKnownHP = StatComp->GetCurrentHealth();
		StatComp->OnCurrentHPChanged.AddUObject(this, &AGS_Monster::HandleHPChanged);
	}

	// Bind to owner's RTSController for attack notifications

	// Bind to local RTSController for attack notifications (UI/Sound)
	if (GetWorld())
	{
		// Find local player controller (RTS Player)
		if (AGS_RTSController* RTSController = Cast<AGS_RTSController>(UGameplayStatics::GetPlayerController(this, 0)))
		{
			if (RTSController->AttackNotificationManager)
			{
				OnMonsterAttacked.AddUniqueDynamic(RTSController->AttackNotificationManager, &UGS_RTSAttackNotificationManager::OnUnitAttacked);
				//UE_LOG(LogTemp, Log, TEXT("[Monster:%s] Attack notification delegate bound to local RTSController"), *GetName());
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

		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->RegisterMonster(this);
		}
	}

	// === Skeletal Mesh Distance Culling 설정 (클라이언트만) ===
	if (!IsRunningDedicatedServer() && GetMesh())
	{
		USkeletalMeshComponent* MeshComp = GetMesh();
		float CullDistance = GS_Rendering::CalculateCullDistance(this, GetOptimalCullDistance());
		int32 MinLOD = GS_Rendering::CalculateMinLOD(this);

		MeshComp->SetCullDistance(CullDistance);
		MeshComp->SetCachedMaxDrawDistance(CullDistance);
		MeshComp->bAllowCullDistanceVolume = true;
		MeshComp->SetBoundsScale(GS_Rendering::DEFAULT_BOUNDS_SCALE);
		MeshComp->MinLodModel = MinLOD;


		// === Animation Optimization (Client) ===
		MeshComp->bEnableUpdateRateOptimizations = true;
		// 몽타주 재생 중에는 화면 밖이라도 틱을 유지하여 공격 판정(AnimNotify) 보장
		MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;

		UE_LOG(LogTemp, Log, TEXT("[Monster:%s] Rendering Optimization - Cull Distance: %.1f"), *GetName(), CullDistance);
	}

	// === Animation Optimization (Server) ===
	if (IsRunningDedicatedServer() && GetMesh())
	{
		// 서버는 항상 틱을 수행하여 판정 및 로직 보장
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}

void AGS_Monster::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	MonsterAnim = Cast<UGS_MonsterAnimInstance>(GetMesh()->GetAnimInstance());
}

void AGS_Monster::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
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

		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->UnregisterMonster(this);
		}
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

	// Monster HP Bar distance & LoS culling (Client only)
	if (GetNetMode() != NM_DedicatedServer && IsValid(HPTextWidgetComp))
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
			{
				FVector CameraLocation = CameraManager->GetCameraLocation();
				FVector WidgetLocation = GetActorLocation() + FVector(0.f, 0.f, 200.f); // HP 위젯 위치로 상향 조정
				
				float DistSq = FVector::DistSquared(CameraLocation, GetActorLocation());
				
				// 시점에 따른 동적 컬링 거리 계산 (RTS 모드 대응)
				float MaxCullDist = GS_Rendering::CalculateCullDistance(this, GS_Rendering::HP_WIDGET_CULL_DISTANCE);
				float MaxCullDistSq = MaxCullDist * MaxCullDist;

				bool bInRange = (DistSq < MaxCullDistSq);
				bool bIsVisible = bInRange;

				// 거리 내에 있다면 차폐 여부 체크 (TPS 모드에서만 적용, RTS 모드에서는 항상 노출)
				if (bInRange && !GS_Rendering::IsRTSMode(this))
				{
					FHitResult HitResult;
					FCollisionQueryParams Params(NAME_None, false, this);
					Params.AddIgnoredActor(PC->GetPawn()); // 로컬 플레이어 무시
					
					// Visibility 채널을 사용하여 차폐 여부 확인
					if (GetWorld()->LineTraceSingleByChannel(HitResult, CameraLocation, WidgetLocation, ECC_Visibility, Params))
					{
						// 환경(지형, 벽)에 맞았을 때만 가림 처리. 다른 캐릭터에 의한 가림은 무시
						if (HitResult.GetActor() != this && !HitResult.GetActor()->IsA<ACharacter>())
						{
							bIsVisible = false;
						}
					}
				}
				
				if (HPTextWidgetComp->IsVisible() != bIsVisible)
				{
					HPTextWidgetComp->SetVisibility(bIsVisible);
				}
			}
		}
	}
}

void AGS_Monster::OnDeath()
{
	// Death 사운드는 부모 클래스(GS_Character::OnDeath)에서 통합 처리됨
	Super::OnDeath();

	// Unbind attack notification delegate
	if (AController* OwnerController = GetController())
	{
		if (AGS_RTSController* RTSController = Cast<AGS_RTSController>(OwnerController))
		{
			if (RTSController->AttackNotificationManager)
			{
				OnMonsterAttacked.RemoveDynamic(RTSController->AttackNotificationManager, &UGS_RTSAttackNotificationManager::OnUnitAttacked);
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
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			const TArray<TWeakObjectPtr<AGS_Seeker>>& SeekerPtrs = Registry->GetSeekers();

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
		DestroyTimerHandle,
		this,
		&AGS_Monster::HandleDelayedDestroy,
		2.f,
		false
	);
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
	if (bIsTargetUIActive == bIsActive) return;

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

void AGS_Monster::HandleSkillCooldownChanged(float InCurrentCoolTime, float InMaxCoolTime)
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
		// 타겟이 죽었는지 확인
		/*if (AGS_AIController* AIController = Cast<AGS_AIController>(GetController()))
		{
			UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent();
			if (Blackboard)
			{
				AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(AGS_AIController::TargetActorKey));
				if (TargetActor)
				{
					if (AGS_Character* TargetChar = Cast<AGS_Character>(TargetActor))
					{
						if (TargetChar->IsDead())
						{
							// 타겟이 죽었으면 공격 취소
							return;
						}
					}
				}
			}
		}*/

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
	MonsterAnim->Montage_Play(AttackMontage);

	// 주의: PlaySound()는 서버에서만 호출 가능 (HasAuthority 체크)
	// Multicast에서는 직접 호출하지 않음
	// 공격 사운드는 애니메이션 노티파이나 별도 로직에서 처리해야 함
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
		UE_LOG(LogTemp, Log, TEXT("[Monster:%s] HP decreased %.1f -> %.1f, Broadcasting attack notification!"), *GetName(), LastKnownHP, CurrentHP);
		OnMonsterAttacked.Broadcast(this, GetActorLocation());
	}

	LastKnownHP = CurrentHP;
}

float AGS_Monster::GetOptimalCullDistance() const
{
	// 기본값: 중간 크기 몬스터 컬링 거리
	return GS_Rendering::MONSTER_MEDIUM_CULL_DISTANCE;
}
