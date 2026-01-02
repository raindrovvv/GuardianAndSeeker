#include "Character/GS_Character.h"
#include "SignificanceManager.h"
#include "AI/RTS/GS_RTSController.h"
#include "AkGameplayStatics.h"
#include "Character/Component/GS_CameraShakeComponent.h"
#include "Character/Component/GS_DebuffComp.h"
#include "Character/Component/GS_DrakharAudioComponent.h"
#include "Character/Component/GS_HitReactComp.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/F_GS_DamageEvent.h"
#include "Character/Player/GS_Player.h"
#include "Character/Player/Guardian/GS_Drakhar.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Components/DecalComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Sound/GS_MonsterAudioComponent.h"
#include "Sound/GS_SeekerAudioComponent.h"
#include "System/GS_PlayerState.h"
#include "System/Utility/GS_AssetLoader.h"
#include "UI/Character/GS_HPText.h"
#include "UI/Character/GS_HPTextWidgetComp.h"
#include "UI/Character/GS_HPWidget.h"
#include "UI/Character/GS_PlayerInfoWidget.h"
#include "VFX/GS_VFX_FunctionLibrary.h"
#include "Weapon/GS_Weapon.h"

AGS_Character::AGS_Character(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	StatComp = ObjectInitializer.CreateDefaultSubobject<UGS_StatComp>(this, TEXT("StatComp"));
	DebuffComp = ObjectInitializer.CreateDefaultSubobject<UGS_DebuffComp>(this, TEXT("DebuffComp"));
	HitReactComp = ObjectInitializer.CreateDefaultSubobject<UGS_HitReactComp>(this, TEXT("HitReactComp"));
	CameraShakeComp =
	    ObjectInitializer.CreateDefaultSubobject<UGS_CameraShakeComponent>(this, TEXT("CameraShakeComp"));

	HPTextWidgetComp =
	    ObjectInitializer.CreateDefaultSubobject<UGS_HPTextWidgetComp>(this, TEXT("TextWidgetComp"));
	HPTextWidgetComp->SetupAttachment(RootComponent);
	HPTextWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	HPTextWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HPTextWidgetComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	HPTextWidgetComp->SetVisibility(false);
	// HPTextWidgetComp->SetDrawAtDesiredSize(true);
	HPTextWidgetComp->SetCullDistance(2000.0f);

	SelectionDecal =
	    ObjectInitializer.CreateDefaultSubobject<UDecalComponent>(this, TEXT("SelectionDecal"));
	SelectionDecal->SetupAttachment(RootComponent);
	SelectionDecal->SetVisibility(false);

	bIsDead = false;
	bIsHovered = false;
	bIsInvincible = false;
}

void AGS_Character::BeginPlay()
{
	Super::BeginPlay();

	bIsInvincible = false;

	// Set Default Stats to Character
	const UEnum* CharacterEnum = StaticEnum<ECharacterType>();
	bool bStatInitialized = false;

	if (CharacterEnum)
	{
		FString EnumToName =
		    CharacterEnum->GetNameStringByValue((int64)CharacterType);
		StatComp->InitStat(FName(EnumToName));
		bStatInitialized = true;
	}
	if (bStatInitialized)
	{
		AGS_PlayerState* PS = GetPlayerState<AGS_PlayerState>();
		if (PS)
		{
			if (PS->CurrentPlayerRole == EPlayerRole::PR_Seeker)
			{
				PS->OnPawnStatInitialized();
			}
		}
	}

	// Set HP 3D widget (monster)
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (IsValid(HPTextWidgetComp))
		{
			// HP 위젯 거리 기반 컬링 설정 (RTS 시점 고려)
			float CullDistance = GS_Rendering::CalculateCullDistance(
			    this, GS_Rendering::HP_WIDGET_CULL_DISTANCE);
			HPTextWidgetComp->SetCullDistance(CullDistance);

			if (HPTextWidgetComp->GetOwner()->ActorHasTag("Monster"))
			{
				if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
				{
					HPTextWidgetComp->SetVisibility(PC->IsA<AGS_RTSController>());
				}
			}
		}
	}

	if (SelectionDecal && SelectionDecal->GetDecalMaterial())
	{
		DynamicDecalMaterial = UMaterialInstanceDynamic::Create(
		    SelectionDecal->GetDecalMaterial(), this);
		SelectionDecal->SetDecalMaterial(DynamicDecalMaterial);
	}

	DefaultCharacterSpeed = this->GetCharacterMovement()->MaxWalkSpeed;
	// CharacterSpeed = DefaultCharacterSpeed;

	if (HasAuthority())
	{
		SpawnAndAttachWeapons();
	}

	// === Significance Manager 등록 (클라이언트만) ===
	RegisterSignificanceManager();
}

void AGS_Character::RegisterSignificanceManager()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (USignificanceManager* SM = USignificanceManager::Get(GetWorld()))
		{
			// TWeakObjectPtr로 캡처하여 액터 파괴 후 람다 호출 시 안전성 확보
			TWeakObjectPtr<AGS_Character> WeakThis(this);

			SM->RegisterObject(
			    this, "Character",
			    [WeakThis](USignificanceManager::FManagedObjectInfo* ObjectInfo,
			               const FTransform& Viewpoint) -> float
			    {
				    if (AGS_Character* StrongThis = WeakThis.Get())
					    return StrongThis->CalculateSignificance(Viewpoint);
				    return 0.0f;
			    },
			    USignificanceManager::EPostSignificanceType::Sequential,
			    [WeakThis](USignificanceManager::FManagedObjectInfo* ObjectInfo,
			               float OldValue, float NewValue, bool bExternal)
			    {
				    if (AGS_Character* StrongThis = WeakThis.Get())
					    StrongThis->OnSignificanceChanged(NewValue);
			    });
		}
	}
}

void AGS_Character::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGS_Character::GetLifetimeReplicatedProps(
    TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGS_Character, WeaponSlots);
	DOREPLIFETIME(AGS_Character, CharacterSpeed);
	DOREPLIFETIME(AGS_Character, bIsDead);
	DOREPLIFETIME(AGS_Character, bIsInvincible);
	DOREPLIFETIME(AGS_Character, bLockRotationToController);
	DOREPLIFETIME(AGS_Character, WeaponHandlingState);
	DOREPLIFETIME(AGS_Character, RepImpactVFX);
}

void AGS_Character::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (StatComp)
	{
		StatComp->OnCurrentHPChanged.Clear();
	}

	// Stability: DefaultSubobject는 DestroyComponent 대신 비활성화만 수행
	// (DestroyComponent 호출 시 ensure(!IsDefaultSubobject()) 실패 위험)
	if (IsValid(HPTextWidgetComp))
	{
		HPTextWidgetComp->SetWidget(nullptr);
		HPTextWidgetComp->SetVisibility(false);
		HPTextWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HPTextWidgetComp->SetComponentTickEnabled(false);

		if (HPTextWidgetComp->GetBodySetup())
		{
			HPTextWidgetComp->DestroyPhysicsState();
		}
	}

	// Significance Manager 해제
	if (UWorld* World = GetWorld())
	{
		if (USignificanceManager* SM = USignificanceManager::Get(World))
		{
			SM->UnregisterObject(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AGS_Character::BeginDestroy()
{
	// 1. 먼저 Super::BeginDestroy() 호출 (중요!)
	Super::BeginDestroy();

	// 2. IsValid() 체크와 함께 안전하게 정리
	if (IsValid(HPTextWidgetComp) && !HPTextWidgetComp->IsBeingDestroyed())
	{
		HPTextWidgetComp->SetWidget(nullptr);
		HPTextWidgetComp->SetVisibility(false);
		HPTextWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// BodySetup 정리 (필요한 경우만)
		if (HPTextWidgetComp->GetBodySetup())
		{
			HPTextWidgetComp->DestroyPhysicsState();
		}
	}
}

float AGS_Character::TakeDamage(float DamageAmount,
                                FDamageEvent const& DamageEvent,
                                AController* EventInstigator,
                                AActor* DamageCauser)
{
	if (bIsInvincible)
	{
		return 0.0f;
	}
	// 이미 죽은 캐릭터는 추가 데미지를 받지 않음
	if (IsDead())
	{
		return 0.0f;
	}

	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent,
	                                       EventInstigator, DamageCauser);
	float CurrentHealth = StatComp->GetCurrentHealth();

	OnDamageStart();

	if (HasAuthority())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			Client_PlayTakeDamageShake(PC);
		}
	}

	if (CanHitReact)
	{
		EHitReactType HitReactType = EHitReactType::DamageOnly;
		FVector HitDirection = -GetActorForwardVector(); // 기본값

		// FGS_DamageEvent 타입인 경우 (커스텀 데미지 이벤트)
		if (DamageEvent.IsOfType(FGS_DamageEvent::ClassID))
		{
			const FGS_DamageEvent& MyDamageEvent =
			    static_cast<const FGS_DamageEvent&>(DamageEvent);
			HitReactType = MyDamageEvent.HitReactType;

			// FGS_DamageEvent도 PointDamage나 RadialDamage를 상속받았을 수 있으므로
			// 체크
			if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
			{
				const FPointDamageEvent* PointEvent =
				    static_cast<const FPointDamageEvent*>(&DamageEvent);
				HitDirection = -PointEvent->ShotDirection;
			}
			else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
			{
				const FRadialDamageEvent* RadialEvent =
				    static_cast<const FRadialDamageEvent*>(&DamageEvent);
				HitDirection =
				    (GetActorLocation() - RadialEvent->Origin).GetSafeNormal();
			}
		}
		// FGS_DamageEvent가 아닌 일반 UE 데미지 이벤트인 경우 (폴백)
		else
		{
			if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
			{
				const FPointDamageEvent* PointEvent =
				    static_cast<const FPointDamageEvent*>(&DamageEvent);
				HitDirection = -PointEvent->ShotDirection;
			}
			else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
			{
				const FRadialDamageEvent* RadialEvent =
				    static_cast<const FRadialDamageEvent*>(&DamageEvent);
				HitDirection =
				    (GetActorLocation() - RadialEvent->Origin).GetSafeNormal();
			}
		}

		if (UGS_HitReactComp* HitReactComponent =
		        GetComponentByClass<UGS_HitReactComp>())
		{
			HitReactComponent->PlayHitReact(HitReactType, HitDirection);
		}
	}

	float NewHealth = CurrentHealth - ActualDamage;
	StatComp->SetCurrentHealth(NewHealth, false);

	return ActualDamage;
}

void AGS_Character::OnDamageStart()
{
	//
}

void AGS_Character::DisableHitReact(float CooldownTime)
{
	SetCanHitReact(false);
	GetWorld()->GetTimerManager().SetTimer(
	    HitReactTimerHandle, [this]()
	    { CanHitReact = true; }, CooldownTime,
	    false);
}

void AGS_Character::DisableHitReact(bool bAllowHitReact)
{
	CanHitReact = bAllowHitReact;
}

void AGS_Character::SetupPlayerInputComponent(
    UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

bool AGS_Character::GetIsLockedRotationToController()
{
	return bLockRotationToController;
}

void AGS_Character::SetIsLockedRotationToController(
    bool InputIsRotationRoController)
{
	bLockRotationToController = InputIsRotationRoController;
}

void AGS_Character::OnDeath()
{
	bIsDead = true;

	OnDeathDelegate.Broadcast();

	// 서버/리슨 서버에서 로컬 Death 사운드 재생
	// 클라이언트는 OnRep_IsDead()에서 재생됨
	if (HasAuthority())
	{
		PlayDeathSoundLocal();
	}

	// 모든 디버프 제거 (VFX 포함)
	if (DebuffComp)
	{
		DebuffComp->ClearAllDebuffs();
	}

	DestroyAllWeapons();
	MulticastRPCCharacterDeath();
}

void AGS_Character::WatchOtherPlayer()
{
}

void AGS_Character::SetHPTextWidget(UGS_HPText* InHPTextWidget)
{
	UGS_HPText* HPTextWidget = Cast<UGS_HPText>(InHPTextWidget);
	if (IsValid(HPTextWidget))
	{
		HPTextWidget->InitializeHPTextWidget(GetStatComp());
		StatComp->OnCurrentHPChanged.AddUObject(HPTextWidget,
		                                        &UGS_HPText::OnCurrentHPChanged);
	}
}

void AGS_Character::SetHPBarWidget(UGS_HPWidget* InHPBarWidget)
{
	UGS_HPWidget* HPBarWidget = Cast<UGS_HPWidget>(InHPBarWidget);
	if (IsValid(HPBarWidget))
	{
		HPBarWidget->InitializeHPWidget(GetStatComp());
		StatComp->OnCurrentHPChanged.AddUObject(
		    HPBarWidget, &UGS_HPWidget::OnCurrentHPBarChanged);
	}
}

void AGS_Character::SetPlayerInfoWidget(
    UGS_PlayerInfoWidget* InPlayerInfoWidget)
{
	if (IsValid(InPlayerInfoWidget))
	{
		InPlayerInfoWidget->InitializePlayerInfoWidget(Cast<AGS_Player>(this));
		StatComp->OnCurrentHPChanged.AddUObject(
		    InPlayerInfoWidget, &UGS_PlayerInfoWidget::OnCurrentHPBarChanged);
	}
}

void AGS_Character::ServerRPCMeleeAttack_Implementation(
    AGS_Character* InDamagedCharacter)
{
	if (IsValid(InDamagedCharacter))
	{
		UGS_StatComp* DamagedCharacterStat = InDamagedCharacter->GetStatComp();
		if (IsValid(DamagedCharacterStat))
		{
			float Damage =
			    DamagedCharacterStat->CalculateDamage(this, InDamagedCharacter);
			FDamageEvent DamageEvent;
			InDamagedCharacter->TakeDamage(Damage, DamageEvent, GetController(),
			                               this);

			// 공격이 성공했을 때 공격자에게 카메라 쉐이크 적용
			if (APlayerController* AttackerPC =
			        Cast<APlayerController>(GetController()))
			{
				Client_PlayAttackSuccessShake(AttackerPC);
			}
		}
	}
}

void AGS_Character::Client_PlayTakeDamageShake_Implementation(
    APlayerController* TargetPC)
{
	if (TargetPC && TargetPC->IsLocalController() && TakeDamageShake.ShakeClass)
	{
		TargetPC->ClientStartCameraShake(TakeDamageShake.ShakeClass,
		                                 TakeDamageShake.Intensity);
	}
}

void AGS_Character::Client_PlayAttackSuccessShake_Implementation(
    APlayerController* TargetPC)
{
	if (TargetPC && TargetPC->IsLocalController() &&
	    AttackSuccessShake.ShakeClass)
	{
		TargetPC->ClientStartCameraShake(AttackSuccessShake.ShakeClass,
		                                 AttackSuccessShake.Intensity);
	}
}

void AGS_Character::Client_PlayAttackSuccessShakeWithInfo_Implementation(
    APlayerController* TargetPC, const FGS_CameraShakeInfo& CustomShakeInfo)
{
	if (TargetPC && TargetPC->IsLocalController() && CustomShakeInfo.ShakeClass)
	{
		TargetPC->ClientStartCameraShake(CustomShakeInfo.ShakeClass,
		                                 CustomShakeInfo.Intensity);
	}
}

FGenericTeamId AGS_Character::GetGenericTeamId() const
{
	return TeamId;
}

bool AGS_Character::IsEnemy(const AGS_Character* Other) const
{
	if (!Other)
	{
		return false;
	}

	const FGenericTeamId MyTeamId = GetGenericTeamId();
	const FGenericTeamId OtherTeamId = Other->GetGenericTeamId();

	// 몬스터(TeamId=2)는 시커(TeamId=1)만 공격
	if (MyTeamId == FGenericTeamId(2))
	{
		return OtherTeamId == FGenericTeamId(1);
	}

	// 다른 팀은 기존 로직 유지 (다른 TeamId = 적)
	return MyTeamId != OtherTeamId;
}

AGS_Weapon* AGS_Character::GetWeaponByIndex(int32 Index) const
{
	return WeaponSlots.IsValidIndex(Index) ? WeaponSlots[Index].WeaponInstance
	                                       : nullptr;
}

AGS_Weapon* AGS_Character::GetWeaponBySocketName(FName SocketName)
{
	for (FWeaponSlot WeaponSlot : WeaponSlots)
	{
		if (WeaponSlot.SocketName == SocketName)
		{
			return WeaponSlot.WeaponInstance;
		}
	}

	return nullptr;
}

void AGS_Character::SetCharacterSpeed(float InRatio)
{
	if (InRatio >= 0.4f &&
	    this->GetDebuffComp()->IsDebuffActive(EDebuffType::Slow))
	{
		// UE_LOG(LogTemp, Error, TEXT("Character Speed(제한됨) = %f"),
		// CharacterSpeed);
		return;
	}

	if (InRatio >= 0 && InRatio <= 1)
	{
		CharacterSpeed = DefaultCharacterSpeed * InRatio;
		GetCharacterMovement()->MaxWalkSpeed = CharacterSpeed;
		/*UE_LOG(LogTemp, Error, TEXT("Character Speed(변경됨) = %f"),
		CharacterSpeed); UE_LOG(LogTemp, Warning, TEXT("SpeedCheck: Slow=%s"),
			this->GetDebuffComp()->IsDebuffActive(EDebuffType::Slow) ?
		TEXT("True") : TEXT("False"));*/
	}
}

bool AGS_Character::IsDead() const
{
	return bIsDead;
}

void AGS_Character::Server_SetCharacterSpeed_Implementation(float InRatio)
{
	if (InRatio >= 0.8f &&
	    this->GetDebuffComp()->IsDebuffActive(EDebuffType::Slow))
	{
		// UE_LOG(LogTemp, Error, TEXT("Character Speed(제한됨) = %f"),
		// CharacterSpeed);
		return;
	}

	CharacterSpeed = DefaultCharacterSpeed * InRatio;
	/*UE_LOG(LogTemp, Error, TEXT("Character Speed(변경됨) = %f"),
	CharacterSpeed); UE_LOG(LogTemp, Warning, TEXT("SpeedCheck: Slow=%s"),
		this->GetDebuffComp()->IsDebuffActive(EDebuffType::Slow) ?
	TEXT("True") : TEXT("False"));*/

	if (HasAuthority())
	{
		OnRep_CharacterSpeed(); // 서버도 직접 반영
	}
}

void AGS_Character::MulticastRPCCharacterDeath_Implementation()
{
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));

	// 콜리전 비활성화하여 몬스터가 더 이상 죽은 캐릭터를 타겟으로 하지 않도록 함
	// GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AGS_Character::MulticastRPCPlaySkillMontage_Implementation(
    UAnimMontage* SkillMontage)
{
	PlayAnimMontage(SkillMontage);
}

void AGS_Character::MulicastRPCStopCurrentSkillMontage_Implementation(
    UAnimMontage* CurrentSkillMontage)
{
	StopAnimMontage(CurrentSkillMontage);
}

void AGS_Character::PlayImpactVFX(UNiagaraSystem* VFXAsset, FVector Scale)
{
	if (!HasAuthority())
		return;

	RepImpactVFX.VFXAsset = VFXAsset;
	RepImpactVFX.Scale = Scale;
	RepImpactVFX.Counter++;

	// 서버가 리슨 서버이거나 스탠드얼론이면 즉시 실행
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnRep_ImpactVFX();
	}
}

void AGS_Character::OnRep_ImpactVFX()
{
	// Soft Reference 로드 (Get()은 로컬 캐시 확인용)
	UNiagaraSystem* LoadedVFX = RepImpactVFX.VFXAsset.Get();

	if (!LoadedVFX && !RepImpactVFX.VFXAsset.IsNull())
	{
		// 이 시점에서 동기 로드를 수행하거나 (안전장치),
		// 시스템적으로 미리 로드되어 있을 것으로 기대.
		LoadedVFX = RepImpactVFX.VFXAsset.LoadSynchronous();
	}

	if (LoadedVFX)
	{
		UNiagaraComponent* SpawnedVFX =
		    UNiagaraFunctionLibrary::SpawnSystemAttached(
		        LoadedVFX, GetRootComponent(), NAME_None,
		        FVector::ZeroVector, FRotator::ZeroRotator,
		        EAttachLocation::SnapToTarget,
		        true, // bAutoDestroy
		        true, // bAutoActivate (위치 수정)
		        ENCPoolMethod::AutoRelease, // Pooling 활성화 (위치 수정)
		        true // bPreCullCheck
		    );

		if (SpawnedVFX)
		{
			SpawnedVFX->SetWorldScale3D(RepImpactVFX.Scale);
		}
	}
}

void AGS_Character::SpawnAndAttachWeapons()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FWeaponSlot& Slot : WeaponSlots)
	{
		if (!Slot.WeaponClass)
		{
			continue;
		}

		FActorSpawnParameters Params;
		Params.Owner = this;
		Slot.WeaponInstance =
		    World->SpawnActor<AGS_Weapon>(Slot.WeaponClass, Params);
		if (!Slot.WeaponInstance)
		{
			continue;
		}

		Slot.WeaponInstance->AttachToComponent(
		    GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale,
		    Slot.SocketName);
	}
}

void AGS_Character::DestroyAllWeapons()
{
	if (!HasAuthority())
	{
		return;
	}

	for (FWeaponSlot& Slot : WeaponSlots)
	{
		if (!Slot.WeaponInstance)
		{
			continue;
		}

		Slot.WeaponInstance->Destroy();
	}
}

void AGS_Character::OnRep_CharacterSpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = CharacterSpeed;
}

void AGS_Character::PlayDeathSoundLocal()
{
	if (BaseAudioComponent)
	{
		BaseAudioComponent->PlayDeathSoundLocal();
	}
}

void AGS_Character::OnRep_IsDead()
{
	// 클라이언트에서 Death 사운드 재생
	if (bIsDead)
	{
		PlayDeathSoundLocal();
	}
}

void AGS_Character::Server_SetCanHitReact_Implementation(bool bCanReact)
{
	CanHitReact = bCanReact;
}

void AGS_Character::SetCanHitReact(bool bCanReact)
{
	CanHitReact = bCanReact;
}

void AGS_Character::SetInvincible(bool bEnable)
{
	bIsInvincible = bEnable;
}

void AGS_Character::NotifyActorBeginCursorOver()
{
	Super::NotifyActorBeginCursorOver();

	SetHovered(true);
}

void AGS_Character::NotifyActorEndCursorOver()
{
	Super::NotifyActorEndCursorOver();

	SetHovered(false);
}

void AGS_Character::SetHovered(bool bHovered)
{
	if (bIsHovered != bHovered)
	{
		bIsHovered = bHovered;

		if (bIsHovered)
		{
			OnHoverBegin();
		}
		else
		{
			OnHoverEnd();
		}

		UpdateDecal();
	}
}

void AGS_Character::UpdateDecal()
{
	if (!SelectionDecal || !ShowDecal())
	{
		SelectionDecal->SetVisibility(false);
		return;
	}

	if (bIsHovered)
	{
		ShowDecalWithColor(GetCurrentDecalColor());
	}
	else
	{
		SelectionDecal->SetVisibility(false);
	}
}

void AGS_Character::ShowDecalWithColor(const FLinearColor& Color)
{
	SelectionDecal->SetVisibility(true);
	if (DynamicDecalMaterial)
	{
		DynamicDecalMaterial->SetVectorParameterValue(TEXT("DecalColor"), Color);
	}
}

FLinearColor AGS_Character::GetCurrentDecalColor()
{
	return FLinearColor::White;
}

bool AGS_Character::ShowDecal()
{
	return false;
}

void AGS_Character::OnHoverBegin()
{
}

void AGS_Character::OnHoverEnd()
{
}

EWeaponHandlingState AGS_Character::GetWeaponHandlingState()
{
	return WeaponHandlingState;
}

void AGS_Character::SetWeaponHandlingState(
    EWeaponHandlingState InputWeaponHandlingState)
{
	WeaponHandlingState = InputWeaponHandlingState;
}

bool AGS_Character::ShouldPlayVFXAtLocation(const FVector& Location,
                                            float MaxDistance) const
{
	return UGS_VFX_FunctionLibrary::ShouldPlayVFXAtLocation(this, Location,
	                                                        MaxDistance, true);
}

UTexture2D* AGS_Character::GetPortrait() const
{
	if (!CharacterData)
	{
		return nullptr;
	}

	// Soft Reference를 동기 로드 (UI는 즉시 표시되어야 하므로)
	return UGS_AssetLoader::SyncLoadAsset(CharacterData->Portrait);
}

float AGS_Character::CalculateSignificance(const FTransform& Viewpoint)
{
	if (IsDead())
		return 0.0f;

	// 로컬 플레이어는 무조건 최상위 중요도 (1.0)
	if (IsLocallyControlled())
		return 1.0f;

	float Score = 0.1f;
	FVector ActorLoc = GetActorLocation();
	FVector ViewLoc = Viewpoint.GetLocation();
	float DistSq = FVector::DistSquared(ActorLoc, ViewLoc);

	// 기본 거리 기반 점수 (플레이어는 50m 기준)
	float MaxRangeSq = FMath::Square(5000.0f);
	Score = FMath::Clamp(1.2f - (DistSq / MaxRangeSq), 0.1f, 1.0f);

	return Score;
}

void AGS_Character::OnSignificanceChanged(float NewSignificance)
{
	CurrentSignificance = NewSignificance;

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
		return;

	// 1. 애니메이션 URO (Update Rate Optimization) 제어
	// 중요도가 0.7 미만이면 프레임 스킵 허용
	MeshComp->bEnableUpdateRateOptimizations = (NewSignificance < 0.7f);

	// 2. 중요도에 따른 애니메이션 틱 옵션 조정
	if (NewSignificance > 0.5f)
	{
		// 중요할 때: 시각적 품질 유지
		MeshComp->VisibilityBasedAnimTickOption =
		    EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	else
	{
		// 보통이거나 낮을 때: 화면에 보일 때만 갱신
		MeshComp->VisibilityBasedAnimTickOption =
		    EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	}

	// 3. 중요도가 매우 낮으면 Tick 비활성화
	if (PrimaryActorTick.bCanEverTick)
	{
		SetActorTickEnabled(NewSignificance > 0.1f);
	}

	// 4. 이동 및 물리 연산 최적화
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (NewSignificance > 0.8f)
		{
			// 매우 중요한 경우: 최고 부드러움 유지
			MoveComp->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
			MoveComp->bComponentShouldUpdatePhysicsVolume = true;
		}
		else if (NewSignificance > 0.3f)
		{
			// 중간 중요도: 선형 보간으로 전환
			MoveComp->NetworkSmoothingMode = ENetworkSmoothingMode::Linear;
		}
		else
		{
			// 낮거나 거의 안 보일 때: 보간 비활성화 및 물리 체크 최소화
			MoveComp->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
			MoveComp->bComponentShouldUpdatePhysicsVolume = false;
		}
	}

	// 5. 네트워크 업데이트 빈도 최적화 (서버 전용)
	if (HasAuthority())
	{
		if (NewSignificance > 0.8f)
			NetUpdateFrequency = 60.0f;
		else if (NewSignificance > 0.4f)
			NetUpdateFrequency = 30.0f;
		else
			NetUpdateFrequency = 5.0f;
	}
}

void AGS_Character::UpdateShadowCulling()
{
	// 클라이언트에서만 실행
	if (IsRunningDedicatedServer())
		return;

	// 로컬 플레이어는 최적화 제외 (Player 타입만 해당)
	if (AGS_Player* Player = Cast<AGS_Player>(this))
	{
		if (Player->IsLocalPlayer())
			return;
	}

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
					// 80m 이상: 동적/정적 그림자 모두 끄되, 캡슐 그림자는 유지 (Monster)
					MeshComp->SetCastShadow(false);
					MeshComp->bCastDynamicShadow = false;
				}
				else if (Distance > GS_Rendering::DYNAMIC_SHADOW_DISABLE_DISTANCE)
				{
					// 40-80m: 정적 그림자 유지, 동적 그림자 비활성화
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
