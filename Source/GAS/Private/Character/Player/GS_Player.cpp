#include "Character/Player/GS_Player.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Character/GS_TpsController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/PostProcessComponent.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Character/Skill/GS_SkillBase.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Controller.h"
#include "System/GS_PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UI/Character/GS_SteamNameWidgetComp.h"
#include "AkAudioDevice.h"
#include "Sound/GS_AudioComponentBase.h"
#include "Components/CapsuleComponent.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"

AGS_Player::AGS_Player(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	SkillComp = ObjectInitializer.CreateDefaultSubobject<UGS_SkillComp>(this, TEXT("SkillComp"));

	SpringArmComp = ObjectInitializer.CreateDefaultSubobject<USpringArmComponent>(this, TEXT("SpringArm"));
	SpringArmComp->TargetArmLength = 400.f;
	SpringArmComp->bUsePawnControlRotation = true;
	SpringArmComp->SetupAttachment(RootComponent);

	CameraComp = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("Camera"));
	CameraComp->bUsePawnControlRotation = false;
	CameraComp->SetupAttachment(SpringArmComp, USpringArmComponent::SocketName);

	PostProcessComponent = ObjectInitializer.CreateDefaultSubobject<UPostProcessComponent>(this, TEXT("PostProcessComponent"));
	PostProcessComponent->SetupAttachment(GetRootComponent());
	PostProcessComponent->bUnbound = true; // 시야 안 전체에만 적용할 경우 false
	PostProcessComponent->BlendWeight = 0.f; // 기본은 비활성화

	//steam name widget
	SteamNameWidgetComp = ObjectInitializer.CreateDefaultSubobject<UGS_SteamNameWidgetComp>(this, TEXT("SteamWidgetComp"));
	SteamNameWidgetComp->SetupAttachment(RootComponent);
	SteamNameWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	SteamNameWidgetComp->GetBodyInstance()->TermBody();
	SteamNameWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SteamNameWidgetComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	SteamNameWidgetComp->SetOwnerNoSee(true);
	SteamNameWidgetComp->SetCullDistance(GS_Rendering::STEAM_NAME_WIDGET_CULL_DISTANCE);
	SteamNameWidgetComp->SetCachedMaxDrawDistance(GS_Rendering::STEAM_NAME_WIDGET_CULL_DISTANCE);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BlurMat(TEXT("/Game/VFX/MI_AbscureDebuff"));
	if (BlurMat.Succeeded())
	{
		PostProcessMat = BlurMat.Object;
	}

	static ConstructorHelpers::FObjectFinder<UCurveFloat> CurveObj(TEXT("/Game/VFX/ObscureCurve"));
	if (CurveObj.Succeeded())
	{
		ObscureCurve = CurveObj.Object;
	}

	TeamId = FGenericTeamId(1);

	// AkComponent 생성
	AkComponent = ObjectInitializer.CreateDefaultSubobject<UAkComponent>(this, TEXT("AkComponent"));
	AkComponent->SetupAttachment(GetRootComponent());

	// 카메라 위치 오디오 리스너 컴포넌트 생성 (TPS 표준)
	CameraAudioListenerComponent = ObjectInitializer.CreateDefaultSubobject<UAkComponent>(this, TEXT("CameraAudioListenerComponent"));
	// CameraComp에 Attach하여 카메라 위치에서 오디오 리스닝
	CameraAudioListenerComponent->SetupAttachment(CameraComp);

	bIsObscuring = false;

	// 네트워크 최적화 초기화
	NetUpdateFrequency = GS_Rendering::NET_UPDATE_FREQ_CLOSE;
	MinNetUpdateFrequency = GS_Rendering::NET_UPDATE_FREQ_MIN;
}

void AGS_Player::BeginPlay()
{
	Super::BeginPlay();

	// === 데디케이티드 서버 크래시 방지 ===
	// 생성자에서 만든 AkComponent들이 리스너 없는 서버에서 Tick하면 크래시 발생
	// DefaultSubobject는 DestroyComponent 대신 비활성화만 수행
	if (IsRunningDedicatedServer() || GetNetMode() == NM_DedicatedServer)
	{
		if (IsValid(AkComponent))
		{
			AkComponent->Stop();
			AkComponent->SetComponentTickEnabled(false);
			AkComponent->Deactivate();
			AkComponent->UnregisterComponent();
		}
		if (IsValid(CameraAudioListenerComponent))
		{
			CameraAudioListenerComponent->Stop();
			CameraAudioListenerComponent->SetComponentTickEnabled(false);
			CameraAudioListenerComponent->Deactivate();
			CameraAudioListenerComponent->UnregisterComponent();
		}
		return; // 서버에서는 오디오 관련 초기화 중단
	}

	// 오디오 디바이스 캐싱
	CachedAudioDevice = FAkAudioDevice::Get();
	if (ObscureCurve)
	{
		FOnTimelineFloat TimelineCallback;
		TimelineCallback.BindUFunction(this, FName("HandleTimelineProgress"));

		FOnTimelineEvent TimelineFinishedCallback;
		TimelineFinishedCallback.BindUFunction(this, FName("OnTimelineFinished"));

		ObscureTimeline.AddInterpFloat(ObscureCurve, TimelineCallback);
		ObscureTimeline.SetTimelineFinishedFunc(TimelineFinishedCallback);
		ObscureTimeline.SetLooping(false);
	}

	BlurMID = UMaterialInstanceDynamic::Create(PostProcessMat, this);
	PostProcessComponent->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, BlurMID));

	// 로컬 플레이어 관리 (오디오 리스너 및 설정)
	if (IsLocalPlayer())
	{
		// 로컬 플레이어만 카메라 리스너 활성화
		SetupCameraAudioListener();

		// 자체 AkComponent의 Occlusion은 비활성화 (자가 차폐 방지)
		if (IsValid(AkComponent))
		{
			AkComponent->OcclusionRefreshInterval = 0.0f;
		}
	}
	else
	{
		// 다른 플레이어(리모트)의 소리는 벽에 의해 감쇠되도록 설정
		if (IsValid(AkComponent))
		{
			AkComponent->OcclusionRefreshInterval = 0.2f;
		}

		// 다른 플레이어의 귀(Listener)는 내 화면에서 소리를 들으면 안 됨.
		// Wwise의 전역 리스너 목록에서 이 컴포넌트를 확실히 제거해야 혼선이 발생하지 않음.
		if (IsValid(CameraAudioListenerComponent))
		{
			if (FAkAudioDevice* AudioDevice = FAkAudioDevice::Get())
			{
				// 이 캐릭터가 '내'가 아니면, 이 캐릭터의 귀는 전역 목록에서 즉시 삭제
				AudioDevice->RemoveDefaultListener(CameraAudioListenerComponent);
			}

			CameraAudioListenerComponent->Stop();
			CameraAudioListenerComponent->SetComponentTickEnabled(false);
			CameraAudioListenerComponent->Deactivate();
			CameraAudioListenerComponent->UnregisterComponent();

			UE_LOG(LogTemp, Log, TEXT("[AGS_Player] Successfully removed Wwise Listener for remote player: %s"), *GetName());
		}
	}

	// === Skeletal Mesh Distance Culling 설정 ===
	// 다른 플레이어(시커)는 몬스터보다 중요하므로 더 먼 거리에서 컬링
	if (!IsRunningDedicatedServer() && GetMesh() && !IsLocalPlayer())
	{
		USkeletalMeshComponent* MeshComp = GetMesh();

		MeshComp->SetCullDistance(GS_Rendering::PLAYER_CULL_DISTANCE);
		MeshComp->SetCachedMaxDrawDistance(GS_Rendering::PLAYER_CULL_DISTANCE);
		MeshComp->bAllowCullDistanceVolume = true;
		MeshComp->SetBoundsScale(GS_Rendering::DEFAULT_BOUNDS_SCALE);
	}

	// === 그림자 컬링 초기 설정 (클라이언트만, Local Player 제외) ===
	if (!IsRunningDedicatedServer() && !IsLocalPlayer())
	{
		UpdateShadowCulling();
	}
}

void AGS_Player::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ObscureTimeline.IsPlaying())
	{
		ObscureTimeline.TickTimeline(DeltaSeconds);
	}

	// 스팀 위젯 회전 업데이트 (컬링은 WidgetComp의 TickComponent에서 처리)
	if (IsValid(SteamNameWidgetComp) && GetNetMode() != NM_DedicatedServer)
	{
		// 위젯이 보이는 경우에만 회전 업데이트 (성능 최적화)
		if (SteamNameWidgetComp->IsVisible())
		{
			UpdateSteamNameWidgetRotation();
		}
	}

	// 시점 전환 보간 처리
	UpdatePerspectiveTransition(DeltaSeconds);
}

void AGS_Player::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (AGS_PlayerState* PS = GetPlayerState<AGS_PlayerState>())
	{
		PS->OnPawnStatInitialized();
		UE_LOG(LogTemp, Warning, TEXT("AGS_Player::PossessedBy: Synced StatComp health from PlayerState. PS Health: %f, StatComp Health set to: %f"), PS->CurrentHealth, StatComp->GetCurrentHealth());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AGS_Player (%s) PossessedBy: PlayerState is NULL!"), *GetName());
	}
}

void AGS_Player::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Stability: DefaultSubobject는 DestroyComponent 대신 비활성화만 수행
	if (IsValid(SteamNameWidgetComp))
	{
		SteamNameWidgetComp->SetWidget(nullptr);
		SteamNameWidgetComp->SetVisibility(false);
		SteamNameWidgetComp->SetComponentTickEnabled(false);
	}

	Super::EndPlay(EndPlayReason);
}

void AGS_Player::BeginDestroy()
{
	// 1. 먼저 Super::BeginDestroy() 호출 (중요!)
	Super::BeginDestroy();

	// 2. IsValid() 체크와 함께 안전하게 정리
	if (IsValid(SteamNameWidgetComp) && !SteamNameWidgetComp->IsBeingDestroyed())
	{
		SteamNameWidgetComp->SetWidget(nullptr);
		SteamNameWidgetComp->SetVisibility(false);
	}

	//	// DestroyComponent() 호출하지 않음! - 자동으로 소멸됨
	//}

	// 3. 다른 참조들도 안전하게 정리
	if (IsValid(AkComponent) && !AkComponent->IsBeingDestroyed())
	{
		AkComponent->Stop();
	}

	// 4. CameraAudioListenerComponent 정리
	if (IsValid(CameraAudioListenerComponent) && !CameraAudioListenerComponent->IsBeingDestroyed())
	{
		// 기본 리스너에서 제거
		FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
		if (AudioDevice != nullptr && CameraAudioListenerComponent->IsDefaultListener)
		{
			AudioDevice->RemoveDefaultListener(CameraAudioListenerComponent);
		}
		CameraAudioListenerComponent->Stop();
	}

	// 5. 타임라인 정리
	if (ObscureTimeline.IsPlaying())
	{
		ObscureTimeline.Stop();
	}

	// 6. 다이나믹 머티리얼 참조 해제
	BlurMID = nullptr;

	UE_LOG(LogTemp, Warning, TEXT("AGS_Player::BeginDestroy() completed for %s"), *GetName());
}

void AGS_Player::Client_StartVisionObscured_Implementation()
{
	StartVisionObscured();
}

void AGS_Player::StartVisionObscured()
{
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		if (!bIsObscuring && ObscureCurve)
		{
			bIsObscuring = true;
			ObscureTimeline.PlayFromStart();
		}

		if (PostProcessComponent)
		{
			PostProcessComponent->BlendWeight = 1.0f;

			if (BlurMID)
			{
				BlurMID->SetScalarParameterValue("InnerRadius", 0.3f); // 완전 차단 반경
				BlurMID->SetScalarParameterValue("OuterRadius", 0.8f); // 차단 종료 반경
			}
		}
	}
}

void AGS_Player::Client_StopVisionObscured_Implementation()
{
	StopVisionObscured();
}

void AGS_Player::StopVisionObscured()
{
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		ObscureTimeline.ReverseFromEnd();
		bIsObscuring = false;
		if (PostProcessComponent)
		{
			PostProcessComponent->BlendWeight = 0.0f;
		}
	}
}

void AGS_Player::HandleTimelineProgress(float Value)
{
	UE_LOG(LogTemp, Warning, TEXT("Timeline Progress: %f"), Value);

	if (PostProcessComponent)
	{
		PostProcessComponent->BlendWeight = 1.0f;

		if (BlurMID)
		{
			BlurMID->SetScalarParameterValue("InnerRadius", Value);
			BlurMID->SetScalarParameterValue("OuterRadius", 1.1f);
		}
	}
}

void AGS_Player::OnTimelineFinished()
{
	bIsObscuring = false;
}

void AGS_Player::Multicast_SetUseControllerRotationYaw_Implementation(bool UseControlRotationYaw)
{
	bUseControllerRotationYaw = UseControlRotationYaw;
}

void AGS_Player::OnDeath()
{
	Super::OnDeath();

	// 추가적인 플레이어 죽음 처리 로직을 여기에 구현할 수 있다
	// 예: 카메라 연출, UI 변경, 리스폰 타이머 등

	// TODO: 추후 빈사 상태 등 복잡한 사망 처리가 필요할 시, 이 로직은 해당 상태 전환 함수로 이동해야 함.

	// 현재 사용 중인 스킬 강제 중단 및 VFX 정리
	if (SkillComp)
	{
		if (UGS_SkillBase* CurrentSkill = SkillComp->GetActiveSkill())
		{
			CurrentSkill->InterruptSkill();
		}
	}

	GetCharacterMovement()->DisableMovement();

	AGS_PlayerState* GS_PS = Cast<AGS_PlayerState>(GetPlayerState());
	if (GS_PS)
	{
		GS_PS->SetIsAlive(false);
	}
	AGS_TpsController* GS_PC = Cast<AGS_TpsController>(GetController());
	if (IsValid(GS_PC) && GS_PS->CurrentPlayerRole == EPlayerRole::PR_Seeker)
	{
		GS_PC->ServerRPCSpectatePlayer();
	}
}

void AGS_Player::Multicast_SetCollisionResponseToChannel_Implementation(ECollisionChannel Channel,
                                                                        ECollisionResponse NewResponse)
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(Channel, NewResponse);
	}

	// Also update mesh collision to prevent stuck issues during roll
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionResponseToChannel(Channel, NewResponse);
	}
}

void AGS_Player::SetSkillInputControl(bool CanLeftClick, bool CanRightClick, bool CanRollClick, bool CanCtrlClick)
{
	SkillInputControl.CanInputLC = CanLeftClick;
	SkillInputControl.CanInputRC = CanRightClick;
	SkillInputControl.CanInputRoll = CanRollClick;
	SkillInputControl.CanInputCtrl = CanCtrlClick;
}

FSkillInputControl AGS_Player::GetSkillInputControl()
{
	return SkillInputControl;
}

void AGS_Player::SetCanUseSkill(bool bCanUse)
{
	if (SkillComp)
	{
		SkillComp->SetCanUseSkill(bCanUse);
	}
}

void AGS_Player::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGS_Player, SkillInputControl);
}

void AGS_Player::Multicast_StopSkillMontage_Implementation(UAnimMontage* Montage)
{
	StopAnimMontage(Montage);
}

void AGS_Player::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Enhanced Input: 시점 전환 (F5)
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_TogglePerspective)
		{
			EnhancedInput->BindAction(IA_TogglePerspective, ETriggerEvent::Started, this, &AGS_Player::TogglePerspective);
		}
	}
}

void AGS_Player::SetupLocalAudioListener()
{
	// 카메라 오디오 리스너 설정으로 통합됨 (TPS 표준)
	SetupCameraAudioListener();
}

void AGS_Player::SetupCameraAudioListener()
{
	// 로컬 플레이어만 리스너 설정
	if (!IsLocalPlayer())
	{
		return;
	}

	// 카메라 컴포넌트 검증
	if (!IsValid(CameraComp))
	{
		UE_LOG(LogTemp, Error, TEXT("[AGS_Player] SetupCameraAudioListener: CameraComp is invalid - %s"), *GetName());
		return;
	}

	// CameraAudioListenerComponent 검증
	if (!IsValid(CameraAudioListenerComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("[AGS_Player] SetupCameraAudioListener: CameraAudioListenerComponent is invalid - %s"), *GetName());
		return;
	}

	// 오디오 디바이스 검증
	if (CachedAudioDevice == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[AGS_Player] SetupCameraAudioListener: CachedAudioDevice is null - %s"), *GetName());
		return;
	}

	// TPS 리스너 설정: 위치와 방향 모두 카메라 따라가기
	CameraAudioListenerComponent->bUseReverbVolumes = true;

	// Occlusion 비활성화 (캐릭터 메시 차폐 방지)
	CameraAudioListenerComponent->OcclusionRefreshInterval = 0.0f;

	// 카메라 위치 리스너를 기본 리스너로 설정 (TPS 표준)
	CachedAudioDevice->AddDefaultListener(CameraAudioListenerComponent);
}

// 로컬 플레이어 확인 함수
bool AGS_Player::IsLocalPlayer() const
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		return PC->IsLocalController();
	}
	return false;
}

void AGS_Player::Multicast_PlaySkillMontage_Implementation(UAnimMontage* Montage, FName Section, int32 PlayRate)
{
	UGS_SeekerAnimInstance* AnimInstance = Cast<UGS_SeekerAnimInstance>(GetMesh()->GetAnimInstance());
	if (AnimInstance && Montage)
	{
		if (Section == NAME_None)
		{
			AnimInstance->Montage_Play(Montage, PlayRate);
		}
		else
		{
			if (AnimInstance->Montage_IsPlaying(Montage))
			{
				AnimInstance->Montage_Stop(0.0f, Montage); // 이걸 꼭 해줘야 새로 PlayRate가 반영됨
			}

			AnimInstance->Montage_Play(Montage);
			AnimInstance->Montage_JumpToSection(Section, Montage);
		}
	}
}

void AGS_Player::PlaySound(UAkAudioEvent* SoundEvent)
{
	if (!IsValid(AkComponent) || !SoundEvent)
	{
		UE_LOG(LogAudio, Warning, TEXT("AkComponent or SoundEvent is null in PlaySound"));
		return;
	}

	// Transform 검증
	const FVector Location = GetActorLocation();
	const FRotator Rotation = GetActorRotation();

	if (!UGS_AudioComponentBase::IsTransformValid(Location, Rotation))
	{
		UE_LOG(LogAudio, Error, TEXT("[AGS_Player] PlaySound: Invalid Transform - %s"), *GetName());
		return;
	}

	AkComponent->PostAkEvent(SoundEvent);
}

void AGS_Player::PlaySoundWithCallback(UAkAudioEvent* SoundEvent, const FOnAkPostEventCallback& Callback)
{
	if (!IsValid(AkComponent) || !SoundEvent)
	{
		UE_LOG(LogAudio, Warning, TEXT("AkComponent or SoundEvent is null in PlaySoundWithCallback"));
		return;
	}

	// Transform 검증
	const FVector Location = GetActorLocation();
	const FRotator Rotation = GetActorRotation();

	if (!UGS_AudioComponentBase::IsTransformValid(Location, Rotation))
	{
		UE_LOG(LogAudio, Error, TEXT("[AGS_Player] PlaySoundWithCallback: Invalid Transform - %s"), *GetName());
		return;
	}

	AkComponent->PostAkEvent(SoundEvent, 0, Callback);
}

void AGS_Player::UpdateSteamNameWidgetRotation()
{
	if (!IsValid(SteamNameWidgetComp))
	{
		return;
	}

	if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		FVector CameraForward = CameraManager->GetCameraRotation().Vector();
		FVector CameraRight = FVector::CrossProduct(CameraForward, FVector::UpVector).GetSafeNormal();
		FVector CameraUp = FVector::CrossProduct(CameraRight, CameraForward).GetSafeNormal();
		FRotator WidgetRotation = UKismetMathLibrary::MakeRotFromXZ(-CameraForward, CameraUp);

		SteamNameWidgetComp->SetWorldRotation(WidgetRotation);
	}
}

/*
void AGS_Player::Server_RestKey_Implementation()
{
	SetSkillInputControl(true, true, true, true);
	UGS_SeekerAnimInstance * SeekerAnim = Cast<UGS_SeekerAnimInstance>(GetMesh()->GetAnimInstance());
	if (SeekerAnim)
	{
		SeekerAnim->IsPlayingUpperBodyMontage = false;
		SeekerAnim->IsPlayingFullBodyMontage = false;
	};
}
*/

float AGS_Player::GetOptimalCullDistance() const
{
	// 기본값: 중간 크기 플레이어 컬링 거리
	return GS_Rendering::MONSTER_MEDIUM_CULL_DISTANCE;
}

float AGS_Player::CalculateSignificance(const FTransform& Viewpoint)
{
	// 로컬 플레이어는 항상 최상위 중요도 (죽어도, 빈사 상태여도 카메라 중심)
	if (IsLocalPlayer())
		return 1.0f;

	// Dying State는 죽음보다 중요! (구조 가능, 파티원이 달려옴, 빈사 애니메이션 중요)
	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(this))
	{
		if (Seeker->IsInDyingState())
			return 0.6f; // 중간~높은 중요도 → 30Hz 네트워크, 중간 애니메이션 품질
	}

	// 죽은 Player는 중간 중요도 유지 (TPS 시점에서 파티원 시체가 보임)
	// Monster와 달리 Player는 죽어도 화면에 보이므로 애니메이션/네트워크 유지 필요
	if (IsDead())
		return 0.5f; // 중간 중요도 → 30Hz 네트워크 업데이트, 중간 애니메이션 품질

	float Score = 0.1f;
	FVector ActorLoc = GetActorLocation();
	FVector ViewLoc = Viewpoint.GetLocation();
	float DistSq = FVector::DistSquared(ActorLoc, ViewLoc);

	// === 전투 상태 체크: HP가 낮으면 중요도 강제 상승 ===
	bool bIsInCombat = false;

	if (StatComp)
	{
		float HealthRatio = StatComp->GetCurrentHealth() / StatComp->GetMaxHealth();
		// HP가 90% 이하이면 전투 중으로 간주
		if (HealthRatio < 0.9f)
		{
			bIsInCombat = true;
		}
	}

	// === 이동 상태 체크: 이동 중이면 중요도 상승 ===
	bool bIsMoving = GetVelocity().SizeSquared() > 100.0f; // 10cm/s 이상

	// 거리 기반 점수 (50m 기준)
	float MaxRangeSq = FMath::Square(5000.0f);
	Score = FMath::Clamp(1.2f - (DistSq / MaxRangeSq), 0.1f, 1.0f);

	// === 전투 중이거나 이동 중이면 중요도 보장 (부모의 NetUpdateFrequency 60Hz 유도) ===
	if (bIsInCombat || bIsMoving)
	{
		Score = FMath::Max(Score, 0.85f); // 0.8 초과 → 부모의 OnSignificanceChanged에서 60Hz 설정
	}

	return Score;
}

// ================
// 시점 전환 시스템 구현
// ================

void AGS_Player::TogglePerspective()
{
	// 로컬 플레이어만 시점 전환 가능
	if (!IsLocalPlayer())
	{
		return;
	}

	if (!SpringArmComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AGS_Player] TogglePerspective: SpringArmComp is null"));
		return;
	}

	bIsFirstPerson = !bIsFirstPerson;
	bIsPerspectiveTransitioning = true; // 보간 시작 플래그 ON

	// 최적화를 위해 Tick이 꺼져 있을 수 있으므로, 보간을 위해 강제로 켬
	SetActorTickEnabled(true);

	// 서버에 시점 상태 동기화 (다른 플레이어에게 회전 모습 반영 위함)
	Server_SetPerspectiveState(bIsFirstPerson);

	if (bIsFirstPerson)
	{
		// TPS → 1인칭: 현재 설정 저장 후 즉시 전환
		SavedTPSArmLength = SpringArmComp->TargetArmLength;
		SavedTPSSocketOffset = SpringArmComp->SocketOffset;
		bSavedUseControllerRotationYaw = bUseControllerRotationYaw;

		// FOV 및 Near Clip Plane 저장 및 적용
		if (CameraComp)
		{
			SavedTPSFOV = CameraComp->FieldOfView;
			CameraComp->SetFieldOfView(FirstPersonFOV);

			// Near Clip Plane 조정 (가까이 오브젝트 렌더링)
			SavedNearClipPlane = GNearClippingPlane;
			GNearClippingPlane = FirstPersonNearClipPlane;
		}

		// 1인칭 설정 적용
		SpringArmComp->TargetArmLength = FirstPersonArmLength;
		SpringArmComp->SocketOffset = FirstPersonEyeOffset;

		// 카메라 방향에 따라 캐릭터 회전 (더 자연스러운 1인칭)
		bUseControllerRotationYaw = true;

		// 1인칭에서 카메라 충돌 테스트 비활성화 (벽에 밀리지 않도록)
		SpringArmComp->bDoCollisionTest = false;

		// 1인칭: 디더링 비활성화 (거리를 0으로 만들어 항상 보이게 함)
		UpdateCharacterDither(false);
	}
	else
	{
		// 1인칭 → TPS: 저장된 설정으로 즉시 복원 시작
		// SpringArmComp->TargetArmLength = SavedTPSArmLength; // 제거: UpdatePerspectiveTransition에서 처리
		SpringArmComp->SocketOffset = SavedTPSSocketOffset;

		// FOV 및 Near Clip Plane 복원
		if (CameraComp)
		{
			CameraComp->SetFieldOfView(SavedTPSFOV);
			GNearClippingPlane = SavedNearClipPlane;
		}

		// 캐릭터 회전 설정 복원
		bUseControllerRotationYaw = bSavedUseControllerRotationYaw;

		// TPS에서 카메라 충돌 테스트 다시 활성화
		SpringArmComp->bDoCollisionTest = true;

		// 3인칭: 디더링 다시 활성화
		UpdateCharacterDither(true);
	}
}

void AGS_Player::Server_SetPerspectiveState_Implementation(bool bFirstPerson)
{
	// 서버에서도 캐릭터 회전 로직을 시점에 맞게 설정
	// 이를 통해 다른 플레이어들도 이 플레이어가 어디를 보는지 정확히 알 수 있음
	bUseControllerRotationYaw = bFirstPerson;
}

void AGS_Player::UpdatePerspectiveTransition(float DeltaTime)
{
	// 보간 진행 중이 아니거나 SpringArm이 없거나 로컬 플레이어가 아니면 무시
	if (!bIsPerspectiveTransitioning || !SpringArmComp || !IsLocalPlayer())
	{
		return;
	}

	// 목표 ArmLength 결정
	float TargetLength = bIsFirstPerson ? FirstPersonArmLength : SavedTPSArmLength;

	// 이미 목표값에 도달했으면 종료
	if (FMath::IsNearlyEqual(SpringArmComp->TargetArmLength, TargetLength, 1.0f))
	{
		SpringArmComp->TargetArmLength = TargetLength;
		bIsPerspectiveTransitioning = false; // 보간 완료 플래그 OFF

		// 시점 전환 완료 후 Tick 비활성화 (성능 최적화)
		// 서브클래스에서 다른 Tick 로직이 필요하면 해당 클래스에서 재활성화
		SetActorTickEnabled(false);
		return;
	}

	// 부드러운 보간으로 전환
	SpringArmComp->TargetArmLength = FMath::FInterpTo(
	    SpringArmComp->TargetArmLength,
	    TargetLength,
	    DeltaTime,
	    PerspectiveTransitionSpeed);
}

void AGS_Player::UpdateCharacterDither(bool bEnabled)
{
	float TargetDistance = bEnabled ? DefaultDitherDistance : 0.0f;

	// 1. 현재 액터의 모든 메시 컴포넌트 찾기
	TArray<UMeshComponent*> MeshComps;
	GetComponents<UMeshComponent>(MeshComps);

	// 2. 부착된 액터(무기 등)의 메시 컴포넌트도 포함하기
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (AttachedActor)
		{
			TArray<UMeshComponent*> AttachedMeshComps;
			AttachedActor->GetComponents<UMeshComponent>(AttachedMeshComps);
			MeshComps.Append(AttachedMeshComps);
		}
	}

	// 3. 자식 액터 컴포넌트 내부의 메시 탐색
	TArray<UChildActorComponent*> ChildActorComps;
	GetComponents<UChildActorComponent>(ChildActorComps);
	for (UChildActorComponent* ChildComp : ChildActorComps)
	{
		if (ChildComp && ChildComp->GetChildActor())
		{
			TArray<UMeshComponent*> ChildMeshComps;
			ChildComp->GetChildActor()->GetComponents<UMeshComponent>(ChildMeshComps);
			MeshComps.Append(ChildMeshComps);
		}
	}

	// 모든 수집된 메시의 머터리얼 파라미터 설정
	// SetScalarParameterValueOnMaterials는 내부적으로 모든 머터리얼을 순회함
	for (UMeshComponent* MeshComp : MeshComps)
	{
		if (MeshComp)
		{
			MeshComp->SetScalarParameterValueOnMaterials(DitherParamName, TargetDistance);
		}
	}
}
