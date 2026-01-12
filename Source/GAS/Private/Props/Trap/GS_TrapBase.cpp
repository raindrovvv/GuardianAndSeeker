#include "Props/Trap/GS_TrapBase.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "NiagaraComponent.h"
#include "Character/GS_Character.h"
#include "Engine/DamageEvents.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Props/Trap/TrapMotion/GS_TrapMotionCompBase.h"
#include "EngineUtils.h"
#include "System/GameMode/GS_InGameGM.h"
#include "Character/F_GS_DamageEvent.h"
#include "Kismet/GameplayStatics.h"
#include "AI/RTS/GS_RTSController.h"
#include "AkComponent.h"
#include "AkAudioDevice.h"
#include "VFX/GS_VFX_FunctionLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Weapon/GS_Weapon.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "DungeonEditor/Component/PlaceInfoComponent.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"
#include "GeometryCacheComponent.h"
#include "Components/LightComponent.h"
#include "Components/CapsuleComponent.h"
#include "SignificanceManager.h"
#include "Misc/App.h"
#include "System/Utility/GS_AssetLoader.h"
#include "NiagaraFunctionLibrary.h"
#include "AkGameplayStatics.h"
#include "Character/Player/GS_Player.h"

AGS_TrapBase::AGS_TrapBase()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;

	bReplicates = true;
	SetReplicateMovement(true);

	RootSceneComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComp"));
	RootComponent = RootSceneComp;
	RootSceneComp->PrimaryComponentTick.bCanEverTick = false;
	RootSceneComp->PrimaryComponentTick.bStartWithTickEnabled = false;
	RootSceneComp->PrimaryComponentTick.bAllowTickOnDedicatedServer = false;

	RotationSceneComp = CreateDefaultSubobject<USceneComponent>(TEXT("RotationScene"));
	RotationSceneComp->SetupAttachment(RootComponent);
	RotationSceneComp->PrimaryComponentTick.bCanEverTick = false;
	RotationSceneComp->PrimaryComponentTick.bStartWithTickEnabled = false;
	RotationSceneComp->PrimaryComponentTick.bAllowTickOnDedicatedServer = false;

	MeshParentSceneComp = CreateDefaultSubobject<USceneComponent>(TEXT("MeshParentSceneComp"));
	MeshParentSceneComp->SetupAttachment(RotationSceneComp);
	MeshParentSceneComp->PrimaryComponentTick.bCanEverTick = false;
	MeshParentSceneComp->PrimaryComponentTick.bStartWithTickEnabled = false;
	MeshParentSceneComp->PrimaryComponentTick.bAllowTickOnDedicatedServer = false;

	ActivateSphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("ActivateSphereComp"));
	ActivateSphereComp->SetupAttachment(MeshParentSceneComp);
	ActivateSphereComp->PrimaryComponentTick.bCanEverTick = false;
	ActivateSphereComp->PrimaryComponentTick.bStartWithTickEnabled = false;
	ActivateSphereComp->PrimaryComponentTick.bAllowTickOnDedicatedServer = false;

	ActivateSphereComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ActivateSphereComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	ActivateSphereComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	DamageBoxComp = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageBox"));
	DamageBoxComp->SetupAttachment(MeshParentSceneComp);
	// DamageBox 콜리전 설정
	DamageBoxComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	//ECC_GameTraceChannel4 : Trap 전용 콜리전
	DamageBoxComp->SetCollisionObjectType(ECC_GameTraceChannel4);
	DamageBoxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageBoxComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	// 바닥/벽/천장 충돌은 기본적으로 무시 (활성화 시에만 켜짐)
	DamageBoxComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	//"OptimizedCollision" 태그가 있는 경우, 플레이어가 근접한 경우에만 콜리전 활성화됨
	DamageBoxComp->ComponentTags.Add("OptimizedCollision");
	DamageBoxComp->ComponentTags.Add("DEFENSIBLE_ATTACK");

	AudioAnchorComponent = CreateDefaultSubobject<USceneComponent>(TEXT("AudioAnchor"));
	AudioAnchorComponent->SetupAttachment(RootComponent);
	AudioAnchorComponent->PrimaryComponentTick.bCanEverTick = false;
	AudioAnchorComponent->PrimaryComponentTick.bStartWithTickEnabled = false;
	AudioAnchorComponent->PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	AudioAnchorComponent->SetRelativeLocation(AudioAnchorRelativeLocation);

	// AkComponent는 기본적으로 생성하지 않음 (BP에서 선택적으로 추가)
	TrapAkComponent = nullptr;
}

void AGS_TrapBase::BeginPlay()
{
	Super::BeginPlay();

	LoadTrapData();

	// TrapData가 로드된 후 AudioAnchor 위치 조정
	AdjustAudioAnchorByPlacement();

	RefreshTrapAudioSetup(true);

	TArray<UActorComponent*> PrimComponents;
	GetComponents(UPrimitiveComponent::StaticClass(), PrimComponents);
	for (UActorComponent* Comp : PrimComponents)
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp))
		{
			// AI Climbing Fix: Disable stepping up on trap components
			Prim->CanCharacterStepUpOn = ECB_No;

			// Prevent traps (especially flat ones) from having NavMesh on top
			if (Prim != ActivateSphereComp)
			{
				Prim->SetCanEverAffectNavigation(false);
			}

			if (Prim->ComponentHasTag("OptimizedCollision"))
			{
				if (IsValid(Prim))
				{
					OptimizedCollisionComponents.Add(Prim);
					Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
			}
		}
	}

	if (HasAuthority())
	{
		AGS_TrapManager* TrapManager = GetTrapManager();
		if (TrapManager)
		{
			TrapManager->RegisterTrap(this);
		}
	}

	DamageBoxComp->OnComponentBeginOverlap.AddDynamic(this, &AGS_TrapBase::OnDamageBoxOverlap);
	DamageBoxComp->OnComponentHit.AddDynamic(this, &AGS_TrapBase::OnDamageBoxHit);
	ActivateSphereComp->OnComponentBeginOverlap.AddDynamic(this, &AGS_TrapBase::OnActivSCompBeginOverlap);

	// Register with ActorRegistrySubsystem
	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->RegisterTrap(this);
		}
	}

	// === Static Mesh Distance Culling 설정 (클라이언트만) ===
	if (!IsRunningDedicatedServer())
	{
		ApplyDistanceCulling();
		RegisterSignificanceManager();

		// 초기 가시성 업데이트 간격 설정 (중요도 시스템에 의해 관리됨)
		float RandomVariance = FMath::RandRange(0.0f, 0.5f);
		if (IsValid(GetWorld()))
		{
			GetWorld()->GetTimerManager().SetTimer(ShadowCullingTimerHandle, this, &AGS_TrapBase::UpdateShadowCulling, GS_Rendering::TIMER_INTERVAL_LOW, true, RandomVariance);
		}

		// 초기 1회 즉시 실행
		CacheOptimizedComponents();
		UpdateShadowCulling();
	}
}

void AGS_TrapBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Significance Manager 해제
	if (!IsRunningDedicatedServer() && GetWorld())
	{
		if (USignificanceManager* SM = USignificanceManager::Get(GetWorld()))
		{
			SM->UnregisterObject(this);
		}
	}

	// 타이머 정리
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CheckOverlapTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(ShadowCullingTimerHandle);
	}

	// 델리게이트 해제 (객체 파괴 시 안정성)
	if (DamageBoxComp)
	{
		DamageBoxComp->OnComponentBeginOverlap.RemoveAll(this);
		DamageBoxComp->OnComponentHit.RemoveAll(this);
	}

	ActivateSphereComp->OnComponentBeginOverlap.RemoveAll(this);

	// Unregister from ActorRegistrySubsystem
	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->UnregisterTrap(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AGS_TrapBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Transform 검증 (서버 안정성)
	// FQuat을 FRotator로 변환
	const FRotator TransformRotation = Transform.GetRotation().Rotator();
	if (!UGS_AudioComponentBase::IsTransformValid(Transform.GetLocation(), TransformRotation))
	{
		UE_LOG(LogTemp, Error, TEXT("[TrapBase] Invalid Transform in OnConstruction - Actor: %s"), *GetName());
		return;
	}

	// 에디터에서 TrapID 변경 시 재로딩을 위해 플래그 초기화
	// (OnConstruction은 프로퍼티 변경 시마다 호출되므로 항상 재로드 필요)
	bTrapDataLoaded = false;
	LoadTrapData();

	// TrapData가 로드된 후 AudioAnchor 위치 조정
	AdjustAudioAnchorByPlacement();

	RefreshTrapAudioSetup(true);

	// 에디터에서도 컬링 거리를 시각적으로 확인하고 프리뷰를 갱신하기 위해 호출
	if (FApp::CanEverRender())
	{
		ApplyDistanceCulling();

#if WITH_EDITOR
		// === 맵 체크 경고 해결을 위한 자동 보정 로직 ===
		TArray<UStaticMeshComponent*> MeshComps;
		GetComponents<UStaticMeshComponent>(MeshComps);
		for (UStaticMeshComponent* Mesh : MeshComps)
		{
			if (Mesh)
			{
				// 1. BoundsScale이 1보다 크면 퍼포먼스 경고가 발생하므로 1.0으로 강제 수정
				if (Mesh->BoundsScale > 1.0f)
				{
					Mesh->SetBoundsScale(1.0f);
				}

				// 2. Static Mesh가 할당되지 않은 경우 에디터 로그로 알림
				if (Mesh->GetStaticMesh() == nullptr)
				{
					UE_LOG(LogTemp, Warning, TEXT("[MapCheck Fix] %s의 메쉬 컴포넌트(%s)에 StaticMesh가 할당되지 않았습니다!"),
					       *GetName(), *Mesh->GetName());
				}
			}
		}
#endif
	}
}

void AGS_TrapBase::RefreshTrapAudioSetup(bool bForceFindComponent)
{
	// World 유효성 체크 (서버 안정성)
	if (!GetWorld() || !IsValid(this))
	{
		return;
	}

	// === 데디케이티드 서버 크래시 방지 ===
	// BP에서 추가된 AkComponent가 리스너 없는 서버에서 Tick하면 크래시 발생
	// DestroyComponent 대신 비활성화로 안전하게 처리
	if (IsRunningDedicatedServer() || GetNetMode() == NM_DedicatedServer)
	{
		TrapAkComponent = FindComponentByClass<UAkComponent>();
		if (IsValid(TrapAkComponent))
		{
			TrapAkComponent->Stop();
			TrapAkComponent->SetComponentTickEnabled(false);
			TrapAkComponent->Deactivate();
			TrapAkComponent->UnregisterComponent();
			TrapAkComponent = nullptr;
		}
		return; // 서버에서는 오디오 설정 중단
	}

	if (IsValid(AudioAnchorComponent))
	{
		AudioAnchorComponent->SetRelativeLocation(AudioAnchorRelativeLocation);
	}

	if (bForceFindComponent || TrapAkComponent == nullptr)
	{
		TrapAkComponent = FindComponentByClass<UAkComponent>();
	}

	if (!IsValid(TrapAkComponent))
	{
		return;
	}

	TrapAkComponent->SetComponentTickEnabled(false);
	AttachTrapAkComponentToAnchor();
}

void AGS_TrapBase::AttachTrapAkComponentToAnchor()
{
	if (!IsValid(TrapAkComponent))
	{
		return;
	}

	// World 유효성 체크 (서버 안정성)
	if (!GetWorld() || !IsValid(this))
	{
		return;
	}

	// 오클루전 활성화 (벽에 의한 소리 감쇠)
	TrapAkComponent->OcclusionRefreshInterval = 0.2f;
	TrapAkComponent->EnableSpotReflectors = false; // Spot Reflector 비활성화

// Wwise의 Diffraction 및 Transmission Loss 기능 활성화
// (벽/천장을 통과하면 소리가 감쇠되도록)
#if WITH_EDITOR
	// 에디터에서만 디버그 로그 출력
	UE_LOG(LogTemp, Verbose, TEXT("[TrapBase] Audio Occlusion disabled for %s"), *GetName());
#endif

	if (bUseAudioAnchor && IsValid(AudioAnchorComponent))
	{
		const FVector AnchorLocation = AudioAnchorComponent->GetComponentLocation();
		const FRotator AnchorRotation = AudioAnchorComponent->GetComponentRotation();

		// Transform 검증 (NaN/Infinity 체크)
		if (!UGS_AudioComponentBase::IsTransformValid(AnchorLocation, AnchorRotation))
		{
			UE_LOG(LogTemp, Error, TEXT("[TrapBase] Invalid AudioAnchor Transform detected - Actor: %s"), *GetName());
			return;
		}

		TrapAkComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		TrapAkComponent->AttachToComponent(AudioAnchorComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		TrapAkComponent->SetRelativeLocation(FVector::ZeroVector);
		TrapAkComponent->SetRelativeRotation(FRotator::ZeroRotator);
		TrapAkComponent->SetRelativeScale3D(FVector::OneVector);
		TrapAkComponent->SetWorldLocation(AnchorLocation);
		TrapAkComponent->SetWorldRotation(AnchorRotation);
	}
	else if (IsValid(RootComponent))
	{
		TrapAkComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		TrapAkComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void AGS_TrapBase::AdjustAudioAnchorByPlacement()
{
	if (!IsValid(AudioAnchorComponent))
	{
		return;
	}

	// TrapData의 Placement 타입에 따라 AudioAnchor 위치 동적 조정
	switch (TrapData.Placement)
	{
	case ETrapPlacement::Ceiling:
		// 천장 함정: 아래쪽으로 위치 이동 (천장 안쪽에 들어가지 않도록)
		AudioAnchorRelativeLocation = FVector(0.0f, 0.0f, -150.0f); // 1.5m 아래
		break;

	case ETrapPlacement::Wall:
		// 벽 함정: 앞쪽으로 위치 이동 (벽 안쪽에 들어가지 않도록)
		AudioAnchorRelativeLocation = FVector(150.0f, 0.0f, 0.0f); // 1.5m 앞
		break;

	case ETrapPlacement::Floor:
	default:
		// 바닥 함정: 위쪽 (기본값 유지)
		AudioAnchorRelativeLocation = FVector(0.0f, 0.0f, 120.0f); // 1.2m 위
		break;
	}

	AudioAnchorComponent->SetRelativeLocation(AudioAnchorRelativeLocation);

	// TrapAkComponent가 이미 Attach되어 있으면 다시 Attach
	if (IsValid(TrapAkComponent))
	{
		AttachTrapAkComponentToAnchor();
	}

	UE_LOG(LogTemp, Verbose, TEXT("[TrapBase] AudioAnchor adjusted for %s placement: %s"),
	       *UEnum::GetValueAsString(TrapData.Placement), *AudioAnchorRelativeLocation.ToString());
}

//함정 활성화
void AGS_TrapBase::OnActivSCompBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                            bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this)
	{
		AGS_Seeker* Seeker = Cast<AGS_Seeker>(OtherActor);
		if (Seeker)
		{
			// Capsule 컴포넌트와 오버랩된 경우에만 활성화. CombatTrigger 같은 다른 콜리전 컴포넌트에 의한 오버랩은 무시
			if (OtherComp != Cast<UPrimitiveComponent>(Seeker->GetCapsuleComponent()))
			{
				return;
			}

			if (!bIsActivated)
			{
				bIsActivated = true;

				// 클라이언트에서만 로컬 사운드 재생 (서버는 ActivateTrap_Implementation에서 Multicast로 처리)
				if (!HasAuthority())
				{
					PlayActivationSound();
					Server_ActivateTrap(OtherActor);
				}
				else
				{
					ActivateTrap(OtherActor);
				}

				if (!GetWorld()->GetTimerManager().IsTimerActive(CheckOverlapTimerHandle))
				{
					StartDeactivateTrapCheck();
				}
			}
		}
	}
}

void AGS_TrapBase::Server_ActivateTrap_Implementation(AActor* TargetActor)
{
	ActivateTrap(TargetActor);
}

void AGS_TrapBase::ActivateTrap_Implementation(AActor* TargetActor)
{
	Multicast_EnableOptimizedCollision();

	// 활성화 사운드 재생
	PlayActivationSound();
}

void AGS_TrapBase::Multicast_EnableOptimizedCollision_Implementation()
{
	for (auto& WeakPrim : OptimizedCollisionComponents)
	{
		if (UPrimitiveComponent* Prim = WeakPrim.Get())
		{
			Prim->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}
}

//Sphere Comp에 End Overlap 시,
void AGS_TrapBase::StartDeactivateTrapCheck()
{
	if (IsValid(GetWorld()))
	{
		GetWorld()->GetTimerManager().SetTimer(CheckOverlapTimerHandle, this, &AGS_TrapBase::CheckOverlappingSeeker, 5.0f, true);
	}
}

void AGS_TrapBase::CheckOverlappingSeeker()
{
	TArray<AActor*> OverlappingActors;
	ActivateSphereComp->GetOverlappingActors(OverlappingActors, AGS_Seeker::StaticClass());

	if (OverlappingActors.Num() == 0)
	{
		DeActivateTrap();
		GetWorld()->GetTimerManager().ClearTimer(CheckOverlapTimerHandle);
		bIsActivated = false;
	}
}

void AGS_TrapBase::DeActivateTrap_Implementation()
{
	Multicast_DisableOptimizedCollision();

	// 비활성화 사운드 재생
	PlayDeactivationSound();
}

void AGS_TrapBase::Multicast_DisableOptimizedCollision_Implementation()
{
	for (auto& WeakPrim : OptimizedCollisionComponents)
	{
		if (UPrimitiveComponent* Prim = WeakPrim.Get())
		{
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

//함정 데미지
void AGS_TrapBase::LoadTrapData()
{
	if (bTrapDataLoaded || TrapID.IsNone() || !TrapDataTable)
		return;

	const FTrapData* Data = TrapDataTable->FindRow<FTrapData>(TrapID, TEXT(""));
	if (Data)
	{
		TrapData = *Data;
		bTrapDataLoaded = true;

		// 비동기 에셋 로딩 및 캐싱
		TArray<FSoftObjectPath> AssetsToLoad;
		if (!TrapData.ActivationSound.IsNull())
			AssetsToLoad.Add(TrapData.ActivationSound.ToSoftObjectPath());
		if (!TrapData.AlertSound.IsNull())
			AssetsToLoad.Add(TrapData.AlertSound.ToSoftObjectPath());
		if (!TrapData.HitSound.IsNull())
			AssetsToLoad.Add(TrapData.HitSound.ToSoftObjectPath());
		if (!TrapData.DeactivationSound.IsNull())
			AssetsToLoad.Add(TrapData.DeactivationSound.ToSoftObjectPath());
		if (!TrapData.TrapHitBloodEffect.IsNull())
			AssetsToLoad.Add(TrapData.TrapHitBloodEffect.ToSoftObjectPath());

		if (AssetsToLoad.Num() > 0)
		{
			TWeakObjectPtr<AGS_TrapBase> WeakThis(this);
			UGS_AssetLoader::AsyncLoadMultipleAssets(AssetsToLoad, [WeakThis]()
			                                         {
				if (AGS_TrapBase* Strong = WeakThis.Get())
				{
					// 캐싱 (GC 방지)
					Strong->CachedActivationSound = Strong->TrapData.ActivationSound.Get();
					Strong->CachedAlertSound = Strong->TrapData.AlertSound.Get();
					Strong->CachedHitSound = Strong->TrapData.HitSound.Get();
					Strong->CachedDeactivationSound = Strong->TrapData.DeactivationSound.Get();

					Strong->CachedTrapHitBloodEffect = Strong->TrapData.TrapHitBloodEffect.Get();
				} });
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TrapData not found for TrapID : %s"), *TrapID.ToString());
	}
}

//데미지 박스에 오버랩된 경우 HandleTrapDamage 함수 실행
void AGS_TrapBase::OnDamageBoxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                      bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	// Seeker 필터링을 가장 먼저 수행 (서버/클라이언트 공통)
	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(OtherActor))
	{
		if (OtherComp != Seeker->GetCapsuleComponent())
		{
			return;
		}
	}

	if (!HasAuthority())
	{
		return;
	}

	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(OtherActor))
	{
		// 서버 (Seeker 캡슐 체크는 이미 위에서 완료됨)
		DamageBoxEffect(Seeker);
		CustomTrapEffect(Seeker);
		HandleTrapDamage(Seeker);

		// 함정 히트 사운드 재생 (단, 무기와의 충돌은 무시)
		if (OtherActor && !OtherActor->IsA<AGS_Weapon>())
		{
			PlayHitSound();
		}
		return;
	}

	if (bPlayHitSoundOnEnvironmentImpact && IsEnvironmentHit(OtherActor, OtherComp))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[TrapBase] Environment impact detected (%s)"), *GetNameSafe(OtherActor));
		PlayHitSound();
	}
}

void AGS_TrapBase::OnDamageBoxHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
                                  UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 유효성 체크
	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(OtherActor))
	{
		// Seeker의 경우 오직 CapsuleComponent와의 충돌만 인정 (CombatTrigger 등 감지 방지)
		if (OtherComp != Seeker->GetCapsuleComponent())
		{
			return;
		}
	}

	// 서버권한 체크 (필터링 이후에 수행하여 클라이언트에서도 조기 리턴 가능하게 함)
	if (!HasAuthority())
	{
		return;
	}

	// 함정이 활성화되지 않았거나 무기와 충돌한 경우 Hit 사운드 무시
	if (!bIsActivated || (OtherActor && OtherActor->IsA<AGS_Weapon>()))
	{
		return;
	}

	// 환경에 부딪힌 경우 충돌 사운드 재생
	if (bPlayHitSoundOnEnvironmentImpact && IsEnvironmentHit(OtherActor, OtherComp))
	{
		PlayHitSound();
	}
}

void AGS_TrapBase::Server_HandleTrapDamage_Implementation(AActor* OtherActor)
{
	HandleTrapDamage(OtherActor);
}

EHitReactType AGS_TrapBase::GetHitReactType() const
{
	return EHitReactType::Interrupt;
}

void AGS_TrapBase::HandleTrapDamage(AActor* OtherActor)
{
	if (!OtherActor)
		return;
	AGS_Seeker* DamagedSeeker = Cast<AGS_Seeker>(OtherActor);
	if (!DamagedSeeker)
		return;

	//디버프 연결
	if (UGS_DebuffComp* DebuffComp = DamagedSeeker->FindComponentByClass<UGS_DebuffComp>())
	{

		const FTrapEffect& Effect = TrapData.Effect;

		//Stun
		if (Effect.bStun)
		{
			DebuffComp->ApplyDebuff(EDebuffType::Stun, nullptr);
		}

		//Slow
		if (Effect.bSlow)
		{
			DebuffComp->ApplyDebuff(EDebuffType::Slow, this);
		}

		//Burn
		if (Effect.bBurn)
		{
			DebuffComp->ApplyDebuff(EDebuffType::Burn, this);
		}

		//Lava
		if (Effect.bLava)
		{
			DebuffComp->ApplyDebuff(EDebuffType::Lava, this);
		}
	}

	//기본 데미지 부여
	if (TrapData.Effect.Damage <= 0.f)
		return;

	FGS_DamageEvent DamageEvent;
	DamageEvent.HitReactType = GetHitReactType();

	DamagedSeeker->TakeDamage(TrapData.Effect.Damage, DamageEvent, nullptr, this);

	// 혈흔 이펙트 재생 (시커의 메시 위치에서)
	if (USkeletalMeshComponent* SeekerMesh = DamagedSeeker->GetMesh())
	{
		// 메시의 중앙 위치 가져오기 (Pelvis 본 또는 루트 본)
		FVector HitLocation = SeekerMesh->GetSocketLocation(FName("pelvis"));
		if (HitLocation.IsNearlyZero())
		{
			HitLocation = SeekerMesh->GetComponentLocation();
		}

		Multicast_PlayTrapHitBloodEffect(HitLocation);
	}
}

void AGS_TrapBase::HandleTrapAreaDamage(const TArray<AActor*>& AffectedActors)
{
}

void AGS_TrapBase::Server_DamageBoxEffect_Implementation(AActor* OtherActor)
{
	UE_LOG(LogTemp, Warning, TEXT("Server_DamageBoxEffect_Implementation called"));
	DamageBoxEffect(OtherActor);
}


void AGS_TrapBase::Multicast_DamageBoxEffect_Implementation(AActor* TargetActor)
{
	DamageBoxEffect(TargetActor);
}

void AGS_TrapBase::DamageBoxEffect_Implementation(AActor* OtherActor)
{
}


void AGS_TrapBase::Server_CustomTrapEffect_Implementation(AActor* TargetActor)
{
	CustomTrapEffect(TargetActor);
}

void AGS_TrapBase::CustomTrapEffect_Implementation(AActor* TargetActor)
{
}

void AGS_TrapBase::Multicast_PlayTrapHitBloodEffect_Implementation(FVector HitLocation)
{
	// 거리/시야 체크 (오디오와 동일한 로직 활용)
	if (!ShouldPlayTrapSoundAtLocation(HitLocation))
		return;

	// 발동 쿨다운 체크
	const double CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastBloodVFXTime < BloodVFXCooldown)
	{
		return;
	}
	LastBloodVFXTime = CurrentTime;

	// 카메라 거리 체크
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			const FVector CameraLocation = CameraManager->GetCameraLocation();
			const float DistanceSquared = FVector::DistSquared(HitLocation, CameraLocation);
			const float MaxDistanceSquared = FMath::Square(GS_Rendering::BLOOD_VFX_MAX_DISTANCE);

			if (DistanceSquared > MaxDistanceSquared)
			{
				return;
			}
		}
	}

	UNiagaraSystem* BloodVFX = CachedTrapHitBloodEffect.Get();
	if (!BloodVFX)
	{
		BloodVFX = UGS_AssetLoader::SyncLoadAsset(TrapData.TrapHitBloodEffect);
	}

	if (BloodVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), BloodVFX, HitLocation, FRotator::ZeroRotator, FVector(1.0f));
	}
}

//플레이어가 안에 있는 경우 밀쳐내는 함수
void AGS_TrapBase::PushCharacterInBox(UBoxComponent* CollisionBox, float PushPower)
{
	if (!CollisionBox)
		return;

	TArray<AActor*> OverlappingActors;
	CollisionBox->GetOverlappingActors(OverlappingActors, ACharacter::StaticClass());

	for (AActor* Actor : OverlappingActors)
	{
		AGS_Character* Character = Cast<AGS_Character>(Actor);
		if (Character)
		{
			// 궁극기 사용 중이거나 무적 상태일 때는 밀쳐내기 무시
			if (Character->IsInvincible())
				continue;

			if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(Character))
			{
				if (Seeker->GetSkillComp() && Seeker->GetSkillComp()->IsSkillActive(ESkillSlot::Ultimate))
				{
					continue;
				}
			}

			FVector LocalCharacterLocation = GetActorTransform().InverseTransformPosition(Character->GetActorLocation());
			FVector PushDirection = (LocalCharacterLocation.Y >= 0.0f)
			                            ? GetActorRightVector()
			                            : -GetActorRightVector();

			if (IsBlockedInDirection(Character->GetActorLocation(), PushDirection, 100.0f, Character))
			{
				PushDirection *= -1.0f;
			}

			PushDirection.Z = 0.0f;
			PushDirection = PushDirection.GetSafeNormal();

			FVector LaunchVelocity = PushDirection * PushPower + FVector(0, 0, 200.0f);

			Character->LaunchCharacter(LaunchVelocity, true, true);
		}
	}
}

bool AGS_TrapBase::IsBlockedInDirection(const FVector& Start, const FVector& Direction, float Distance, AGS_Character* CharacterToIgnore)
{
	FHitResult HitResult;
	FVector End = Start + Direction * Distance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (CharacterToIgnore)
	{
		Params.AddIgnoredActor(CharacterToIgnore);
	}

	if (UWorld* World = GetWorld())
	{
		return World->LineTraceSingleByChannel(HitResult, Start, End, ECC_WorldStatic, Params);
	}

	return false;
}

//Trap Motion
AGS_TrapManager* AGS_TrapBase::GetTrapManager() const
{
	if (CachedTrapManager.IsValid())
	{
		return CachedTrapManager.Get();
	}

	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			if (AGS_TrapManager* TrapManager = Registry->GetTrapManager())
			{
				CachedTrapManager = TrapManager;
				return TrapManager;
			}
		}
	}

	return nullptr;
}

UGS_TrapMotionCompBase* AGS_TrapBase::GetValidMotionComponent() const
{
	TArray<UActorComponent*> Components;
	GetComponents(Components);

	bool bFoundAnyMotionComp = false;

	for (UActorComponent* Comp : Components)
	{
		if (UGS_TrapMotionCompBase* MotionComp = Cast<UGS_TrapMotionCompBase>(Comp))
		{
			/*UE_LOG(LogTemp, Warning, TEXT("[Trap: %s] MotionComp exists : %s / Active: %s"),
				*GetName(), *MotionComp->GetName(), MotionComp->IsActive() ? TEXT("True") : TEXT("False"));*/
			return MotionComp;
		}
	}
	//UE_LOG(LogTemp, Warning, TEXT("[Trap: %s] MotionComp does not exist at all"), *GetName());
	return nullptr;
}

bool AGS_TrapBase::CanStartMotion() const
{
	return true;
}

//void AGS_TrapBase::ClearDotTimerForActor(AActor* Actor)
//{
//	if (!Actor)
//	{
//		return;
//	}
//	FTimerHandle TimerHandle;
//	if (ActiveDoTTimers.RemoveAndCopyValue(Actor, TimerHandle))
//	{
//		GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
//		UE_LOG(LogTemp, Warning, TEXT("DoT Timer Successfully ended - Actor: %s"), *GetNameSafe(Actor));
//	}
//	else
//	{
//		UE_LOG(LogTemp, Warning, TEXT("DoT Timer failed to end - Actor: %s (no timer)"), *GetNameSafe(Actor));
//	}
//
//}

//void AGS_TrapBase::ApplyDotDamage(AActor* DamagedActor)
//{
//	if (!DamagedActor || !HasAuthority() || !TrapData.Effect.bDoT)
//	{
//		return;
//	}
//
//	if (ActiveDoTTimers.Contains(DamagedActor))
//	{
//		ClearDotTimerForActor(DamagedActor);
//	}
//
//	int32 CurrentTick = 0;
//	FTimerHandle TimerHandle;
//
//	TWeakObjectPtr<AActor> WeakActor = DamagedActor;
//
//	FTimerDelegate Delegate;
//	//타이머가 끝나면 실행되는 람다
//	Delegate.BindLambda([=, this]() mutable
//		{
//			if (!WeakActor.IsValid() || !TrapData.Effect.bDoT)
//			{
//				ClearDotTimerForActor(WeakActor.Get());
//				return;
//			}
//
//			AActor* ValidActor = WeakActor.Get();
//
//			if (!ValidActor || !IsValid(ValidActor))
//			{
//				ClearDotTimerForActor(ValidActor);
//				return;
//			}
//			FDamageEvent DamageEvent;
//			ValidActor->TakeDamage(TrapData.Effect.Damage, DamageEvent, nullptr, this);
//			UE_LOG(LogTemp, Warning, TEXT("CurrentTick : %d"), CurrentTick);
//			CurrentTick++;
//
//
//
//			if (CurrentTick >= TrapData.Effect.DamageCount)
//			{
//				////current tick이 damage count보다 같거나 크다면 타이머 초기화 후 ActiveDoTTimers 맵에서 제거
//				//if (ActiveDoTTimers.Contains(ValidActor))
//				//{
//				//	GetWorld()->GetTimerManager().ClearTimer(ActiveDoTTimers[ValidActor]);
//				//
//				//	//크래시 지점
//				//	ActiveDoTTimers.Remove(ValidActor);
//				//	//
//				//}
//
//				ClearDotTimerForActor(ValidActor);
//			}
//		});
//
//	GetWorld()->GetTimerManager().SetTimer(TimerHandle, Delegate, TrapData.Effect.DamageInterval, true);
//	ActiveDoTTimers.Add(DamagedActor, TimerHandle);
//}

// ===================
// Audio Functions Implementation
// ===================

bool AGS_TrapBase::IsRTSMode() const
{
	if (!GetWorld())
	{
		return false;
	}

	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!LocalPC)
	{
		return false;
	}

	return Cast<AGS_RTSController>(LocalPC) != nullptr;
}


bool AGS_TrapBase::ShouldPlayTrapSoundAtLocation(const FVector& TrapLocation) const
{
	// 월드 유효성 체크
	if (!GetWorld() || !IsValid(this))
	{
		return false;
	}

	// 위치 검증 (NaN/Infinity 체크)
	if (!UGS_AudioComponentBase::IsLocationValid(TrapLocation))
	{
		return false;
	}

	// 플레이어 컨트롤러 가져오기
	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!LocalPC || !IsValid(LocalPC))
	{
		return false;
	}

	// 리스너 위치와 카메라 회전 정보 가져오기
	FVector ListenerLocation;
	FRotator CameraRotation;

	if (LocalPC->PlayerCameraManager && IsValid(LocalPC->PlayerCameraManager))
	{
		ListenerLocation = LocalPC->PlayerCameraManager->GetCameraLocation();
		CameraRotation = LocalPC->PlayerCameraManager->GetCameraRotation();
	}
	else if (APawn* PlayerPawn = LocalPC->GetPawn())
	{
		if (IsValid(PlayerPawn))
		{
			ListenerLocation = PlayerPawn->GetActorLocation();
			CameraRotation = PlayerPawn->GetActorRotation();
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	// 리스너 위치 검증
	if (!UGS_AudioComponentBase::IsLocationValid(ListenerLocation))
	{
		return false;
	}

	const float DistanceToListener = FVector::Dist(TrapLocation, ListenerLocation);

	// 매우 가까우면 항상 재생 (모든 방향에서)
	if (DistanceToListener <= 800.0f) // 8m
	{
		return true;
	}

	// 모드별 거리 및 방향 체크
	const bool bRTS = IsRTSMode();

	if (bRTS)
	{
		// RTS 모드: 거리만 체크 (200m)
		return DistanceToListener <= 20000.0f;
	}
	else
	{
		// TPS 모드: 카메라 방향 체크 추가
		const FVector CameraForwardVector = CameraRotation.Vector();
		const FVector DirectionToSource = (TrapLocation - ListenerLocation).GetSafeNormal();

		// 카메라 정면 방향과의 내적 (앞/뒤 판단)
		const float DotProduct = FVector::DotProduct(DirectionToSource, CameraForwardVector);

		// 상하 방향 벡터 계산 (천장/바닥 함정 판별)
		const FVector UpVector = FVector::UpVector;
		const float VerticalDot = FVector::DotProduct(DirectionToSource, UpVector);

		// 높이 차이 계산
		const float HeightDifference = TrapLocation.Z - ListenerLocation.Z;
		const float AbsHeightDiff = FMath::Abs(HeightDifference);

		// 수평 거리 계산 (Z축 제외)
		const FVector HorizontalTrapLocation(TrapLocation.X, TrapLocation.Y, ListenerLocation.Z);
		const float HorizontalDistance = FVector::Dist(HorizontalTrapLocation, ListenerLocation);

		// 천장/바닥 함정 판별
		// VerticalDot > 0.5f: 소리가 위쪽에서 옴 (천장 함정)
		// VerticalDot < -0.5f: 소리가 아래쪽에서 옴 (바닥 함정)
		const bool bIsCeilingSound = (VerticalDot > 0.3f) && (HeightDifference > 100.0f); // 1m 이상 위쪽
		const bool bIsFloorSound = (VerticalDot < -0.3f) && (HeightDifference < -100.0f); // 1m 이상 아래쪽
		const bool bIsVerticalSound = bIsCeilingSound || bIsFloorSound;

		// 천장/바닥 함정 특별 처리 (경고/활성화 사운드)
		if (bRelaxDirectionFilterForWarning && bIsVerticalSound)
		{
			// 천장/바닥 함정의 경우 방향 필터링 완화
			// 수평 거리가 가까우면 카메라 방향과 무관하게 들리도록
			if (HorizontalDistance <= 1500.0f) // 수평 거리 15m 이내
			{
				const bool bPlaySound = DistanceToListener <= ActivationSoundMaxDistance;
#if WITH_EDITOR
				if (!bPlaySound)
				{
					UE_LOG(LogTemp, Verbose, TEXT("[TrapBase] Vertical sound blocked - Distance: %.1f, MaxDistance: %.1f, Height: %.1f, HorizontalDist: %.1f"),
					       DistanceToListener, ActivationSoundMaxDistance, HeightDifference, HorizontalDistance);
				}
#endif
				return bPlaySound;
			}

			// 수평 거리가 멀어도 카메라가 소리 쪽을 향하고 있으면 허용
			if (DotProduct > -0.3f) // 약 107도 범위
			{
				const bool bPlaySound = DistanceToListener <= ActivationSoundMaxDistance;
				return bPlaySound;
			}
		}

		// 일반적인 필터링: 카메라 뒤쪽 필터링 (옆쪽 사각지대 포함)
		if (DotProduct < 0.0f)
		{
			return false; // 카메라 뒤의 소리는 안 들림
		}

		// 카메라 앞쪽이면 거리 체크 (20m)
		return DistanceToListener <= 2000.0f;
	}
}

void AGS_TrapBase::SetTrapAkComponent(UAkComponent* NewAkComponent)
{
	// 유효성 체크 (서버 안정성)
	if (!IsValid(NewAkComponent))
	{
		return;
	}

	TrapAkComponent = NewAkComponent;
	RefreshTrapAudioSetup(false);
}

void AGS_TrapBase::PlayActivationSound()
{
	if (!HasAuthority())
	{
		return;
	}

	Multicast_PlayTrapSound(ETrapSoundType::Activation);
}

void AGS_TrapBase::PlayDeactivationSound()
{
	if (!HasAuthority())
	{
		return;
	}

	Multicast_PlayTrapSound(ETrapSoundType::Deactivation);
}

void AGS_TrapBase::PlayHitSound()
{
	if (!HasAuthority())
	{
		return;
	}

	Multicast_PlayTrapSound(ETrapSoundType::Hit);
}

void AGS_TrapBase::Multicast_PlayTrapSound_Implementation(ETrapSoundType SoundType)
{
	// 데디케이티드 서버에서는 오디오 처리 불필요
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Actor 유효성 체크
	if (!IsValid(this))
	{
		return;
	}

	// 거리 기반 최적화 체크
	if (!ShouldPlayTrapSoundAtLocation(GetActorLocation()))
	{
		return;
	}

	TSoftObjectPtr<UAkAudioEvent> SoundSoftPtr;
	TObjectPtr<UAkAudioEvent> CachedSound;

	bool bIsRTS = IsRTSMode();

	switch (SoundType)
	{
	case ETrapSoundType::Activation:
		SoundSoftPtr = TrapData.ActivationSound;
		CachedSound = CachedActivationSound;
		break;
	case ETrapSoundType::Deactivation:
		SoundSoftPtr = TrapData.DeactivationSound;
		CachedSound = CachedDeactivationSound;
		break;
	case ETrapSoundType::Hit:
		SoundSoftPtr = TrapData.HitSound;
		CachedSound = CachedHitSound;
		break;
	}

	UAkAudioEvent* SoundToPlay = CachedSound ? CachedSound.Get() : UGS_AssetLoader::SyncLoadAsset(SoundSoftPtr);

	if (SoundToPlay)
	{
		// TrapAkComponent가 있으면 해당 컴포넌트(위치)에서 재생, 없으면 현재 위치에서 재생
		if (IsValid(TrapAkComponent))
		{
			// RTS 모드에 따른 Attenuation Scaling 적용
			TrapAkComponent->SetAttenuationScalingFactor(bIsRTS ? 2.0f : 1.0f);

			TrapAkComponent->PostAkEvent(SoundToPlay, 0, FOnAkPostEventCallback());
		}
		else
		{
			// PostEvent(this)를 사용할 경우 내부적으로 AkComponent를 찾거나 생성하므로 스케일링 적용이 어려울 수 있음.
			// 하지만 TrapAkComponent를 사용하는 것이 권장되는 패턴임.
			UAkGameplayStatics::PostEvent(SoundToPlay, this, 0, FOnAkPostEventCallback());
		}
	}
}

void AGS_TrapBase::ApplyDistanceCulling()
{
	// 렌더링이 불가능한 환경(데디서버 등)이면 스킵
	if (!FApp::CanEverRender())
		return;

	const float BaseCullDist = GetTrapCullDistance();
	const float CullDistance = GS_Rendering::CalculateCullDistance(this, BaseCullDist);
	const int32 MinLOD = GS_Rendering::CalculateMinLOD(this);

	// 1. 모든 Primitive 컴포넌트 순회 (메시, 데칼, 이펙트 등)
	TArray<UActorComponent*> AllComponents;
	GetComponents(AllComponents, true);

	// 지오메트리 캐시는 함정 애니메이션 메시이므로 함정 거리와 동일하게 설정
	const float GeoCacheCullDistance = CullDistance;
	const float VFXCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::VFX_DISABLE_DISTANCE);

	int32 ProcessedCount = 0;

	for (UActorComponent* Comp : AllComponents)
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp);
		if (!PrimComp || !IsValid(PrimComp))
			continue;

		// 트리거 및 비가시성 컴포넌트 제외
		if (PrimComp == DamageBoxComp || PrimComp == ActivateSphereComp)
			continue;
		if (PrimComp->ComponentTags.Contains("IgnoreCulling"))
			continue;

		// 이미 숨겨진 컴포넌트도 일단 컬링 거리는 설정 (나중에 보일 때를 대비)

		float TargetDistance = CullDistance;

		// 특수 타입 체크
		if (PrimComp->IsA<UGeometryCacheComponent>())
		{
			TargetDistance = GeoCacheCullDistance;
		}
		else if (PrimComp->IsA<UNiagaraComponent>())
		{
			TargetDistance = VFXCullDistance;
		}

		// 컬링 강제 적용
		PrimComp->bNeverDistanceCull = false;
		PrimComp->SetCullDistance(TargetDistance);
		PrimComp->SetCachedMaxDrawDistance(TargetDistance);
		PrimComp->bAllowCullDistanceVolume = true; // 엔진 컬링 시스템 활용을 위해 true로 복구
		PrimComp->SetBoundsScale(GS_Rendering::DEFAULT_BOUNDS_SCALE);

		// 메시 상세 설정
		if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(PrimComp))
		{
			MeshComp->MinLOD = MinLOD;
			// SetCullDistance 내부에서 MarkRenderStateDirty가 호출됨
		}
		else if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(PrimComp))
		{
			SkelComp->MinLodModel = MinLOD;
			SkelComp->bEnableUpdateRateOptimizations = true;
			SkelComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
		}

		ProcessedCount++;
	}

	// 2. 라이트 최적화
	TArray<ULightComponent*> LightComponents;
	GetComponents<ULightComponent>(LightComponents, true);
	const float LightCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::LIGHT_CULL_DISTANCE);

	for (ULightComponent* LightComp : LightComponents)
	{
		if (IsValid(LightComp))
		{
			LightComp->MaxDrawDistance = LightCullDistance;
			LightComp->MaxDistanceFadeRange = 500.0f;
			if (!LightComp->ComponentHasTag(FName("MainLight")))
			{
				LightComp->SetCastShadows(false);
			}
			LightComp->MarkRenderStateDirty();
		}
	}
}


float AGS_TrapBase::GetTrapCullDistance() const
{
	// PlaceInfoComponent의 CellCoord 크기로 함정 크기 판별
	if (UPlaceInfoComponent* PlaceInfo = GetComponentByClass<UPlaceInfoComponent>())
	{
		int32 CellCount = PlaceInfo->GetCellCoord().Num();

		if (CellCount <= 1)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[Trap:%s] GetTrapCullDistance: SMALL (Cell: %d)"), *GetName(), CellCount);
			return GS_Rendering::TRAP_SMALL_CULL_DISTANCE;
		}
		else if (CellCount <= 4)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[Trap:%s] GetTrapCullDistance: MEDIUM (Cell: %d)"), *GetName(), CellCount);
			return GS_Rendering::TRAP_MEDIUM_CULL_DISTANCE;
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("[Trap:%s] GetTrapCullDistance: LARGE (Cell: %d)"), *GetName(), CellCount);
			return GS_Rendering::TRAP_LARGE_CULL_DISTANCE;
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("[Trap:%s] GetTrapCullDistance: DEFAULT (No PlaceInfo)"), *GetName());

	// PlaceInfo가 없으면 기본값 (중간 크기)
	return GS_Rendering::TRAP_MEDIUM_CULL_DISTANCE;
}

bool AGS_TrapBase::IsEnvironmentHit(AActor* HitActor, UPrimitiveComponent* HitComp) const
{
	if (!IsValid(HitActor))
	{
		return false;
	}

	if (HitComp && IsValid(HitComp))
	{
		const ECollisionChannel Channel = HitComp->GetCollisionObjectType();
		return Channel == ECC_WorldStatic || Channel == ECC_WorldDynamic;
	}

	// RootComponent 체크
	if (const UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(HitActor->GetRootComponent()))
	{
		if (IsValid(RootPrim))
		{
			const ECollisionChannel Channel = RootPrim->GetCollisionObjectType();
			return Channel == ECC_WorldStatic || Channel == ECC_WorldDynamic;
		}
	}

	return false;
}

void AGS_TrapBase::RegisterSignificanceManager()
{
	if (USignificanceManager* SM = USignificanceManager::Get(GetWorld()))
	{
		TWeakObjectPtr<AGS_TrapBase> WeakThis(this);
		SM->RegisterObject(
		    this, "Trap",
		    [WeakThis](USignificanceManager::FManagedObjectInfo* ObjectInfo, const FTransform& Viewpoint) -> float
		    {
			    if (AGS_TrapBase* StrongThis = WeakThis.Get())
				    return StrongThis->CalculateSignificance(Viewpoint);
			    return 0.0f;
		    },
		    USignificanceManager::EPostSignificanceType::Sequential,
		    [WeakThis](USignificanceManager::FManagedObjectInfo* ObjectInfo, float OldValue, float NewValue, bool bExternal)
		    {
			    if (AGS_TrapBase* StrongThis = WeakThis.Get())
				    StrongThis->OnSignificanceChanged(NewValue);
		    });
	}
}

float AGS_TrapBase::CalculateSignificance(const FTransform& Viewpoint)
{
	if (bIsActivated)
		return 1.0f; // 활성화된 함정은 최상위 중요도

	float DistSq = FVector::DistSquared(GetActorLocation(), Viewpoint.GetLocation());
	const float FinalCullDistance = GS_Rendering::CalculateCullDistance(this, GetTrapCullDistance());

	// 컬링 거리의 1.1배를 기준으로 0.1~1.0 사이 점수 계산
	float MaxDistSq = FMath::Square(FinalCullDistance * 1.1f);
	return FMath::Clamp(1.0f - (DistSq / MaxDistSq), 0.1f, 1.0f);
}

void AGS_TrapBase::OnSignificanceChanged(float NewSignificance)
{
	CurrentSignificance = NewSignificance;

	// ========================================
	// [핵심 최적화] 중요도가 낮으면 함정 전체 숨김
	// 단, 활성화된 함정은 항상 표시 (플레이어와 상호작용 중)
	// 임계값 0.4 = Small 함정 기준 약 44m 이상 떨어지면 숨김
	// ========================================
	static constexpr float VISIBILITY_THRESHOLD = 0.4f;

	if (!bIsActivated && NewSignificance < VISIBILITY_THRESHOLD)
	{
		// 함정 전체 비활성화 (Draw Call 완전 제거)
		SetActorHiddenInGame(true);

		// 그림자 컬링 타이머 정지 (숨겨진 함정은 연산 불필요)
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(ShadowCullingTimerHandle);
		}
	}
	else
	{
		// 함정 다시 표시
		SetActorHiddenInGame(false);

		// 중요도에 따라 타이머 주기 동적 변경 (0.1s ~ 1.0s)
		const float NewInterval = GS_Rendering::GetAdaptiveTimerInterval(NewSignificance);

		if (GetWorld())
		{
			const float RemainingTime = GetWorld()->GetTimerManager().GetTimerRemaining(ShadowCullingTimerHandle);
			GetWorld()->GetTimerManager().SetTimer(
			    ShadowCullingTimerHandle, this, &AGS_TrapBase::UpdateShadowCulling,
			    NewInterval, true, FMath::Max(0.01f, RemainingTime));
		}
	}
}

void AGS_TrapBase::CacheOptimizedComponents()
{
	CachedPrimitiveComponents.Empty();
	CachedLightComponents.Empty();
	CachedNiagaraComponents.Empty();

	TArray<UActorComponent*> AllComponents;
	GetComponents(AllComponents, true);

	for (UActorComponent* Comp : AllComponents)
	{
		if (!IsValid(Comp))
			continue;

		if (Comp->ComponentTags.Contains("IgnoreCulling"))
			continue;

		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
		{
			// 트리거는 가시성과 관계없이 항상 활성화 유지
			if (PrimComp == DamageBoxComp || PrimComp == ActivateSphereComp)
				continue;

			CachedPrimitiveComponents.Add(PrimComp);
		}
		else if (ULightComponent* LightComp = Cast<ULightComponent>(Comp))
		{
			CachedLightComponents.Add(LightComp);
		}
		else if (UNiagaraComponent* NiagaraComp = Cast<UNiagaraComponent>(Comp))
		{
			CachedNiagaraComponents.Add(NiagaraComp);
		}
	}
}

void AGS_TrapBase::UpdateShadowCulling()
{
	if (IsRunningDedicatedServer())
		return;

	if (!GetWorld())
		return;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
		return;

	FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
	float DistSq = FVector::DistSquared(GetActorLocation(), CameraLoc);

	const float BaseCullDist = GetTrapCullDistance();
	const float CullDistance = GS_Rendering::CalculateCullDistance(this, BaseCullDist);

	// 거리 제곱 임계값 캐싱 (1.21 = 1.1^2, 10% 여유치)
	const float CullDistSqThreshold = (CullDistance * CullDistance) * 1.21f;

	// 가시성 판단 (나나이트는 엔진 컬링이 미흡하므로 수동 제어)
	bool bShouldBeVisible = DistSq < CullDistSqThreshold;

	const bool bIsRTSMode = GS_Rendering::IsRTSMode(this);

	// === Primitive 컴포넌트 처리 ===
	for (TObjectPtr<UPrimitiveComponent> PrimComp : CachedPrimitiveComponents)
	{
		if (!PrimComp || !IsValid(PrimComp))
			continue;

		// 액터 위치가 아닌 개별 컴포넌트 위치 기준으로 거리 계산
		const float CompDistSq = FVector::DistSquared(PrimComp->GetComponentLocation(), CameraLoc);
		bool bCompShouldBeVisible = CompDistSq < CullDistSqThreshold;

		// 지오메트리 캐시의 경우 에셋이 없으면 렌더링 상태 업데이트 시 크래시 위험이 있음
		if (UGeometryCacheComponent* GeoComp = Cast<UGeometryCacheComponent>(PrimComp))
		{
			if (!GeoComp->GetGeometryCache())
				continue;
		}

		// 태그 기반 가시성 판단
		if (bCompShouldBeVisible)
		{
			if (PrimComp->ComponentTags.Contains(FName("Hidden")) ||
			    (bIsRTSMode && PrimComp->ComponentTags.Contains(FName("RTS"))))
			{
				bCompShouldBeVisible = false;
			}
		}

		// 1. 가시성 업데이트
		if (PrimComp->GetVisibleFlag() != bCompShouldBeVisible)
		{
			PrimComp->SetVisibility(bCompShouldBeVisible);
		}

		// 2. 그림자 업데이트 (보일 때만)
		if (bCompShouldBeVisible)
		{
			GS_Rendering::UpdateShadowCulling(this, PrimComp);
		}
	}

	// === Light 컴포넌트 처리 ===
	for (TObjectPtr<ULightComponent> LightComp : CachedLightComponents)
	{
		if (!LightComp || !IsValid(LightComp))
			continue;

		// 태그 기반 가시성 판단
		bool bLightShouldBeVisible = bShouldBeVisible;
		if (bLightShouldBeVisible)
		{
			if (LightComp->ComponentTags.Contains(FName("Hidden")) ||
			    (bIsRTSMode && LightComp->ComponentTags.Contains(FName("RTS"))))
			{
				bLightShouldBeVisible = false;
			}
		}

		// 1. 가시성 업데이트
		if (LightComp->GetVisibleFlag() != bLightShouldBeVisible)
		{
			LightComp->SetVisibility(bLightShouldBeVisible);
		}

		// 2. 그림자 누수 방지 (근거리에서는 그림자 강제 활성화)
		if (bLightShouldBeVisible)
		{
			bool bNearby = DistSq < (GS_Rendering::DYNAMIC_SHADOW_DISABLE_DISTANCE * GS_Rendering::DYNAMIC_SHADOW_DISABLE_DISTANCE);
			if (LightComp->CastShadows != bNearby)
			{
				LightComp->SetCastShadows(bNearby);
			}
		}
	}

	// === Niagara 컴포넌트 처리 ===
	for (TObjectPtr<UNiagaraComponent> NiagaraComp : CachedNiagaraComponents)
	{
		if (!NiagaraComp || !IsValid(NiagaraComp))
			continue;

		// 태그 기반 가시성 판단
		bool bVfxShouldBeVisible = bShouldBeVisible;
		if (bVfxShouldBeVisible)
		{
			if (NiagaraComp->ComponentTags.Contains(FName("Hidden")) ||
			    (bIsRTSMode && NiagaraComp->ComponentTags.Contains(FName("RTS"))))
			{
				bVfxShouldBeVisible = false;
			}
		}

		if (NiagaraComp->GetVisibleFlag() != bVfxShouldBeVisible)
		{
			NiagaraComp->SetVisibility(bVfxShouldBeVisible);
		}
	}

	// === [디버그] 오클루전 디버그 라인 그리기 ===
	// GS.Audio.ShowOcclusionRay 1 명령어 활성화 시 표시됨
	if (bShouldBeVisible && IsValid(TrapAkComponent))
	{
		// 이전에 상단에서 선언된 PC 변수를 그대로 사용
		if (PC && PC->PlayerCameraManager)
		{
			FVector ListenerLoc = PC->PlayerCameraManager->GetCameraLocation();
			// DrawOcclusionDebug 내부에서 직접 LineTrace를 수행하여 색상을 결정함
			UGS_AudioComponentBase::DrawOcclusionDebug(this, TrapAkComponent->GetComponentLocation(), ListenerLoc);
		}
	}
}
