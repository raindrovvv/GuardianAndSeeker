#include "Props/Interactables/GS_Door.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Net/UnrealNetwork.h"
#include "AI/RTS/GS_RTSController.h"
#include "AkComponent.h"
#include "AkAudioDevice.h"
#include "AkAudioEvent.h"
#include "AkGameplayStatics.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/GS_AudioComponentBase.h"
#include "Rendering/GS_RenderingConstants.h"
#include "NiagaraComponent.h"
#include "Components/LightComponent.h"
#include "SignificanceManager.h"
#include "Engine/World.h"
#include "Misc/App.h"


AGS_Door::AGS_Door()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;

	bReplicates = true;
	SetReplicateMovement(true);

	RootSceneComp = CreateDefaultSubobject<UBoxComponent>(TEXT("RootSceneComp"));
	RootComponent = RootSceneComp;

	DoorFrameMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorFrameMeshComp"));
	DoorFrameMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DoorFrameMeshComp->SetCollisionObjectType(ECC_WorldStatic);
	DoorFrameMeshComp->SetCollisionResponseToAllChannels(ECR_Block);
	DoorFrameMeshComp->SetupAttachment(RootComponent);
	DoorFrameMeshComp->SetCastShadow(true); // 빛 누수 방지를 위해 그림자 필수
	DoorFrameMeshComp->bUseAsOccluder = true; // 문 프레임은 강력한 차단벽
	DoorFrameMeshComp->PrimaryComponentTick.bCanEverTick = false;
	DoorFrameMeshComp->PrimaryComponentTick.bStartWithTickEnabled = false;
	DoorFrameMeshComp->PrimaryComponentTick.bAllowTickOnDedicatedServer = false;

	DoorMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMeshComp"));
	DoorMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DoorMeshComp->SetCollisionObjectType(ECC_WorldStatic);
	DoorMeshComp->SetCollisionResponseToAllChannels(ECR_Block);
	DoorMeshComp->SetupAttachment(DoorFrameMeshComp);
	// 문이 닫혀있어도 AI가 경로를 찾을 수 있도록 DoorMeshComp가 NavMesh에 영향을 주지 않도록 설정
	DoorMeshComp->SetCanEverAffectNavigation(false);
	DoorMeshComp->SetCastShadow(true); // 문이 닫혔을 때 빛을 차단해야 함
	DoorMeshComp->bUseAsOccluder = true; // 문 메시 자체가 차단벽 역할 수행
	DoorMeshComp->PrimaryComponentTick.bCanEverTick = false;
	DoorMeshComp->PrimaryComponentTick.bStartWithTickEnabled = false;
	DoorMeshComp->PrimaryComponentTick.bAllowTickOnDedicatedServer = false;

	TriggerBoxComp = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBoxComp->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBoxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBoxComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBoxComp->SetupAttachment(DoorFrameMeshComp);
	TriggerBoxComp->SetGenerateOverlapEvents(true);
	TriggerBoxComp->PrimaryComponentTick.bCanEverTick = false;
	TriggerBoxComp->PrimaryComponentTick.bStartWithTickEnabled = false;
	TriggerBoxComp->PrimaryComponentTick.bAllowTickOnDedicatedServer = false;

	AudioAnchorComponent = CreateDefaultSubobject<USceneComponent>(TEXT("AudioAnchor"));
	AudioAnchorComponent->SetupAttachment(RootComponent);
	AudioAnchorComponent->PrimaryComponentTick.bCanEverTick = false;
	AudioAnchorComponent->PrimaryComponentTick.bStartWithTickEnabled = false;
	AudioAnchorComponent->PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	AudioAnchorComponent->SetRelativeLocation(AudioAnchorRelativeLocation);

	// AkComponent는 기본적으로 생성하지 않음 (BP에서 선택적으로 추가)
	DoorAkComponent = nullptr;
}


void AGS_Door::BeginPlay()
{
	Super::BeginPlay();

	RefreshDoorAudioSetup(true);

	InitDoor();
	TriggerBoxComp->OnComponentBeginOverlap.AddDynamic(this, &AGS_Door::OnTriggerBeginOverlap);

	// === Culling 설정 (클라이언트만) ===
	if (!IsRunningDedicatedServer())
	{
		ApplyDistanceCulling();
		RegisterSignificanceManager();

		// 그림자 및 가시성 컬링 타이머 (중요도 시스템에 의해 관리됨)
		float RandomVariance = FMath::RandRange(0.0f, 0.5f);
		GetWorld()->GetTimerManager().SetTimer(ShadowCullingTimerHandle, this, &AGS_Door::UpdateCulling, 0.5f, true, RandomVariance);

		// 초기 1회 즉시 실행
		CacheOptimizedComponents();
		UpdateCulling();
	}
}

void AGS_Door::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Significance Manager 해제
	if (!IsRunningDedicatedServer() && GetWorld())
	{
		if (USignificanceManager* SM = USignificanceManager::Get(GetWorld()))
		{
			SM->UnregisterObject(this);
		}
	}

	// 타이머 정리 (레벨 전환 안정성)
	SafeClearTimer(DoorCloseTimerHandle);
	SafeClearTimer(ShadowCullingTimerHandle);

	// 델리게이트 해제 (객체 파괴 시 안정성)
	if (TriggerBoxComp)
	{
		TriggerBoxComp->OnComponentBeginOverlap.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AGS_Door::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Transform 검증 (서버 안정성)
	const FRotator TransformRotation = Transform.GetRotation().Rotator();
	if (!UGS_AudioComponentBase::IsTransformValid(Transform.GetLocation(), TransformRotation))
	{
		return;
	}

	RefreshDoorAudioSetup(true);
}

void AGS_Door::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                     UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                     bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this)
	{
		return;
	}
	//UE_LOG(LogTemp, Warning, TEXT("Something overlapped: %s"), *OtherActor->GetName());

	AGS_Character* Character = Cast<AGS_Character>(OtherActor);
	if (!Character || !HasAuthority())
	{
		//UE_LOG(LogTemp, Warning, TEXT("Overlap but not AGS_Character: %s"), *OtherActor->GetName());
		return;
	}
	if (!bIsOpen)
	{
		bIsOpen = true;
		Server_DoorOpen(Character);
	}
}


void AGS_Door::Server_DoorOpen_Implementation(AActor* TargetActor)
{
	// 문 사운드 먼저 재생 (BP 오버라이드 여부와 무관하게 항상 재생)
	PlayOpenSound();

	//문 열리는 함수 호출하고
	DoorOpen();

	// 안전한 타이머 관리
	SafeClearTimer(DoorCloseTimerHandle);

	if (IsWorldContextValid())
	{
		GetWorldTimerManager().SetTimer(DoorCloseTimerHandle, this, &AGS_Door::CheckForPlayerInTrigger, 2.0f, false);
	}
}

void AGS_Door::CheckForPlayerInTrigger()
{
	if (!IsWorldContextValid())
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	TriggerBoxComp->GetOverlappingActors(OverlappingActors, AGS_Character::StaticClass());

	if (OverlappingActors.Num() > 0)
	//오버랩 되는 엑터가 있으면 타이머 초기화
	{
		if (IsWorldContextValid())
		{
			GetWorldTimerManager().SetTimer(DoorCloseTimerHandle, this, &AGS_Door::CheckForPlayerInTrigger, 2.0f, false);
		}
	}
	else
	{
		// 문 사운드 먼저 재생 (BP 오버라이드 여부와 무관하게 항상 재생)
		PlayCloseSound();

		//오버랩 되는 엑터가 없으면 문 닫히는 함수 호출
		DoorClose();
		bIsOpen = false;
	}
}

void AGS_Door::DoorOpen_Implementation()
{
	// 사운드는 Server_DoorOpen에서 이미 재생됨
	// BP에서 문 여는 애니메이션 등 구현
}

void AGS_Door::DoorClose_Implementation()
{
	// 사운드는 CheckForPlayerInTrigger에서 이미 재생됨
	// BP에서 문 닫는 애니메이션 등 구현
}

// ===============================
// Audio Functions Implementation
// ===============================

bool AGS_Door::IsRTSMode() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	APlayerController* LocalPC = World->GetFirstPlayerController();
	if (!LocalPC)
	{
		return false;
	}

	// 클래스 비교를 통한 최적화 (Cast 대비 오버헤드 감소)
	return LocalPC->IsA<AGS_RTSController>();
}


bool AGS_Door::ShouldPlayDoorSoundAtLocation(const FVector& DoorLocation) const
{
	// 월드 유효성 체크
	if (!GetWorld() || !IsValid(this))
	{
		return false;
	}

	// 위치 검증 (NaN/Infinity 체크)
	if (!UGS_AudioComponentBase::IsLocationValid(DoorLocation))
	{
		return false;
	}

	// 플레이어 컨트롤러 가져오기
	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!LocalPC || !IsValid(LocalPC))
	{
		return false;
	}

	// 리스너 위치 가져오기
	FVector ListenerLocation;

	if (LocalPC->PlayerCameraManager && IsValid(LocalPC->PlayerCameraManager))
	{
		ListenerLocation = LocalPC->PlayerCameraManager->GetCameraLocation();
	}
	else if (APawn* PlayerPawn = LocalPC->GetPawn())
	{
		if (IsValid(PlayerPawn))
		{
			ListenerLocation = PlayerPawn->GetActorLocation();
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

	const float DistanceToListener = FVector::Dist(DoorLocation, ListenerLocation);

	// 매우 가까우면 항상 재생
	if (DistanceToListener <= 800.0f) // 8m
	{
		return true;
	}

	// 모드별 거리 체크
	const bool bRTS = IsRTSMode();

	if (bRTS)
	{
		// RTS 모드: 거리만 체크 (200m)
		const bool bShouldPlay = DistanceToListener <= 20000.0f;
		return bShouldPlay;
	}
	else
	{
		// TPS 모드: 거리 체크 (최대 거리 사용)
		const bool bShouldPlay = DistanceToListener <= DoorSoundMaxDistance;
		return bShouldPlay;
	}
}

void AGS_Door::SetDoorAkComponent(UAkComponent* NewAkComponent)
{
	// 유효성 체크 (서버 안정성)
	if (!IsValid(NewAkComponent))
	{
		return;
	}

	DoorAkComponent = NewAkComponent;
	RefreshDoorAudioSetup(false);
}

void AGS_Door::PlayOpenSound()
{
	if (!HasAuthority())
	{
		return;
	}

	UAkAudioEvent* SoundEvent = UGS_AssetLoader::SyncLoadAsset(OpenSound);

	if (SoundEvent)
	{
		Multicast_PlayOpenSound();
	}
}

void AGS_Door::PlayCloseSound()
{
	if (!HasAuthority())
	{
		return;
	}

	UAkAudioEvent* SoundEvent = UGS_AssetLoader::SyncLoadAsset(CloseSound);
	if (SoundEvent)
	{
		Multicast_PlayCloseSound();
	}
}

void AGS_Door::Multicast_PlayOpenSound_Implementation()
{
	// 데디케이티드 서버에서는 오디오 처리 불필요
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Actor 유효성 체크 (서버 안정성)
	if (!IsValid(this))
	{
		return;
	}

	// 거리 기반 최적화 체크
	const bool bShouldPlay = ShouldPlayDoorSoundAtLocation(GetActorLocation());
	if (!bShouldPlay)
	{
		return;
	}

	const bool bIsRTS = IsRTSMode();
	UAkAudioEvent* SoundEvent = UGS_AssetLoader::SyncLoadAsset(OpenSound);

	if (SoundEvent)
	{
		// DoorAkComponent가 있으면 AudioAnchor 위치에서 재생, 없으면 Actor 자체 사용
		if (IsValid(DoorAkComponent))
		{
			// 동적 거리 감쇠 설정 (Wwise Attenuation Scaling Factor)
			DoorAkComponent->SetAttenuationScalingFactor(bIsRTS ? 2.0f : 1.0f);
			const AkPlayingID PlayingID = DoorAkComponent->PostAkEvent(SoundEvent, 0, FOnAkPostEventCallback());
		}
		else
		{
			const AkPlayingID PlayingID = UAkGameplayStatics::PostEvent(SoundEvent, this, 0, FOnAkPostEventCallback());
		}
	}
}

void AGS_Door::Multicast_PlayCloseSound_Implementation()
{
	// 데디케이티드 서버에서는 오디오 처리 불필요
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Actor 유효성 체크 (서버 안정성)
	if (!IsValid(this))
	{
		return;
	}

	// 거리 기반 최적화 체크
	if (!ShouldPlayDoorSoundAtLocation(GetActorLocation()))
	{
		return;
	}

	const bool bIsRTS = IsRTSMode();
	UAkAudioEvent* SoundEvent = UGS_AssetLoader::SyncLoadAsset(CloseSound);
	if (SoundEvent)
	{
		// DoorAkComponent가 있으면 AudioAnchor 위치에서 재생, 없으면 Actor 자체 사용
		if (IsValid(DoorAkComponent))
		{
			// 동적 거리 감쇠 설정 (Wwise Attenuation Scaling Factor)
			DoorAkComponent->SetAttenuationScalingFactor(bIsRTS ? 2.0f : 1.0f);
			DoorAkComponent->PostAkEvent(SoundEvent, 0, FOnAkPostEventCallback());
		}
		else
		{
			UAkGameplayStatics::PostEvent(SoundEvent, this, 0, FOnAkPostEventCallback());
		}
	}
}

void AGS_Door::RefreshDoorAudioSetup(bool bForceFindComponent)
{
	// World 유효성 체크 (서버 안정성)
	if (!GetWorld() || !IsValid(this))
	{
		return;
	}

	// === 데디케이티드 서버 크래시 방지 ===
	// BP에서 추가된 AkComponent가 리스너 없는 서버에서 Tick하면 크래시 발생
	if (IsRunningDedicatedServer() || GetNetMode() == NM_DedicatedServer)
	{
		DoorAkComponent = FindComponentByClass<UAkComponent>();
		if (IsValid(DoorAkComponent))
		{
			DoorAkComponent->Stop();
			DoorAkComponent->SetComponentTickEnabled(false);
			DoorAkComponent->UnregisterComponent();
			DoorAkComponent->DestroyComponent();
			DoorAkComponent = nullptr;
		}
		return; // 서버에서는 오디오 설정 중단
	}

	if (IsValid(AudioAnchorComponent))
	{
		AudioAnchorComponent->SetRelativeLocation(AudioAnchorRelativeLocation);
	}

	if (bForceFindComponent || DoorAkComponent == nullptr)
	{
		DoorAkComponent = FindComponentByClass<UAkComponent>();
	}

	if (!IsValid(DoorAkComponent))
	{
		return;
	}

	DoorAkComponent->SetComponentTickEnabled(false);
	AttachDoorAkComponentToAnchor();
}

void AGS_Door::AttachDoorAkComponentToAnchor()
{
	if (!IsValid(DoorAkComponent))
	{
		return;
	}

	// World 유효성 체크 (서버 안정성)
	if (!GetWorld() || !IsValid(this))
	{
		return;
	}

	// 오클루전 완전 비활성화 (방 모듈에 의한 소리 차단 방지)
	DoorAkComponent->OcclusionRefreshInterval = 0.0f;
	DoorAkComponent->EnableSpotReflectors = false;

	if (bUseAudioAnchor && IsValid(AudioAnchorComponent))
	{
		const FVector AnchorLocation = AudioAnchorComponent->GetComponentLocation();
		const FRotator AnchorRotation = AudioAnchorComponent->GetComponentRotation();

		// Transform 검증 (NaN/Infinity 체크)
		if (!UGS_AudioComponentBase::IsTransformValid(AnchorLocation, AnchorRotation))
		{
			return;
		}

		DoorAkComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		DoorAkComponent->AttachToComponent(AudioAnchorComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		DoorAkComponent->SetRelativeLocation(FVector::ZeroVector);
		DoorAkComponent->SetRelativeRotation(FRotator::ZeroRotator);
		DoorAkComponent->SetRelativeScale3D(FVector::OneVector);
		DoorAkComponent->SetWorldLocation(AnchorLocation);
		DoorAkComponent->SetWorldRotation(AnchorRotation);
	}
	else if (IsValid(RootComponent))
	{
		DoorAkComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		DoorAkComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	}
}

// ===================
// Timer Safety Functions
// ===================

void AGS_Door::SafeClearTimer(FTimerHandle& TimerHandle)
{
	if (TimerHandle.IsValid())
	{
		UWorld* World = GetWorld();
		if (World && World->IsValidLowLevel() && !World->bIsTearingDown)
		{
			World->GetTimerManager().ClearTimer(TimerHandle);
		}
		TimerHandle.Invalidate();
	}
}

bool AGS_Door::IsWorldContextValid() const
{
	UWorld* World = GetWorld();
	return World &&
	       World->IsValidLowLevel() &&
	       !World->bIsTearingDown &&
	       IsValid(World) &&
	       IsValid(this);
}

void AGS_Door::ApplyDistanceCulling()
{
	if (!FApp::CanEverRender())
		return;

	const float DoorCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::ROOM_CULL_DISTANCE);
	const int32 MinLOD = GS_Rendering::CalculateMinLOD(this);

	TArray<UActorComponent*> AllComponents;
	GetComponents(AllComponents, true);

	for (UActorComponent* Comp : AllComponents)
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp);
		if (!PrimComp || !IsValid(PrimComp))
			continue;

		if (PrimComp == TriggerBoxComp)
			continue;
		if (PrimComp->ComponentTags.Contains("IgnoreCulling"))
			continue;

		PrimComp->bNeverDistanceCull = false;
		PrimComp->SetCullDistance(DoorCullDistance);
		PrimComp->SetCachedMaxDrawDistance(DoorCullDistance);
		PrimComp->bAllowCullDistanceVolume = true;

		if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(PrimComp))
		{
			MeshComp->MinLOD = MinLOD;
		}

		PrimComp->MarkRenderStateDirty();
	}
}

void AGS_Door::CacheOptimizedComponents()
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
			// TriggerBox 제외
			if (PrimComp != TriggerBoxComp)
			{
				CachedPrimitiveComponents.Add(PrimComp);
			}
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

void AGS_Door::UpdateCulling()
{
	if (IsRunningDedicatedServer())
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
		return;

	FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
	float DistSq = FVector::DistSquared(GetActorLocation(), CameraLoc);

	const float DoorCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::ROOM_CULL_DISTANCE);

	// 거리 제곱 임계값 캐싱 (1.21 = 1.1^2, 10% 여유치)
	const float CullDistSqThreshold = (DoorCullDistance * DoorCullDistance) * 1.21f;

	// 나나이트 대응 수동 가시성 제어
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

		// 태그 기반 가시성 판단
		if (bCompShouldBeVisible)
		{
			if (PrimComp->ComponentTags.Contains(FName("Hidden")) ||
			    (bIsRTSMode && PrimComp->ComponentTags.Contains(FName("RTS"))))
			{
				bCompShouldBeVisible = false;
			}
		}

		if (PrimComp->GetVisibleFlag() != bCompShouldBeVisible)
		{
			PrimComp->SetVisibility(bCompShouldBeVisible);
		}

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

		if (LightComp->GetVisibleFlag() != bLightShouldBeVisible)
		{
			LightComp->SetVisibility(bLightShouldBeVisible);
		}

		// 그림자 누수 방지 로직
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

	// === 오클루전 디버그 라인 그리기 ===
	if (bShouldBeVisible && IsValid(DoorAkComponent))
	{
		if (PC && PC->PlayerCameraManager)
		{
			FVector ListenerLoc = PC->PlayerCameraManager->GetCameraLocation();
			UGS_AudioComponentBase::DrawOcclusionDebug(this, DoorAkComponent->GetComponentLocation(), ListenerLoc);
		}
	}
}

void AGS_Door::RegisterSignificanceManager()
{
	if (USignificanceManager* SM = USignificanceManager::Get(GetWorld()))
	{
		TWeakObjectPtr<AGS_Door> WeakThis(this);
		SM->RegisterObject(
		    this, "Door",
		    [WeakThis](USignificanceManager::FManagedObjectInfo* ObjectInfo, const FTransform& Viewpoint) -> float
		    {
			    if (AGS_Door* StrongThis = WeakThis.Get())
				    return StrongThis->CalculateSignificance(Viewpoint);
			    return 0.0f;
		    },
		    USignificanceManager::EPostSignificanceType::Sequential,
		    [WeakThis](USignificanceManager::FManagedObjectInfo* ObjectInfo, float OldValue, float NewValue, bool bExternal)
		    {
			    if (AGS_Door* StrongThis = WeakThis.Get())
				    StrongThis->OnSignificanceChanged(NewValue);
		    });
	}
}

float AGS_Door::CalculateSignificance(const FTransform& Viewpoint)
{
	float DistSq = FVector::DistSquared(GetActorLocation(), Viewpoint.GetLocation());
	const float FinalCullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::ROOM_CULL_DISTANCE);

	// 컬링 거리의 1.2배를 기준으로 0.1~1.0 사이 점수 계산
	float MaxDistSq = FMath::Square(FinalCullDistance * 1.2f);
	return FMath::Clamp(1.0f - (DistSq / MaxDistSq), 0.1f, 1.0f);
}

void AGS_Door::OnSignificanceChanged(float NewSignificance)
{
	CurrentSignificance = NewSignificance;

	// 중요도에 따라 타이머 주기 동적 변경 (0.1s ~ 1.0s)
	float NewInterval = GS_Rendering::GetAdaptiveTimerInterval(NewSignificance);

	if (GetWorld())
	{
		float RemainingTime = GetWorld()->GetTimerManager().GetTimerRemaining(ShadowCullingTimerHandle);
		GetWorld()->GetTimerManager().SetTimer(ShadowCullingTimerHandle, this, &AGS_Door::UpdateCulling, NewInterval, true, FMath::Max(0.01f, RemainingTime));
	}
}
