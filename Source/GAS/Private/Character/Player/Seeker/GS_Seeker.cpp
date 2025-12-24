// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Player/Seeker/GS_Seeker.h"
#include "Blueprint/UserWidget.h"
#include "Character/Component/GS_SkillInputHandlerComp.h"
#include "Character/Component/GS_StatComp.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Engine/GameInstance.h"
#include "Sound/GS_AudioManager.h"
#include "System/GS_PlayerState.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Components/ChildActorComponent.h"
#include "GameFramework/Character.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Character/Component/GS_VFXComponent.h"
#include "Animation/Character/Seeker/GS_ChooserInputObj.h"
#include "Character/GS_TpsController.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Rendering/GS_RenderingConstants.h"
#include "AkAudioEvent.h"
/*#include "AkComponent.h"
#include "AkAudioDevice.h"*/
#include "UI/Character/GS_HPTextWidgetComp.h"
#include "Sound/GS_SeekerAudioComponent.h"
#include "Character/Component/GS_LowHealthEffectComponent.h"
#include "Character/Component/GS_DetectionEffectComponent.h"
#include "Character/Component/Seeker/GS_MarkerPlacementComponent.h"
#include "Props/Item/SeekerItem/GS_HP_Potion.h"
#include "Props/Item/GS_ItemData.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"
#include "UI/Character/GS_SteamNameWidgetComp.h"
#include "Props/Item/EmberChest/GS_EmberChest.h"

// Sets default values
AGS_Seeker::AGS_Seeker()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;

	GetMesh()->bEnableUpdateRateOptimizations = false;
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->bOnlyAllowAutonomousTickPose = false;

    // Post Process Component 생성 (Low Health)
    LowHealthPostProcessComp = CreateDefaultSubobject<UPostProcessComponent>(TEXT("LowHealthPostProcessComp"));
    LowHealthPostProcessComp->SetupAttachment(CameraComp);
    LowHealthPostProcessComp->bEnabled = false;
    LowHealthPostProcessComp->Priority = 10;
    LowHealthEffectComp = CreateDefaultSubobject<UGS_LowHealthEffectComponent>(TEXT("LowHealthEffectComp"));

    // Post Process Component 생성 (가디언 감지 - MPP_Detect)
    DetectionPostProcessComp = CreateDefaultSubobject<UPostProcessComponent>(TEXT("DetectionPostProcessComp"));
    DetectionPostProcessComp->SetupAttachment(CameraComp);
    DetectionPostProcessComp->bEnabled = false;
    DetectionPostProcessComp->Priority = 11; // Low Health보다 높은 우선순위
    DetectionEffectComp = CreateDefaultSubobject<UGS_DetectionEffectComponent>(TEXT("DetectionEffectComp"));

	// =======================
	// Post Process Component 생성 (빈사 상태 - Dying)
	// =======================
	DyingPostProcessComp = CreateDefaultSubobject<UPostProcessComponent>(TEXT("DyingPostProcessComp"));
	DyingPostProcessComp->SetupAttachment(CameraComp);
	DyingPostProcessComp->bEnabled = false;
	DyingPostProcessComp->Priority = 12; // 다른 효과보다 높은 우선순위

	// =======================
	// VFX 컴포넌트 생성 (디버프, 힐링 등 모든 VFX)
	// =======================
	VFXComponent = CreateDefaultSubobject<UGS_VFXComponent>("VFXComponent");

	// =======================
	// 시커 오디오 컴포넌트 생성 (RTS/TPS 지원)
	// =======================
	SeekerAudioComponent = CreateDefaultSubobject<UGS_SeekerAudioComponent>("SeekerAudioComponent");

	// =======================
	// 마커 배치 컴포넌트 생성
	// =======================
	MarkerPlacementComponent = CreateDefaultSubobject<UGS_MarkerPlacementComponent>("MarkerPlacementComponent");

	// Fire Effect 생성 및 설정
	FeetLavaVFX_L = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FeetLavaVFX_L"));
	FeetLavaVFX_L->SetupAttachment(GetMesh(), FName("foot_l_Socket"));
	FeetLavaVFX_L->bAutoActivate = false;
	FeetLavaVFX_L->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	FeetLavaVFX_R = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FeetLavaVFX_R"));
	FeetLavaVFX_R->SetupAttachment(GetMesh(), FName("foot_r_Socket"));
	FeetLavaVFX_R->bAutoActivate = false;
	FeetLavaVFX_R->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));

	BodyLavaVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BodyLavaVFX"));
	BodyLavaVFX->SetupAttachment(GetMesh(), FName("spine_03"));
	BodyLavaVFX->bAutoActivate = false;
	BodyLavaVFX->SetRelativeLocation(FVector(-60.f, 0.f, 0.f));
	BodyLavaVFX->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

	// =======================
	// 빈사 상태 불꽃 VFX 컴포넌트 초기화
	// =======================
	DyingFlameEffectComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DyingFlameEffectComp"));
	DyingFlameEffectComp->SetupAttachment(RootComponent);
	DyingFlameEffectComp->bAutoActivate = false;
	DyingFlameEffectComp->SetRelativeLocation(FVector(0.f, 0.f, -88.f)); // 캡슐 바닥에서 시작

	DyingMagicCircleComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DyingMagicCircleComp"));
	DyingMagicCircleComp->SetupAttachment(RootComponent);
	DyingMagicCircleComp->bAutoActivate = false;
	DyingMagicCircleComp->SetRelativeLocation(FVector(0.f, 0.f, -90.f)); // 바닥
	DyingMagicCircleComp->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // 바닥에 평행하게

	// 전투 BGM 트리거 생성 (시커가 몬스터를 감지)
	CombatTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("CombatTrigger"));
    CombatTrigger->SetupAttachment(RootComponent);
    CombatTrigger->SetSphereRadius(CombatTriggerRadius);
	CombatTrigger->SetCollisionProfileName(TEXT("SoundTrigger"));

	//함정 - 화살발사기의 화살 채널 설정(Projectile)
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
	//함정 - 모든 함정 채널 설정(Trap)
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Overlap);
	GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Ignore);

	// State
	SeekerGait = EGait::Run;
	LastSeekerGait = SeekerGait;
	CanChangeSeekerGait = true;
	GaitBeforeDying = EGait::Run;

	// Item (hard coding) -> 나중에 SkillSet DataTable 과 같이 ItemSet DataTable 를 가지고 초기화 할 수 있도록 한다. // SJE
	UGS_ItemData* ItemData = CreateDefaultSubobject<UGS_ItemData>(TEXT("HP_Potion_Data"));
	ItemData->ItemName = TEXT("HP_Potion");
	ItemData->ItemType = EItemType::HP_Potion;
	ItemData->MaxCount = 5;
	ItemData->CurCount = 5;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FullPotionMesh(TEXT("/Game/Props/Item/Stuff/Mesh/HP_Potion_Full.HP_Potion_Full"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> EmptyPotionMesh(TEXT("/Game/Props/Item/Stuff/Mesh/HP_Potion_Empty.HP_Potion_Empty"));
	
	ItemData->ItemMeshs.Add(FName(TEXT("HP_Potion_Full")), FullPotionMesh.Object);
	ItemData->ItemMeshs.Add(FName(TEXT("HP_Potion_Empty")), EmptyPotionMesh.Object);

	ItemDatas.Add(EItemType::HP_Potion, ItemData);
}

void AGS_Seeker::BeginPlay()
{
	Super::BeginPlay();

	// CombatTrigger 오버랩 이벤트 바인딩
	if (CombatTrigger)
	{
		CombatTrigger->OnComponentBeginOverlap.AddDynamic(this, &AGS_Seeker::OnCombatTriggerBeginOverlap);
		CombatTrigger->OnComponentEndOverlap.AddDynamic(this, &AGS_Seeker::OnCombatTriggerEndOverlap);
	}

	// Generate Overlap Events 활성화 (화살 함정 충돌 처리를 위해 필요)
	if (GetMesh())
	{
		if (!GetMesh()->GetGenerateOverlapEvents())
		{
			GetMesh()->SetGenerateOverlapEvents(true);
		}
	}

    if (IsLocallyControlled())
	{
		InitializeCameraManager();

		// 스탯 컴포넌트 가져와서 델리게이트 바인딩
        if (UGS_StatComp* FoundStatComp = FindComponentByClass<UGS_StatComp>())
		{
			FoundStatComp->OnCurrentHPChanged.AddUObject(this, &AGS_Seeker::HandleLowHealthEffect);
		}

		// PlayerState 생존 상태 변경 델리게이트 바인딩
		AGS_PlayerState* PS = GetPlayerState<AGS_PlayerState>();
		if (PS)
		{
			PS->OnPlayerAliveStatusChangedDelegate.AddUObject(this, &AGS_Seeker::HandleAliveStatusChanged);
		}
	}

	// Register to Subsystem for optimization
	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->RegisterSeeker(this);
		}
	}

	// === 최적화: Tick 비활성화 및 타이머 시스템 가동 ===
	// 기본 Tick을 비활성화하여 CPU 사용량을 줄입니다.
	SetActorTickEnabled(false);

	// 주변 감지(상자 등) 및 저빈도 업데이트용 타이머 시작 (0.1초/10Hz)
	// 이 타이머는 블루프린트의 OnPeripheralSensorUpdate 이벤트를 호출합니다.
	GetWorldTimerManager().SetTimer(PeripheralSensorTimerHandle, this, &AGS_Seeker::UpdatePeripheralSensor, 0.1f, true);
}

void AGS_Seeker::PawnClientRestart()
{
	Super::PawnClientRestart();

	// 로컬 플레이어 빙의 후, 이미 가디언에게 감지된 상태라면 UI 업데이트
	if (IsLocallyControlled() && bIsDetectedByGuardian)
	{
		UpdateDetectionEffects();
	}
}

void AGS_Seeker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 현재 시커의 Tick 로직은 모두 타이머(UpdatePeripheralSensor, UpdateDyingStateTimer)로 이동되었습니다.
	// 하위 클래스(Merci의 Zoom 등)에서 필요할 경우 하위 클래스에서 Tick을 켜고 사용할 수 있습니다.
}

// Called to bind functionality to input
void AGS_Seeker::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (SkillInputHandlerComponent)
	{
		SkillInputHandlerComponent->SetupEnhancedInput(PlayerInputComponent);
	}
}

void AGS_Seeker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// 소유 클라이언트는 어차피 로컬에서 효과를 계산하므로 복제 제외 가능
	DOREPLIFETIME_CONDITION(AGS_Seeker, bIsLowHealthEffectActive, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AGS_Seeker, CurrentEffectStrength, COND_SkipOwner);
	
	DOREPLIFETIME(AGS_Seeker, LastSeekerGait);
	DOREPLIFETIME(AGS_Seeker, SeekerGait);
	DOREPLIFETIME(AGS_Seeker, CanChangeSeekerGait);
	DOREPLIFETIME(AGS_Seeker, CanAcceptComboInput);
	DOREPLIFETIME(AGS_Seeker, CurrentComboIndex);
	DOREPLIFETIME(AGS_Seeker, SeekerState);

	DOREPLIFETIME(AGS_Seeker, bIsDetectedByGuardian);
	DOREPLIFETIME(AGS_Seeker, DetectionIntensity);
	
	// 빈사 상태 변수들
	DOREPLIFETIME(AGS_Seeker, bIsInDyingState);
	DOREPLIFETIME(AGS_Seeker, DyingTimeRemaining);
	DOREPLIFETIME(AGS_Seeker, CurrentDyingCount);
	DOREPLIFETIME(AGS_Seeker, bIsBeingRevived);
	DOREPLIFETIME(AGS_Seeker, ReviveProgress);
}

AGS_Item* AGS_Seeker::GetItem(EItemType ItemType)
{
	if (AGS_Item* Item = Cast<AGS_Item>(Items.FindRef(ItemType)))
	{
		return Item;
	}
	
	return nullptr;
}

UGS_ItemData* AGS_Seeker::GetItemData(EItemType ItemType)
{
	if (UGS_ItemData* ItemData = Cast<UGS_ItemData>(Items.FindRef(ItemType)))
	{
		return ItemData;
	}
	return nullptr;
}

void AGS_Seeker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(SeekerAudioComponent))
	{
		SeekerAudioComponent->SetComponentTickEnabled(false);
	}

	if (GetWorldTimerManager().IsTimerActive(LowHealthEffectTimer))
	{
		GetWorldTimerManager().ClearTimer(LowHealthEffectTimer);
	}
	
    if (IsLocallyControlled() && LowHealthPostProcessComp)
	{
		LowHealthPostProcessComp->bEnabled = false;
		LowHealthPostProcessComp->Settings.WeightedBlendables.Array.Empty();
	}

	// PlayerState 생존 상태 변경 델리게이트 해제
	if (IsLocallyControlled())
	{
		AGS_PlayerState* PS = GetPlayerState<AGS_PlayerState>();
		if (PS)
		{
			PS->OnPlayerAliveStatusChangedDelegate.RemoveAll(this);
		}
	}

	// 빈사/구조 관련 타이머 정리
	SafeClearTimer(ReviveDecayTimerHandle);
	SafeClearTimer(DyingUpdateTimerHandle);
	SafeClearTimer(PeripheralSensorTimerHandle);

	// 포스트 프로세스 비활성화
	// Unregister from Subsystem
	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->UnregisterSeeker(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}


void AGS_Seeker::SetAimState(bool IsAim)
{
	SeekerState.IsAim = IsAim;
	
	// 시커 오디오 컴포넌트에 조준 상태 변경 알림
	if (SeekerAudioComponent)
	{
		if (IsAim)
		{
			SeekerAudioComponent->SetSeekerAudioState(ESeekerAudioState::Aiming);
		}
		else if (SeekerAudioComponent->GetCurrentAudioState() == ESeekerAudioState::Aiming)
		{
			// 조준을 해제했을 때 다른 상태로 전환
			SeekerAudioComponent->SetSeekerAudioState(ESeekerAudioState::Idle);
		}
	}
}

bool AGS_Seeker::GetAimState()
{
	return SeekerState.IsAim;
}

void AGS_Seeker::SetDrawState(bool IsDraw)
{
	FSeekerState NewState = SeekerState;
	NewState.IsDraw = IsDraw;
	SeekerState = NewState;
}

bool AGS_Seeker::GetDrawState()
{
	return SeekerState.IsDraw;
}

void AGS_Seeker::Server_SetSeekerGait_Implementation(EGait Gait)
{
	// 빈사 상태인 경우 Crawl 외의 Gait 변경 무시
	if (bIsInDyingState && Gait != EGait::Crawl)
	{
		return;
	}

	LastSeekerGait = SeekerGait;
	SeekerGait = Gait;

	if (UGS_SeekerAnimInstance* SeekerAnim = Cast<UGS_SeekerAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		SeekerAnim->ChooserInputObj->Gait = SeekerGait;
	}
	
	switch (Gait)
	{
	case EGait::Walk :
		SetCharacterSpeed(0.45f);	
		break;
	case EGait::Run :
		SetCharacterSpeed(0.8f);
		break;
	case EGait::Sprint :
		SetCharacterSpeed(1.0f);
		break;
	case EGait::Crawl :
		SetCharacterSpeed(0.15f);  // 빈사 상태 기어다니기 - 매우 느린 속도
		break;
	}
}

void AGS_Seeker::SetSeekerGait(EGait Gait)
{
	// 빈사 상태인 경우 Crawl 외의 Gait 변경 무시
	if (bIsInDyingState && Gait != EGait::Crawl)
	{
		return;
	}

	LastSeekerGait = SeekerGait;
	SeekerGait = Gait;
	if (UGS_SeekerAnimInstance* SeekerAnim = Cast<UGS_SeekerAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		SeekerAnim->ChooserInputObj->Gait = SeekerGait;
	}
	
	switch (Gait)
	{
	case EGait::Walk :
		SetCharacterSpeed(0.45f);	
		break;
	case EGait::Run :
		SetCharacterSpeed(0.8f);
		break;
	case EGait::Sprint :
		SetCharacterSpeed(1.0f);
		break;
	case EGait::Crawl :
		SetCharacterSpeed(0.15f);  // 빈사 상태 기어다니기 - 매우 느린 속도
		break;
	}
}

EGait AGS_Seeker::GetSeekerGait()
{
	return SeekerGait;
}

EGait AGS_Seeker::GetLastSeekerGait()
{
	return LastSeekerGait;
}

void AGS_Seeker::StateReset()
{
	this->SetAimState(false);
	this->SetDrawState(false);
	
	if (GetMesh() && GetMesh()->GetAnimInstance())
	{
		if (UGS_SeekerAnimInstance* AnimInstance = Cast<UGS_SeekerAnimInstance>(GetMesh()->GetAnimInstance()))
		{
			this->Multicast_SetMontageSlot(ESeekerMontageSlot::None);
		}
	}

	if (AController* PlayerController = GetController())
	{
		if (AGS_TpsController* TPSController = Cast<AGS_TpsController>(PlayerController))
		{
			TPSController->SetIsAutoMoving(false);
		}
	}
	
	this->CanChangeSeekerGait = true;
	this->CanAcceptComboInput = true;
	this->SetMoveControlValue(true, true);
	this->SetLookControlValue(true, true);

	this->Multicast_SetMontageSlot(ESeekerMontageSlot::None);

	if (this->GetSkillComp())
	{
		this->GetSkillComp()->ResetAllowedSkillsMask();
	}
}

const FName AGS_Seeker::HPRatioParamName = TEXT("HPRatio");
const FName AGS_Seeker::EffectIntensityParamName = TEXT("EffectIntensity");

void AGS_Seeker::InitializeCameraManager()
{
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		LocalCameraManager = PC->PlayerCameraManager;

        // Low Health: 컴포넌트 초기화
        if (LowHealthEffectComp)
        {
            LowHealthEffectComp->InitializeForOwner(this, LowHealthPostProcessComp, LowHealthEffectMaterial);
        }

        // Detection: 컴포넌트 초기화
        if (DetectionEffectComp)
        {
            DetectionEffectComp->InitializeForOwner(this, DetectionPostProcessComp, DetectionEffectMaterial);
        }

		// Dying: PostProcess 초기화
		if (DyingPostProcessComp && DyingEffectMaterial)
		{
			DyingDynamicMaterial = UMaterialInstanceDynamic::Create(DyingEffectMaterial, this);
			DyingPostProcessComp->Settings.WeightedBlendables.Array.Empty();
			DyingPostProcessComp->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, DyingDynamicMaterial));
			DyingPostProcessComp->bEnabled = false;
		}
	}
}

void AGS_Seeker::Server_SetNextComboFlag_Implementation(bool NextCombo)
{
	bNextCombo = NextCombo;
}

void AGS_Seeker::Server_SetComboInputFlag_Implementation(bool InputCombo)
{
	CanAcceptComboInput = InputCombo;
}

void AGS_Seeker::ComboInputOpen()
{
	CanAcceptComboInput = true;
}

void AGS_Seeker::ComboInputClose()
{
	if (HasAuthority())
	{
		CanAcceptComboInput = false;
		if (bNextCombo)
		{
			ServerAttackMontage();
			Server_SetNextComboFlag(false);
		}
	}
}

float AGS_Seeker::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// 빈사 상태면 모든 데미지 및 피격 반응 무시 (무적)
	if (bIsInDyingState)
	{
		return 0.0f;
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AGS_Seeker::Server_OnComboAttack_Implementation()
{
	// 빈사 상태에서는 공격 불가
	if (bIsInDyingState)
	{
		return;
	}

	if (!CanAcceptComboInput) // Handler 에서도 검사하고 있었는데 서버에서도 검사한다. 이중검사가 필요한가?
	{
		UE_LOG(LogTemp, Warning, TEXT("Server_OnComboAttack, CanAcceptComboInput == false"));
		return;
	}

	if (!GetSkillComp()->IsSkillAllowed(ESkillSlot::Combo))
	{
		UE_LOG(LogTemp, Warning, TEXT("Server_OnComboAttack, IsSkillAllowed == false"));
		return;
	}
		
	if (CurrentComboIndex == 0)
	{
		GetWorldTimerManager().ClearTimer(AttackSoundResetTimerHandle);
		ServerAttackMontage();
	}
	else
	{
		Server_SetNextComboFlag(true);
		Server_SetComboInputFlag(false); // server 함수의 호출을 막기 위해서 합친 함수를 만들어야 하나?
	}
}

void AGS_Seeker::SetMoveControlValue(bool bMoveForward, bool bMoveRight)
{
	if (AGS_TpsController* TPSController = Cast<AGS_TpsController>(GetController()))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetMoveControlValue"));
		TPSController->SetMoveControlValue(bMoveRight, bMoveForward);
	}
}

void AGS_Seeker::SetLookControlValue(bool bLookUp, bool bLookRight)
{
	if (AGS_TpsController* TPSController = Cast<AGS_TpsController>(GetController()))
	{
		TPSController->SetLookControlValue(bLookRight, bLookUp);
	}
}

FName AGS_Seeker::GetManualRowName_Implementation() const
{
	return ManualRowName;
}

void AGS_Seeker::UpdatePostProcessEffect(float EffectStrength)
{
    if (LowHealthEffectComp)
    {
        LowHealthEffectComp->ApplyStrength(EffectStrength);
    }
}

void AGS_Seeker::ServerAttackMontage_Implementation()
{
	Multicast_SetMontageSlot(ESeekerMontageSlot::FullBody);
	MulticastPlayComboSection(CurrentComboIndex);
}

void AGS_Seeker::MulticastPlayComboSection_Implementation(int32 ComboIndex)
{
	FName SectionName = FName(*FString::Printf(TEXT("Attack%d"), ComboIndex + 1));

	if (UGS_SeekerAnimInstance* AnimInstance = Cast<UGS_SeekerAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->Montage_Play(ComboAnimMontage);
		AnimInstance->Montage_JumpToSection(SectionName, ComboAnimMontage);

		if (HasAuthority())
		{
			CurrentComboIndex++;
			CanAcceptComboInput = false;
			bNextCombo = false;
		}
	}
}

void AGS_Seeker::HandleLowHealthEffect(UGS_StatComp* InStatComp)
{
    if (!IsLocallyControlled() || !InStatComp)
	{
		return;
	}
    if (LowHealthEffectComp)
    {
        LowHealthEffectComp->OnHealthChanged(InStatComp->GetCurrentHealth(), InStatComp->GetMaxHealth());
    }
}

void AGS_Seeker::UpdateLowHealthEffect(){}

void AGS_Seeker::OnRep_SeekerGait()
{
	if (UGS_SeekerAnimInstance* AnimInstance = Cast<UGS_SeekerAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		if (UGS_ChooserInputObj* InputObj = AnimInstance->ChooserInputObj)
		{
			InputObj->Gait = SeekerGait;
		}
	}
}

void AGS_Seeker::Multicast_SetMontageSlot_Implementation(ESeekerMontageSlot InputMontageSlot)
{
	if (UGS_SeekerAnimInstance* AnimInstance = Cast<UGS_SeekerAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->SetCurMontageSlot(InputMontageSlot);
	}
}

void AGS_Seeker::Multicast_SetMustTurnInPlace_Implementation(bool MustTurn)
{
	if (UGS_SeekerAnimInstance* AnimInstance = Cast<UGS_SeekerAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->SetMustTurnInPlace(MustTurn);
	}
}

/*void AGS_Seeker::Multicast_SetIsFullBodySlot_Implementation(bool bFullBodySlot)
{
	if (!IsValid(this) || !GetWorld() || GetWorld()->bIsTearingDown || GetWorld()->IsInSeamlessTravel())
	{
		return;
	}

	if (UGS_SeekerAnimInstance* AnimInstance = Cast<UGS_SeekerAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->IsPlayingFullBodyMontage = bFullBodySlot;
	}
}*/

/*void AGS_Seeker::Multicast_SetIsUpperBodySlot_Implementation(bool bUpperBodySlot)
{
	if (!IsValid(this) || !GetWorld() || GetWorld()->bIsTearingDown || GetWorld()->IsInSeamlessTravel())
	{
		return;
	}

	if (UGS_SeekerAnimInstance* AnimInstance = Cast<UGS_SeekerAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->IsPlayingUpperBodyMontage = bUpperBodySlot;
	}
}*/

void AGS_Seeker::OnRep_IsLowHealthEffectActive()
{
    if (LowHealthPostProcessComp)
    {
        LowHealthPostProcessComp->bEnabled = bIsLowHealthEffectActive;
    }
}

void AGS_Seeker::OnRep_CurrentEffectStrength()
{
    UpdatePostProcessEffect(CurrentEffectStrength);
}

// ============================
// 상태 전환에 따른 음악 함수 관련
// ============================

// 몬스터 감지 시스템 (시커가 몬스터를 감지)
void AGS_Seeker::OnCombatTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA(AGS_Monster::StaticClass()))
	{
		if (AGS_Monster* Monster = Cast<AGS_Monster>(OtherActor))
		{
			AddCombatMonster(Monster);
			
			if (UGS_HPTextWidgetComp* HPWidgetComp = Monster->FindComponentByClass<UGS_HPTextWidgetComp>())
			{
				HPWidgetComp->SetVisibility(true);
			}
		}
	}
}

void AGS_Seeker::OnCombatTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && OtherActor->IsA(AGS_Monster::StaticClass()))
	{
		if (AGS_Monster* Monster = Cast<AGS_Monster>(OtherActor))
		{
			RemoveCombatMonster(Monster);

			if (UGS_HPTextWidgetComp* HPWidgetComp = Monster->FindComponentByClass<UGS_HPTextWidgetComp>())
			{
				HPWidgetComp->SetVisibility(false);
			}
		}
	}
}

void AGS_Seeker::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 상태 초기화
	StateReset();
}

void AGS_Seeker::AddCombatMonster(AGS_Monster* Monster)
{
	if (!IsValid(Monster))
	{
		return;
	}

	// TWeakObjectPtr 배열이므로 수동으로 중복 확인
	bool bAlreadyContains = false;
	for (const TWeakObjectPtr<AGS_Monster>& Ptr : NearbyMonsters)
	{
		if (Ptr.Get() == Monster)
		{
			bAlreadyContains = true;
			break;
		}
	}

	if (!bAlreadyContains)
	{
		NearbyMonsters.Add(TWeakObjectPtr<AGS_Monster>(Monster));
		
		// 몬스터 사망 시 호출될 델리게이트 바인딩
		Monster->OnMonsterDead.AddUniqueDynamic(this, &AGS_Seeker::HandleMonsterDeath);

		// 첫 번째 몬스터가 추가되면 음악 시작 (서버에서 클라이언트로 명령)
		if (NearbyMonsters.Num() == 1)
		{
			StartCombatMusic();
		}
	}
}

void AGS_Seeker::ClearNearbyMonsters()
{
	for (int32 i = NearbyMonsters.Num() - 1; i >= 0; --i)
	{
		if (NearbyMonsters[i].IsValid())
		{
			NearbyMonsters[i]->OnMonsterDead.RemoveDynamic(this, &AGS_Seeker::HandleMonsterDeath);
		}
	}
	NearbyMonsters.Reset();
}

void AGS_Seeker::RemoveCombatMonster(AGS_Monster* Monster)
{
	if (!Monster) return;

	NearbyMonsters.RemoveAll([Monster](const TWeakObjectPtr<AGS_Monster>& Ptr) {
		return Ptr.Get() == Monster;
	});
	Monster->OnMonsterDead.RemoveDynamic(this, &AGS_Seeker::HandleMonsterDeath);

	// 모든 몬스터가 제거되면 음악 중지
	if (NearbyMonsters.Num() == 0)
	{
		ClientRPCStopCombatMusic();
	}
}

void AGS_Seeker::HandleMonsterDeath(AGS_Monster* DeadMonster)
{
	RemoveCombatMonster(DeadMonster);
}

void AGS_Seeker::StartCombatMusic()
{
	// 로컬 제어 확인
	if (!IsLocallyControlled())
	{
		return;
	}

	// 무효한 몬스터 제거 (TWeakObjectPtr이므로 유효성 체크만 수행)
	NearbyMonsters.RemoveAll([](const TWeakObjectPtr<AGS_Monster>& M) { return !M.IsValid(); });

	if (NearbyMonsters.Num() == 0)
	{
		return;
	}

	// AudioManager 가져오기
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UGS_AudioManager* AudioManager = GameInstance->GetSubsystem<UGS_AudioManager>())
		{
            UAkAudioEvent* CombatStartEvent = nullptr;
            UAkAudioEvent* CombatStopEvent = nullptr;

            // 유효한 이벤트를 가진 몬스터를 우선 탐색
            for (const TWeakObjectPtr<AGS_Monster>& MonsterPtr : NearbyMonsters)
            {
                AGS_Monster* Monster = MonsterPtr.Get();
                if (!IsValid(Monster))
                {
                    continue;
                }
                if (Monster->CombatMusicEvent)
                {
                    CombatStartEvent = Monster->CombatMusicEvent;
                    CombatStopEvent = Monster->CombatMusicStopEvent;
                    break;
                }
            }

            if (CombatStartEvent)
            {
                AudioManager->StartCombatSequence(this, CombatStartEvent, CombatStopEvent);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[Seeker] StartCombatMusic - 유효한 CombatMusicEvent가 없습니다. (NearbyMonsters: %d)"), NearbyMonsters.Num());
            }
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Seeker] StartCombatMusic - AudioManager를 찾을 수 없습니다!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Seeker] StartCombatMusic - GameInstance를 찾을 수 없습니다!"));
	}
}

void AGS_Seeker::ClientRPCStopCombatMusic_Implementation()
{
	// AudioManager 가져오기
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UGS_AudioManager* AudioManager = GameInstance->GetSubsystem<UGS_AudioManager>())
		{
			// 현재 재생 중인 전투 BGM 이벤트 가져오기 (가장 마지막에 추가된 몬스터 기준 또는 다른 로직)
			UAkAudioEvent* CombatStopEventToUse = nullptr;
			if (AudioManager->GetCurrentCombatMusicStopEvent()) // AudioManager에 저장된 StopEvent가 우선
			{
				CombatStopEventToUse = AudioManager->GetCurrentCombatMusicStopEvent();
			}
			else if (!NearbyMonsters.IsEmpty() && NearbyMonsters.Last().IsValid() && NearbyMonsters.Last().Get()->CombatMusicStopEvent) // 몬스터 배열에서 가져오기
			{
				CombatStopEventToUse = NearbyMonsters.Last().Get()->CombatMusicStopEvent;
			}

			// EndCombatSequence 호출 시 CombatStopEvent도 전달
			AudioManager->EndCombatSequence(this, CombatStopEventToUse);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("AGS_Seeker::StopCombatMusic() - AudioManager not found"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AGS_Seeker::StopCombatMusic() - GameInstance not found"));
	}
}


void AGS_Seeker::OnDeath()
{
	// 사망 시 빈사 상태 효과 확실히 제거
	if (HasAuthority())
	{
		Multicast_DeactivateDyingFlame();
		bIsInDyingState = false;
	}

	// LowHP Pain 사운드 즉시 중지
	if (SeekerAudioComponent)
	{
		SeekerAudioComponent->StopLowHPPainSound();
	}

	// Death 사운드는 부모 클래스(GS_Character::OnDeath)에서 통합 처리됨
	Super::OnDeath();

	ClientRPCStopCombatMusic();
	ClearNearbyMonsters();
}

void AGS_Seeker::HandleAliveStatusChanged(AGS_PlayerState* ChangedPlayerState, bool bIsNowAlive)
{
	if (!IsLocallyControlled()) 
	{
		return;
	}

	// 자신의 PlayerState인지 확인
	AGS_PlayerState* MyPlayerState = GetPlayerState<AGS_PlayerState>();
	if (ChangedPlayerState != MyPlayerState) 
	{
		return;
	}

	if (!bIsNowAlive) // 자신이 죽었을 때
	{
		ClientRPCStopCombatMusic();
		ClearNearbyMonsters();
	}
}

void AGS_Seeker::TransWeaponHandingState(EWeaponHandlingState RequiredCurState, EWeaponHandlingState NextState,
	UAnimMontage* TargetAM, ESeekerMontageSlot TargetMontageSlot)
{
	if (WeaponHandlingState == RequiredCurState)
	{
		GetSkillComp()->SetCurAllowedSkillsMask(0);
		Multicast_SetMontageSlot(TargetMontageSlot);
		Multicast_PlaySkillMontage(TargetAM);
		SetWeaponHandlingState(NextState);
	}
}

void AGS_Seeker::Server_RestKey_Implementation()
{
	this->StateReset();
}

void AGS_Seeker::Multicast_PlaySound_Implementation(UAkAudioEvent* SoundToPlay)
{
	if (SeekerAudioComponent && IsValid(SeekerAudioComponent))
	{
		SeekerAudioComponent->PlayGenericSound(SoundToPlay);
	}

}

void AGS_Seeker::OnHoverBegin()
{
	Super::OnHoverBegin();

	OnSeekerHover.Broadcast(true);
}

void AGS_Seeker::OnHoverEnd()
{
	Super::OnHoverEnd();

	OnSeekerHover.Broadcast(false);
}

FLinearColor AGS_Seeker::GetCurrentDecalColor()
{
	return FLinearColor::Red;
}

bool AGS_Seeker::ShowDecal()
{
	return true;
}

// ================
// 가디언 감지 시스템
// ================

void AGS_Seeker::OnDetectedByGuardian(bool bIsDetected)
{
	// 서버에서만 호출되어야 함
	if (HasAuthority())
	{
		// 상태 변경 시 자동으로 OnRep_IsDetectedByGuardian이 모든 클라이언트에서 호출됨
		bIsDetectedByGuardian = bIsDetected;

		// 리슨 서버인 경우 본인(서버 플레이어)을 위해 직접 호출
		if (IsLocallyControlled())
		{
			OnRep_IsDetectedByGuardian();
		}

		// 감지 해제 시 강도도 0으로 초기화
		if (!bIsDetected)
		{
			DetectionIntensity = 0.0f;
		}
	}
}

void AGS_Seeker::SetDetectionIntensity(float Intensity)
{
	if (HasAuthority())
	{
		DetectionIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
	}
}

void AGS_Seeker::OnRep_IsDetectedByGuardian()
{
	// 로컬 플레이어의 시커에만 효과 적용
	if (!IsLocallyControlled())
	{
		return;
	}

    // 시각적 효과 업데이트 (항상 실행)
    UpdateDetectionEffects();

	// 델리게이트 알림 (블루프린트 UI용)
	OnDetectedByGuardianChanged.Broadcast(bIsDetectedByGuardian);

	// 청각적 피드백
	if (!SeekerAudioComponent)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	float CurrentTime = World->GetTimeSeconds();

    if (bIsDetectedByGuardian)
	{
		// 입장 감지 사운드 제한 적용
		float TimeSinceLastSound = CurrentTime - LastDetectionSoundTime;
		if (TimeSinceLastSound >= DetectionSoundCooldown)
		{
			SeekerAudioComponent->PlayDetectionWarningSound();
            LastDetectionSoundTime = CurrentTime;
		}
	}
	else
	{
		// 퇴장 감지 사운드 제한 적용
		float TimeSinceLastExitSound = CurrentTime - LastExitDetectionSoundTime;
		if (TimeSinceLastExitSound >= ExitDetectionSoundCooldown)
		{
			SeekerAudioComponent->PlayDetectionClearedSound();
			LastExitDetectionSoundTime = CurrentTime;
		}
	}
}

void AGS_Seeker::OnRep_DetectionIntensity()
{
	// 로컬 플레이어의 시커에만 포스트 프로세스 효과 적용
	if (!IsLocallyControlled())
	{
		return;
	}

	// 포스트 프로세스 효과 강도 업데이트
    UpdateDetectionPostProcessEffect(DetectionIntensity);
}

void AGS_Seeker::UpdateDetectionEffects()
{
	if (bIsDetectedByGuardian)
	{
		// 감지되었을 때 - 블루프린트에서 HUD 위젯 표시
        UpdateDetectionHUD(true);

		// 감지 전용 포스트 프로세스 활성화
        if (DetectionEffectComp)
        {
            DetectionEffectComp->OnDetectedChanged(true);
        }
	}
	else
	{
		// 감지 해제 시 - 블루프린트에서 HUD 위젯 숨김
        UpdateDetectionHUD(false);

		// 감지 전용 포스트 프로세스 비활성화
        if (DetectionEffectComp)
        {
            DetectionEffectComp->OnDetectedChanged(false);
        }
	}
}

void AGS_Seeker::UpdateDetectionPostProcessEffect(float Intensity)
{
    if (!IsLocallyControlled())
	{
		return;
	}

    if (DetectionEffectComp)
    {
        DetectionEffectComp->SetIntensity(Intensity);
    }
}

void AGS_Seeker::UpdateDetectionHUD_Implementation(bool bIsDetected)
{
	// 로컬 플레이어가 아닌 경우 UI 처리를 하지 않음
	if (!IsLocallyControlled())
	{
		return;
	}

	if (bIsDetected)
	{
		// 감지되었을 때 UI 표시
		if (!DetectionHUDWidget && DetectionHUDWidgetClass)
		{
			DetectionHUDWidget = CreateWidget<UUserWidget>(GetWorld(), DetectionHUDWidgetClass);
			if (DetectionHUDWidget)
			{
				DetectionHUDWidget->AddToViewport(100); // UI가 다른 요소에 가려지지 않도록 ZOrder 설정
			}
		}
		else if (DetectionHUDWidget)
		{
			DetectionHUDWidget->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else
	{
		// 감지되지 않았을 때 UI 숨김
		if (DetectionHUDWidget)
		{
			DetectionHUDWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

// ==========================================
// 빈사 (Dying) 상태 시스템 구현
// ==========================================

void AGS_Seeker::EnterDyingState()
{
	// 서버에서만 호출
	if (!HasAuthority())
	{
		return;
	}

	// 이미 빈사 상태면 무시
	if (bIsInDyingState)
	{
		return;
	}

	// 빈사 횟수 증가
	CurrentDyingCount++;

	// 최대 빈사 횟수에 도달하면 즉시 사망
	if (CurrentDyingCount >= MaxDyingCount)
	{
		OnDeath();
		return;
	}

	// 빈사 상태 활성화
	bIsInDyingState = true;
	DyingTimeRemaining = MaxDyingTime;
	bIsBeingRevived = false;
	ReviveProgress = 0.0f;
	bDangerSoundPlayed = false;

	// 빈사 상태 업데이트 타이머 시작 (Tick 대신 사용, 20Hz)
	GetWorldTimerManager().SetTimer(DyingUpdateTimerHandle, this, &AGS_Seeker::UpdateDyingStateTimer, 0.05f, true);

	// 현재 Gait 저장
	GaitBeforeDying = SeekerGait;

	// 기어다니기 모드로 전환
	Server_SetSeekerGait(EGait::Crawl);

	// 스킬 사용 불가
	SetCanUseSkill(false);

	// 메르시 등 무기/조준 상태 강제 해제 (애니메이션 정상화를 위해)
	SetDrawState(false);
	SetAimState(false);
	SetIsLockedRotationToController(false); // 회전 고정 해제 추가

	// 몬스터 회피: 물리적 충돌(Pawn)은 유지하여 통과하지 않게 함.
	// 대신 AI 감지(Visibility, Camera) 및 타겟팅(RTSTarget)을 무시하여 공격 대상에서 제외되도록 유도.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore); // RTSTarget

	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

	// 방법 A: 불꽃 효과 활성화 (OnRep에서 처리하도록 변수 복제만 담당, 서버/리슨서버는 직접 호출)
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnRep_IsInDyingState();
	}

	// 위험 사운드 플래그 초기화
	bDangerSoundPlayed = false;

	// LowHP Pain 사운드는 빈사 상태에서도 계속 재생 (더 긴박한 분위기)

	// 델리게이트 브로드캐스트
	OnDyingStateChanged.Broadcast(true, DyingTimeRemaining);
}

void AGS_Seeker::ExitDyingState(bool bWasRevived)
{
	// 서버에서만 호출
	if (!HasAuthority())
	{
		return;
	}

	if (!bIsInDyingState)
	{
		return;
	}

	// 빈사 상태 해제
	bIsInDyingState = false;
	bIsBeingRevived = false;
	ReviveProgress = 0.0f;
	CurrentReviver = nullptr;


	// 몬스터 콜리전 복구 (감지 채널들)
	// 원래 설정값으로 복구해야 하지만, 기본적으로 Block 또는 Overlap일 것이므로 Block으로 설정
	// (프로젝트 설정에 따라 다를 수 있으나, 일반적으로 캐릭터는 Visibility/Camera에 반응함)
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // or Overlap? 보통 캡슐은 Trace Block 함
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Block); // or Ignore? (카메라 줌인 방지 등) -> 일단 Block으로 복구
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); // RTSTarget

	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);

	// 불꽃 효과 비활성화 (OnRep에서 처리하도록 변수 복제만 담당, 서버/리슨서버는 직접 호출)
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnRep_IsInDyingState();
	}

	// 델리게이트 브로드캐스트
	OnDyingStateChanged.Broadcast(false, 0.0f);

	if (bWasRevived)
	{
		// 구조된 경우 - 이전 Gait로 복구
		Server_SetSeekerGait(GaitBeforeDying);
		
		// 스킬 사용 가능
		SetCanUseSkill(true);
	}
	// 사망한 경우는 OnDyingTimeExpired에서 처리
}

void AGS_Seeker::OnRevived()
{
	// 서버에서만 호출
	if (!HasAuthority())
	{
		return;
	}

	// 25% HP로 회복
	if (UGS_StatComp* Stat = GetStatComp())
	{
		float MaxHP = Stat->GetMaxHealth();
		float ReviveHP = MaxHP * ReviveHealthPercent;
		Stat->SetCurrentHealth(ReviveHP, true);  // true = healing
	}

	// 빈사 상태 해제
	ExitDyingState(true);
}

void AGS_Seeker::OnDyingTimeExpired()
{
	// 서버에서만 호출
	if (!HasAuthority())
	{
		return;
	}

	// 빈사 상태 해제 (구조 실패)
	bIsInDyingState = false;
	bIsBeingRevived = false;
	ReviveProgress = 0.0f;
	CurrentReviver = nullptr;

	// 실제 사망 처리
	OnDeath();
}

void AGS_Seeker::UpdatePeripheralSensor()
{
	// 1. 블루프린트에서 추가 기능을 수행할 수 있도록 이벤트 호출
	OnPeripheralSensorUpdate();

	// 2. 주변 보물상자 감지 및 시각 효과 (C++)
	CheckNearbyEmberChests();

	// 3. 부모 클래스(AGS_Player)의 이름표 회전 기능을 여기서 저빈도로 수행 (Tick 대신)
	if (IsValid(SteamNameWidgetComp))
	{
		UpdateSteamNameWidgetRotation();
	}

	// 4. 로컬 컨트롤러의 상호작용 로직 업데이트 (Tick이 꺼져 있으므로 여기서 명시적 호출)
	if (IsLocallyControlled())
	{
		if (AGS_TpsController* TPSController = Cast<AGS_TpsController>(GetController()))
		{
			TPSController->UpdateNearbyInteractable();
		}
	}
}

void AGS_Seeker::UpdateDyingStateTimer()
{
	// 고정 시간 간격 (0.05s)
	const float DeltaTime = 0.05f;

	if (bIsInDyingState)
	{
		// 서버: 타이머 로직 처리
		if (HasAuthority())
		{
			UpdateDyingState(DeltaTime);
		}

		// 로컬 전용 포스트 프로세스 효과
		if (IsLocallyControlled())
		{
			UpdateDyingPostProcessEffect();
		}

		// 비주얼 및 경고 사운드 업데이트 (모든 클라이언트)
		UpdateDyingFlameVisuals(DyingTimeRemaining);

		// 로컬 클라이언트 UI 업데이트를 위한 방송 (남은 시간 실시간 갱신)
		if (IsLocallyControlled())
		{
			OnDyingStateChanged.Broadcast(true, DyingTimeRemaining);
		}
	}
	else
	{
		// 상태가 해제되었으면 타이머 종료
		GetWorldTimerManager().ClearTimer(DyingUpdateTimerHandle);
	}
}

void AGS_Seeker::CheckNearbyEmberChests()
{
	// 로컬 플레이어만 시각 효과(아웃라인 등)를 처리하면 됨
	if (!IsLocallyControlled())
	{
		return;
	}

	// 1. 주변 액터 검색 (오버랩 기반)
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors, AGS_EmberChest::StaticClass());

	AGS_EmberChest* ClosestChest = nullptr;
	float MinDistanceSq = TNumericLimits<float>::Max();

	for (AActor* Actor : OverlappingActors)
	{
		AGS_EmberChest* Chest = Cast<AGS_EmberChest>(Actor);
		if (Chest && Chest->CurrentState == EEmberChestState::Idle)
		{
			float DistanceSq = FVector::DistSquared(GetActorLocation(), Chest->GetActorLocation());
			if (DistanceSq < MinDistanceSq)
			{
				MinDistanceSq = DistanceSq;
				ClosestChest = Chest;
			}
		}
	}

	// 2. 감지된 상자가 변경되었을 때만 처리
	if (ClosestChest != CurrentDetectedChest.Get())
	{
		// 이전 상자의 하이라이트 끄기
		if (CurrentDetectedChest.IsValid())
		{
			CurrentDetectedChest->SetHighlight(false);
		}

		// 새 상자의 하이라이트 켜기
		if (ClosestChest)
		{
			ClosestChest->SetHighlight(true);
		}

		CurrentDetectedChest = ClosestChest;
	}
}

void AGS_Seeker::UpdateDyingState(float DeltaTime)
{
	// 서버에서만 호출
	if (!HasAuthority() || !bIsInDyingState)
	{
		return;
	}

	// 구조 중이어도 죽음 타이머는 계속 감소
	DyingTimeRemaining -= DeltaTime;

	if (DyingTimeRemaining <= 0.0f)
	{
		DyingTimeRemaining = 0.0f;
		OnDyingTimeExpired();
		return;
	}

	// 구조 중이면 구조 진행도 업데이트
	if (bIsBeingRevived)
	{
		UpdateReviveProgress(DeltaTime);
	}

	// 빈사 상태 로직 (서버 사이드: 진행도 및 타이머만 관리)
	// 비주얼 및 경고음은 Tick -> UpdateDyingFlameVisuals (로컬) 에서 처리됨
}

void AGS_Seeker::UpdateDyingPostProcessEffect()
{
	// 로컬 플레이어만
	if (!IsLocallyControlled() || !DyingPostProcessComp)
	{
		return;
	}

	if (bIsInDyingState)
	{
		DyingPostProcessComp->bEnabled = true;

		// 남은 시간에 따라 효과 강도 조절 (시간이 적을수록 강해짐)
		float TimeRatio = DyingTimeRemaining / MaxDyingTime;
		float EffectStrength = 1.0f - TimeRatio;  // 0~1 범위

		if (DyingDynamicMaterial)
		{
			DyingDynamicMaterial->SetScalarParameterValue(TEXT("EffectStrength"), EffectStrength);
		}
	}
	else
	{
		DyingPostProcessComp->bEnabled = false;
	}
}

void AGS_Seeker::Server_StartRevive_Implementation(AGS_Seeker* Reviver)
{
	if (!bIsInDyingState || bIsBeingRevived)
	{
		return;
	}

	if (!IsValid(Reviver) || Reviver == this)
	{
		return;
	}

	// 구조 시작 시 진행도 감소 중지
	StopReviveDecay();

	bIsBeingRevived = true;
	// ReviveProgress는 유지 (점진적 감소로 남아있던 값)
	CurrentReviver = Reviver;
}

void AGS_Seeker::Server_CancelRevive_Implementation()
{
	if (!bIsBeingRevived)
	{
		return;
	}

	bIsBeingRevived = false;
	CurrentReviver = nullptr;

	// 즉시 초기화 대신 점진적 감소 시작
	StartReviveDecay();
}

bool AGS_Seeker::CanContinueRevive() const
{
	// 구조자 유효성 확인
	if (!CurrentReviver.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Revive] 구조자가 유효하지 않음"));
		return false;
	}

	// 거리 확인
	if (!IsReviverInRange(CurrentReviver.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Revive] 구조자가 범위 밖으로 나감"));
		return false;
	}

	// E키 홀드 상태 확인
	if (!IsReviverValid(CurrentReviver.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Revive] E키가 떼어짐"));
		return false;
	}

	return true;
}

void AGS_Seeker::UpdateReviveProgress(float DeltaTime)
{
	if (!HasAuthority() || !bIsBeingRevived)
	{
		return;
	}

	// 1. 구조 진행 조건 검증
	if (!CanContinueRevive())
	{
		Server_CancelRevive();
		return;
	}

	// 2. 진행도 업데이트
	ReviveProgress += DeltaTime / ReviveTime;
	ReviveProgress = FMath::Clamp(ReviveProgress, 0.0f, 1.0f);

	// 3. UI 업데이트
	OnReviveProgressChanged.Broadcast(ReviveProgress);

	// 4. 완료 확인
	if (ReviveProgress >= 1.0f)
	{
		CompleteRevive();
	}
}

void AGS_Seeker::CompleteRevive()
{
	if (!HasAuthority())
	{
		return;
	}

	// 구조 완료 처리
	OnRevived();
}

void AGS_Seeker::StartReviveDecay()
{
	// 서버에서만 호출
	if (!HasAuthority())
	{
		return;
	}

	// 이미 감소 중이면 무시
	if (bIsReviveDecaying)
	{
		return;
	}

	// 진행도가 0이면 시작 안 함
	if (ReviveProgress <= 0.0f)
	{
		return;
	}

	bIsReviveDecaying = true;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(
			ReviveDecayTimerHandle,
			this,
			&AGS_Seeker::OnReviveDecayTick,
			REVIVE_DECAY_TICK_INTERVAL,  // 100ms마다 업데이트
			true   // 반복
		);
	}
}

void AGS_Seeker::StopReviveDecay()
{
	// 서버에서만 호출
	if (!HasAuthority())
	{
		return;
	}

	if (!bIsReviveDecaying)
	{
		return;
	}

	bIsReviveDecaying = false;

	UWorld* World = GetWorld();
	if (World && ReviveDecayTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(ReviveDecayTimerHandle);
	}
}

void AGS_Seeker::OnReviveDecayTick()
{
	// 서버에서만 호출
	if (!HasAuthority() || !bIsReviveDecaying)
	{
		return;
	}

	// 진행도 감소 (0.25 * 0.1초 = 초당 0.25 감소 = 4초에 0%)
	ReviveProgress -= ReviveDecayRate * REVIVE_DECAY_TICK_INTERVAL;
	ReviveProgress = FMath::Max(ReviveProgress, 0.0f);

	// 진행도 변화 델리게이트 브로드캐스트 (UI 업데이트)
	OnReviveProgressChanged.Broadcast(ReviveProgress);

	// 0%에 도달하면 감소 중지
	if (ReviveProgress <= 0.0f)
	{
		StopReviveDecay();
	}
}

// ========================================
// 헬퍼 함수 구현
// ========================================

void AGS_Seeker::SafeClearTimer(FTimerHandle& TimerHandle)
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

bool AGS_Seeker::IsReviverInRange(const AGS_Seeker* Reviver) const
{
	if (!IsValid(Reviver))
	{
		return false;
	}

	const float Distance = FVector::Dist(GetActorLocation(), Reviver->GetActorLocation());
	return Distance <= MaxReviveDistance;
}

bool AGS_Seeker::IsReviverValid(const AGS_Seeker* Reviver) const
{
	if (!IsValid(Reviver))
	{
		return false;
	}

	const AGS_TpsController* ReviverController = Cast<AGS_TpsController>(Reviver->GetController());
	if (!ReviverController)
	{
		return false;
	}

	return ReviverController->IsHoldingReviveKey();
}

void AGS_Seeker::OnRep_IsInDyingState()
{
	// 클라이언트에서 빈사 상태 로직을 위한 타이머 관리 (Tick 대신 사용)
	if (bIsInDyingState)
	{
		if (!GetWorldTimerManager().IsTimerActive(DyingUpdateTimerHandle))
		{
			GetWorldTimerManager().SetTimer(DyingUpdateTimerHandle, this, &AGS_Seeker::UpdateDyingStateTimer, 0.05f, true);
		}
	}
	else
	{
		GetWorldTimerManager().ClearTimer(DyingUpdateTimerHandle);
	}

	// 클라이언트에서 빈사 상태 변화 처리 (UI 및 효과)
	if (IsLocallyControlled())
	{
		UpdateDetectionEffects();
		
		if (bIsInDyingState)
		{
			// 빈사 상태 화면 효과 활성화
			if (DyingPostProcessComp)
			{
				DyingPostProcessComp->bEnabled = true;

				// 동적 머티리얼 생성 (첫 진입 시)
				if (!DyingDynamicMaterial && DyingEffectMaterial)
				{
					DyingDynamicMaterial = UMaterialInstanceDynamic::Create(DyingEffectMaterial, this);
					DyingPostProcessComp->Settings.WeightedBlendables.Array.Empty();
					DyingPostProcessComp->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, DyingDynamicMaterial));
				}
			}
		}
		else
		{
			// 빈사 상태 화면 효과 비활성화
			if (DyingPostProcessComp)
			{
				DyingPostProcessComp->bEnabled = false;
			}
		}
	}

	// 불꽃 효과 동기화 (모든 플레이어에게 보임)
	if (bIsInDyingState)
	{
		ActivateDyingFlameEffects();
	}
	else
	{
		DeactivateDyingFlameEffects();
	}

	// 델리게이트 브로드캐스트 (UI 업데이트용 - 모든 클라이언트/로컬 공통)
	OnDyingStateChanged.Broadcast(bIsInDyingState, bIsInDyingState ? DyingTimeRemaining : 0.0f);
}

void AGS_Seeker::OnRep_IsDead()
{
	Super::OnRep_IsDead();

	// 사망 시 고통 소리(LowHP Pain) 즉시 중지 (클라이언트 동기화)
	if (SeekerAudioComponent && IsDead())
	{
		SeekerAudioComponent->StopLowHPPainSound();
	}
}

void AGS_Seeker::OnRep_IsBeingRevived()
{
	// 구조 상태 변화 시 UI 업데이트 등 처리
	if (!bIsBeingRevived)
	{
		// 구조 취소됨 - 진행도 초기화 (삭제: 점진적 감소를 위해 0으로 초기화하지 않음)
		// OnReviveProgressChanged.Broadcast(0.0f);
	}
}

void AGS_Seeker::OnRep_ReviveProgress()
{
	// 구조 진행도 UI 업데이트
	OnReviveProgressChanged.Broadcast(ReviveProgress);
}// ========================================
// 빈사 상태 불꽃 효과 구현
// ========================================

void AGS_Seeker::ActivateDyingFlameEffects()
{
	// Niagara 컴포넌트 활성화
	if (DyingFlameEffectComp && DyingFlameEffectComp->GetAsset())
	{
		DyingFlameEffectComp->Activate();
	}

	if (DyingMagicCircleComp && DyingMagicCircleComp->GetAsset())
	{
		DyingMagicCircleComp->Activate();
	}

	// 불꽃 발동 사운드 재생 (Spawn + Loop)
	if (SeekerAudioComponent)
	{
		SeekerAudioComponent->PlayDyingFlameSpawnSound();
		SeekerAudioComponent->PlayDyingFlameLoopSound();
	}
}

void AGS_Seeker::DeactivateDyingFlameEffects()
{
	// Niagara 컴포넌트 비활성화
	if (DyingFlameEffectComp && DyingFlameEffectComp->IsActive())
	{
		DyingFlameEffectComp->Deactivate();
	}

	if (DyingMagicCircleComp && DyingMagicCircleComp->IsActive())
	{
		DyingMagicCircleComp->Deactivate();
	}

	// 불꽃 사운드 정리 (Loop Stop + End Sound)
	if (SeekerAudioComponent)
	{
		SeekerAudioComponent->StopDyingFlameLoopSound();
		SeekerAudioComponent->PlayDyingFlameEndSound();
	}
}

void AGS_Seeker::UpdateDyingFlameVisuals(float TimeRemaining)
{
	if (!DyingFlameEffectComp && !DyingMagicCircleComp)
	{
		return;
	}

	// 남은 시간 비율 계산 (1.0 ~ 0.0)
	float TimeRatio = FMath::Clamp(TimeRemaining / MaxDyingTime, 0.0f, 1.0f);

	// 불꽃 크기 스케일 (시간이 지날수록 작아짐)
	float FlameScale = FMath::Lerp(0.3f, 1.0f, TimeRatio); // 30% ~ 100%

	if (DyingFlameEffectComp)
	{
		// Niagara 파라미터로 크기 조절 (에셋에 "FlameScale" 파라미터 필요)
		DyingFlameEffectComp->SetFloatParameter(FName("FlameScale"), FlameScale);
	}

	if (DyingMagicCircleComp)
	{
		// 마법진 크기도 동일하게 조절 (에셋에 "CircleScale" 파라미터 필요)
		DyingMagicCircleComp->SetFloatParameter(FName("CircleScale"), FlameScale);
	}

	// 10초 이하일 때 UI 경고음 재생 (한 번만)
	if (IsLocallyControlled() && TimeRemaining <= 10.0f && TimeRemaining > 0.0f)
	{
		// 아직 재생하지 않았을 때만 재생 (-1: 미재생 상태)
		if (LastDyingWarningSecond == -1)
		{
			if (SeekerAudioComponent)
			{
				SeekerAudioComponent->PlayDyingTimerWarningSound();
			}
			// 재생 완료 표시
			LastDyingWarningSecond = 1; 
		}
	}
	else if (TimeRemaining > 10.0f)
	{
		// 10초 넘어가면 리셋 (구조 등으로 인해 시간이 늘어난 경우)
		LastDyingWarningSecond = -1;
	}
}

void AGS_Seeker::Multicast_ActivateDyingFlame_Implementation()
{
	ActivateDyingFlameEffects();
}

void AGS_Seeker::Multicast_DeactivateDyingFlame_Implementation()
{
	DeactivateDyingFlameEffects();
}

float AGS_Seeker::GetOptimalCullDistance() const
{
	// Seeker는 중간 크기 컬링 거리 (45m)
	return GS_Rendering::MONSTER_MEDIUM_CULL_DISTANCE;
}
