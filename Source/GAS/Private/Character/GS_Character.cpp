#include "Character/GS_Character.h"
#include "Net/UnrealNetwork.h"
#include "Character/Player/GS_Player.h"
#include "SignificanceManager.h"
#include "Engine/World.h"
#include "AI/RTS/GS_RTSController.h"
#include "AkGameplayStatics.h"
#include "Character/Component/GS_CameraShakeComponent.h"
#include "Character/Component/GS_DebuffComp.h"
#include "Character/Component/GS_DrakharAudioComponent.h"
#include "Character/Component/GS_HitReactComp.h"
#include "Character/Component/GS_HitIndicatorComponent.h"
#include "Character/Component/GS_DamageNumberComponent.h"
#include "Character/Component/GS_StatComp.h"
#include "UI/Damage/EDamageNumberType.h"
#include "Character/F_GS_DamageEvent.h"
#include "Character/Component/GS_KillFeedbackComponent.h"
#include "Character/Component/GS_PositiveEffectComponent.h"
#include "Containers/Set.h"
#include "GameFramework/PlayerState.h"
#include "Character/CharacterDataAsset.h"
#include "Character/Player/Guardian/GS_Drakhar.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Components/DecalComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Sound/GS_MonsterAudioComponent.h"
#include "Sound/GS_SeekerAudioComponent.h"
#include "System/GS_PlayerState.h"
#include "System/Utility/GS_AssetLoader.h"
#include "UI/Character/GS_HPText.h"
#include "UI/Character/GS_HPTextWidgetComp.h"
#include "Weapon/Projectile/GS_WeaponProjectile.h"
#include "UI/Character/GS_HPWidget.h"
#include "UI/Character/GS_PlayerInfoWidget.h"
#include "VFX/GS_VFX_FunctionLibrary.h"
#include "Weapon/GS_Weapon.h"
#include "Sound/GS_AudioMixingComponent.h"
#include "Props/Trap/GS_TrapBase.h"

AGS_Character::AGS_Character(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	StatComp = ObjectInitializer.CreateDefaultSubobject<UGS_StatComp>(this, TEXT("StatComp"));
	DebuffComp = ObjectInitializer.CreateDefaultSubobject<UGS_DebuffComp>(this, TEXT("DebuffComp"));
	HitReactComp = ObjectInitializer.CreateDefaultSubobject<UGS_HitReactComp>(this, TEXT("HitReactComp"));
	CameraShakeComp = ObjectInitializer.CreateDefaultSubobject<UGS_CameraShakeComponent>(this, TEXT("CameraShakeComp"));

	HPTextWidgetComp = ObjectInitializer.CreateDefaultSubobject<UGS_HPTextWidgetComp>(this, TEXT("TextWidgetComp"));
	HPTextWidgetComp->SetupAttachment(RootComponent);
	HPTextWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	HPTextWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HPTextWidgetComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	HPTextWidgetComp->SetVisibility(false);
	HPTextWidgetComp->SetCullDistance(2000.0f);

	SelectionDecal = ObjectInitializer.CreateDefaultSubobject<UDecalComponent>(this, TEXT("SelectionDecal"));
	SelectionDecal->SetupAttachment(RootComponent);
	SelectionDecal->SetVisibility(false);

	bIsDead = false;
	bIsHovered = false;
	bIsInvincible = false;

	AudioMixingComponent =
		ObjectInitializer.CreateDefaultSubobject<UGS_AudioMixingComponent>(this, TEXT("AudioMixingComponent"));
	DamageNumberComp =
		ObjectInitializer.CreateDefaultSubobject<UGS_DamageNumberComponent>(this, TEXT("DamageNumberComp"));
}

void AGS_Character::BeginPlay()
{
	Super::BeginPlay();

	bIsInvincible = false;

	const UEnum* CharacterEnum = StaticEnum<ECharacterType>();
	bool bStatInitialized = false;

	if (CharacterEnum)
	{
		FString EnumToName = CharacterEnum->GetNameStringByValue((int64)CharacterType);
		StatComp->InitStat(FName(EnumToName));
		bStatInitialized = true;
	}

	if (bStatInitialized)
	{
		AGS_PlayerState* PS = GetPlayerState<AGS_PlayerState>();
		if (PS && PS->CurrentPlayerRole == EPlayerRole::PR_Seeker)
		{
			PS->OnPawnStatInitialized();
		}
	}

	if (GetNetMode() != NM_DedicatedServer && IsValid(HPTextWidgetComp))
	{
		float CullDistance = GS_Rendering::CalculateCullDistance(this, GS_Rendering::HP_WIDGET_CULL_DISTANCE);
		HPTextWidgetComp->SetCullDistance(CullDistance);

		if (HPTextWidgetComp->GetOwner()->ActorHasTag("Monster"))
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				HPTextWidgetComp->SetVisibility(PC->IsA<AGS_RTSController>());
			}
		}
		else if (IsA<AGS_Seeker>())
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				if (PC->IsA<AGS_RTSController>())
				{
					HPTextWidgetComp->SetVisibility(true);
				}
			}
		}
	}

	if (SelectionDecal && SelectionDecal->GetDecalMaterial())
	{
		DynamicDecalMaterial = UMaterialInstanceDynamic::Create(SelectionDecal->GetDecalMaterial(), this);
		SelectionDecal->SetDecalMaterial(DynamicDecalMaterial);
	}

	DefaultCharacterSpeed = this->GetCharacterMovement()->MaxWalkSpeed;

	if (HasAuthority())
	{
		SpawnAndAttachWeapons();
	}

	RegisterSignificanceManager();
}

void AGS_Character::RegisterSignificanceManager()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (USignificanceManager* SM = USignificanceManager::Get(GetWorld()))
		{
			TWeakObjectPtr<AGS_Character> WeakThis(this);
			SM->RegisterObject(
				this,
				"Character",
				[WeakThis](USignificanceManager::FManagedObjectInfo* ObjectInfo, const FTransform& Viewpoint) -> float
				{
					if (AGS_Character* StrongThis = WeakThis.Get())
						return StrongThis->CalculateSignificance(Viewpoint);
					return 0.0f;
				},
				USignificanceManager::EPostSignificanceType::Sequential,
				[WeakThis](USignificanceManager::FManagedObjectInfo* ObjectInfo,
						   float OldValue,
						   float NewValue,
						   bool bExternal)
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

	if (CurrentCameraKnockback > 0.0f)
	{
		float PreviousKnockback = CurrentCameraKnockback;
		CurrentCameraKnockback =
			FMath::FInterpTo(CurrentCameraKnockback, 0.0f, DeltaTime, CameraKnockbackRecoverySpeed);

		AGS_Player* Player = IsA<AGS_Player>() ? static_cast<AGS_Player*>(this) : nullptr;
		if (IsValid(Player) && IsValid(Player->SpringArmComp))
		{
			float Delta = PreviousKnockback - CurrentCameraKnockback;
			Player->SpringArmComp->TargetArmLength -= Delta;
		}

		if (CurrentCameraKnockback <= KINDA_SMALL_NUMBER)
		{
			CurrentCameraKnockback = 0.0f;
		}
	}
}

void AGS_Character::ApplyCameraKnockback(float IntensityMultiplier)
{
	if (!IsLocallyControlled())
		return;

	AGS_Player* Player = IsA<AGS_Player>() ? static_cast<AGS_Player*>(this) : nullptr;
	if (IsValid(Player) && IsValid(Player->SpringArmComp))
	{
		float DynamicKnockbackDistance = CameraKnockbackDistance * IntensityMultiplier;
		float NewKnockback = FMath::Min(CurrentCameraKnockback + DynamicKnockbackDistance, 20.0f);
		float Delta = NewKnockback - CurrentCameraKnockback;

		CurrentCameraKnockback = NewKnockback;
		Player->SpringArmComp->TargetArmLength += Delta;

		SetActorTickEnabled(true);
	}
}

void AGS_Character::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGS_Character, WeaponSlots);
	DOREPLIFETIME(AGS_Character, CharacterSpeed);
	DOREPLIFETIME(AGS_Character, bIsDead);
	DOREPLIFETIME(AGS_Character, bIsInvincible);
	DOREPLIFETIME(AGS_Character, bLockRotationToController);
	DOREPLIFETIME(AGS_Character, WeaponHandlingState);
	DOREPLIFETIME(AGS_Character, RepImpactVFX);
	DOREPLIFETIME(AGS_Character, LastKillerInfo);
}

void AGS_Character::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (StatComp)
	{
		StatComp->OnCurrentHPChanged.Clear();
	}

	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(HitReactTimerHandle);
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

	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (!StatComp)
	{
		return 0.0f;
	}

	float CurrentHealth = StatComp->GetCurrentHealth();

	OnDamageStart();

	if (HasAuthority())
	{
		bool bSuppressCameraEffects = false;
		if (DamageEvent.IsOfType(FGS_DamageEvent::ClassID))
		{
			const FGS_DamageEvent& MyDamageEvent = static_cast<const FGS_DamageEvent&>(DamageEvent);
			bSuppressCameraEffects = MyDamageEvent.bSuppressCameraEffects;
		}

		if (!bSuppressCameraEffects)
		{
			if (AController* C = GetController())
			{
				if (APlayerController* PC = Cast<APlayerController>(C))
				{
					// 데미지 강도에 따른 카메라 쉐이크 선택
					FGS_CameraShakeInfo SelectedShake;
					float IntensityMultiplier = 1.0f;

					if (ActualDamage < LightDamageThreshold)
					{
						SelectedShake = LightDamageShake;
						IntensityMultiplier = 0.5f;
					}
					else if (ActualDamage < NormalDamageThreshold)
					{
						SelectedShake = NormalDamageShake;
						IntensityMultiplier = 1.0f;
					}
					else if (ActualDamage < HeavyDamageThreshold)
					{
						SelectedShake = HeavyDamageShake;
						IntensityMultiplier = 1.5f;
					}
					else
					{
						SelectedShake = HeavyDamageShake;
						IntensityMultiplier = 2.0f;
					}

					SelectedShake.Intensity *= IntensityMultiplier;
					Client_PlayTakeDamageShake(PC, SelectedShake, IntensityMultiplier);
				}
			}
		}
	}

	if (CanHitReact)
	{
		EHitReactType HitReactType = EHitReactType::DamageOnly;
		FVector HitDirection = -GetActorForwardVector(); // 기본값

		// FGS_DamageEvent 타입인 경우 (커스텀 데미지 이벤트)
		if (DamageEvent.IsOfType(FGS_DamageEvent::ClassID))
		{
			const FGS_DamageEvent& MyDamageEvent = static_cast<const FGS_DamageEvent&>(DamageEvent);
			HitReactType = MyDamageEvent.HitReactType;

			// FGS_DamageEvent도 PointDamage나 RadialDamage를 상속받았을 수 있으므로
			// 체크
			if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
			{
				const FPointDamageEvent* PointEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
				HitDirection = -PointEvent->ShotDirection;
			}
			else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
			{
				const FRadialDamageEvent* RadialEvent = static_cast<const FRadialDamageEvent*>(&DamageEvent);
				HitDirection = (GetActorLocation() - RadialEvent->Origin).GetSafeNormal();
			}
		}
		// FGS_DamageEvent가 아닌 일반 UE 데미지 이벤트인 경우 (폴백)
		else
		{
			if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
			{
				const FPointDamageEvent* PointEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
				HitDirection = -PointEvent->ShotDirection;
			}
			else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
			{
				const FRadialDamageEvent* RadialEvent = static_cast<const FRadialDamageEvent*>(&DamageEvent);
				HitDirection = (GetActorLocation() - RadialEvent->Origin).GetSafeNormal();
			}
		}

		if (UGS_HitReactComp* HitReactComponent = GetComponentByClass<UGS_HitReactComp>())
		{
			HitReactComponent->PlayHitReact(HitReactType, HitDirection);
		}
	}

	// 피격 방향 HUD 표시 (CanHitReact와 관계없이 항상 호출)
	if (UGS_HitIndicatorComponent* HitIndicator = GetComponentByClass<UGS_HitIndicatorComponent>())
	{
		// 기본값은 Omni(전 방향)로 설정 - ZeroVector가 전달되면 Omni 인디케이터 활성화
		FVector DirToAttacker = FVector::ZeroVector;

		// 1. Point Damage: 탄환, 근접공격 등 정확한 타격 방향이 있음
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			const FPointDamageEvent* PointEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
			DirToAttacker = -PointEvent->ShotDirection;
		}
		// 2. Radial Damage: 폭발 등 중심점이 있음
		else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
		{
			const FRadialDamageEvent* RadialEvent = static_cast<const FRadialDamageEvent*>(&DamageEvent);
			DirToAttacker = (RadialEvent->Origin - GetActorLocation()).GetSafeNormal();
		}
		// 3. 캐릭터(몬스터, 가디언 등)가 공격한 경우 - 공격자 위치 방향 표시
		else if (AGS_Character* AttackerCharacter = Cast<AGS_Character>(DamageCauser))
		{
			DirToAttacker = (AttackerCharacter->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		}
		// 4. 프로젝타일(가디언 원거리, 몬스터 투사체 등)이 공격한 경우 - 프로젝타일 위치 방향 표시
		else if (AGS_WeaponProjectile* Projectile = Cast<AGS_WeaponProjectile>(DamageCauser))
		{
			DirToAttacker = (Projectile->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		}
		// 5. 그 외 (도트 데미지, 환경 데미지, 함정 등): Omni (ZeroVector 유지)

		HitIndicator->NotifyDamageDirection(DirToAttacker, ActualDamage);
	}

	float NewHealth = CurrentHealth - ActualDamage;
	StatComp->SetCurrentHealth(NewHealth, false);

	// 데미지 숫자 팝업 표시 (공격자 화면에 표시)
	const FGS_DamageEvent* GSDamageEvent =
		DamageEvent.IsOfType(FGS_DamageEvent::ClassID) ? static_cast<const FGS_DamageEvent*>(&DamageEvent) : nullptr;

	// DoT 데미지: HitReactType이 DamageOnly이고 Point/Radial 이벤트가 아닌 경우
	bool bIsDotDamage = false;
	if (GSDamageEvent)
	{
		bIsDotDamage = (GSDamageEvent->HitReactType == EHitReactType::DamageOnly) &&
					   !DamageEvent.IsOfType(FPointDamageEvent::ClassID) &&
					   !DamageEvent.IsOfType(FRadialDamageEvent::ClassID);
	}

	// 공격자 캐릭터에게 데미지 숫자 표시 요청
	// 직접 캐릭터가 공격했거나, 프로젝타일을 통해 공격한 경우 모두 처리
	AGS_Character* AttackerCharacter = Cast<AGS_Character>(DamageCauser);

	// DamageCauser가 캐릭터가 아닌 경우 (프로젝타일 등)
	if (!AttackerCharacter)
	{
		// 프로젝타일인 경우 소유자(발사한 캐릭터) 찾기
		if (AGS_WeaponProjectile* Projectile = Cast<AGS_WeaponProjectile>(DamageCauser))
		{
			AttackerCharacter = Cast<AGS_Character>(Projectile->GetOwner());
		}
		// 일반 무기인 경우
		else if (AGS_Weapon* Weapon = Cast<AGS_Weapon>(DamageCauser))
		{
			AttackerCharacter = Cast<AGS_Character>(Weapon->GetOwner());
		}
	}

	// 마지막 공격자 정보 저장 (사망 화면 표시용)
	if (ActualDamage > 0.0f)
	{
		if (AttackerCharacter)
		{
			LastKillerInfo.KillerName = AttackerCharacter->GetCharacterName();
			LastKillerInfo.KillerType = AttackerCharacter->GetCharacterType();
			LastKillerInfo.bIsPlayerControlled = AttackerCharacter->IsPlayerControlled();
		}
		// 캐릭터가 아닌 경우 (함정 등)
		else if (AGS_TrapBase* Trap = Cast<AGS_TrapBase>(DamageCauser))
		{
			// 함정 ID가 있으면 해당 ID를 이름으로 사용하거나, 기본 '함정' 텍스트 사용
			FString TrapName = Trap->TrapID.IsNone() ? TEXT("함정") : Trap->TrapID.ToString();
			LastKillerInfo.KillerName = TrapName;

			// 함정은 특정 캐릭터 타입이 없으므로 Ares(기본값)로 두되,
			// 위젯에서 이름이 '함정'이므로 인지 가능
			// 필요하다면 ECharacterType에 Trap을 추가할 수 있음
			LastKillerInfo.bIsPlayerControlled = false;
		}
	}


	// 데미지 기록 업데이트 (서버 전용 어시스트 추적)
	if (HasAuthority() && AttackerCharacter && AttackerCharacter != this && ActualDamage > 0.1f)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		bool bFound = false;

		// 기존 기록 확인
		for (FDamageRecord& Record : DamageHistory)
		{
			if (Record.Damager.Get() == AttackerCharacter)
			{
				Record.DamageAmount += ActualDamage;
				Record.LastDamageTime = CurrentTime;
				bFound = true;
				break;
			}
		}

		// 새로운 기록 추가
		if (!bFound)
		{
			FDamageRecord NewRecord;
			NewRecord.Damager = AttackerCharacter;
			NewRecord.DamageAmount = ActualDamage;
			NewRecord.LastDamageTime = CurrentTime;
			DamageHistory.Add(NewRecord);
		}

		// 오래된 기록 정리 (최적화)
		DamageHistory.RemoveAll(
			[CurrentTime, this](const FDamageRecord& Record)
			{ return !Record.Damager.IsValid() || (CurrentTime - Record.LastDamageTime) > AssistWindowSeconds; });
	}

	if (AttackerCharacter && ActualDamage > 0.0f)
	{
		if (UGS_DamageNumberComponent* DmgNumComp = AttackerCharacter->GetDamageNumberComponent())
		{
			// 데미지 타입 결정: Critical > DoT > Normal
			EDamageNumberType NumType = EDamageNumberType::Normal;
			if (GSDamageEvent && GSDamageEvent->bIsCritical)
			{
				NumType = EDamageNumberType::Critical;
			}
			else if (bIsDotDamage)
			{
				NumType = EDamageNumberType::DoT;
			}

			// 피해자의 머리 위치에 표시 (캡슐 높이 상단 - 너무 높지 않게)
			FVector DisplayLocation = GetActorLocation();
			DisplayLocation.Z += GetDefaultHalfHeight(); // 1.0x로 낮춤

			DmgNumComp->ShowDamageNumber(ActualDamage, NumType, DisplayLocation);
		}
	}

	// 킬 피드백 알림 (서버에서만 처리)
	if (HasAuthority() && NewHealth <= 0.0f && CurrentHealth > 0.0f)
	{
		// 킬 피드백을 브로드캐스트할 수 있는 컴포넌트 찾기 (주로 공격자의 컨트롤러나 아무 PC)
		UGS_KillFeedbackComponent* FeedbackComp = nullptr;

		// 1. 공격자의 컨트롤러에서 찾기
		if (AttackerCharacter && AttackerCharacter->GetController())
		{
			FeedbackComp = AttackerCharacter->GetController()->FindComponentByClass<UGS_KillFeedbackComponent>();
		}

		// 2. 공격자에게 없으면 아무 플레이어 컨트롤러에서나 찾기
		if (!FeedbackComp)
		{
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* PC = It->Get())
				{
					FeedbackComp = PC->FindComponentByClass<UGS_KillFeedbackComponent>();
					if (FeedbackComp)
						break;
				}
			}
		}

		if (FeedbackComp)
		{
			EKillFeedbackType Type = GetKillFeedbackType();
			FString KillerName = AttackerCharacter ? AttackerCharacter->GetCharacterName() : TEXT("");
			FString VictimName = GetCharacterName();
			uint8 KillerTeam = AttackerCharacter ? AttackerCharacter->GetGenericTeamId().GetId() : 255;
			bool bIsKillerPlayer = AttackerCharacter ? AttackerCharacter->IsPlayerControlled() : false;
			bool bWasCritical = GSDamageEvent ? GSDamageEvent->bIsCritical : false;

			FeedbackComp->NotifyKill(Type, VictimName, KillerName, KillerTeam, bIsKillerPlayer, bWasCritical);

			// 어시스트 알림 처리
			float CurrentTime = GetWorld()->GetTimeSeconds();
			float MaxHP = StatComp ? StatComp->GetMaxHealth() : 1000.f;
			float MinAssistDamage = MaxHP * AssistThresholdRatio;

			TMap<FString, bool> UniqueAssisters;

			// 1. 직접 데미지 기여자
			for (const FDamageRecord& Record : DamageHistory)
			{
				AGS_Character* DamagerChar = Record.Damager.Get();
				if (!DamagerChar || DamagerChar == AttackerCharacter)
				{
					continue;
				}

				if ((CurrentTime - Record.LastDamageTime) <= AssistWindowSeconds &&
					Record.DamageAmount >= MinAssistDamage)
				{
					UniqueAssisters.Add(DamagerChar->GetCharacterName(), DamagerChar->IsPlayerControlled());

					// 데미지 기여자를 도운 서포터들도 어시스트 (1단계만)
					for (const FSupportRecord& SRecord : DamagerChar->GetSupportHistory())
					{
						AGS_Character* SupporterChar = SRecord.Supporter.Get();
						// 피해자 본인 및 킬러 제외
						if (SupporterChar && SupporterChar != this && SupporterChar != AttackerCharacter &&
							(CurrentTime - SRecord.LastSupportTime) <= AssistWindowSeconds)
						{
							UniqueAssisters.Add(SupporterChar->GetCharacterName(), SupporterChar->IsPlayerControlled());
						}
					}
				}
			}

			// 2. 킬러를 도운 서포터들
			if (AttackerCharacter)
			{
				for (const FSupportRecord& SRecord : AttackerCharacter->GetSupportHistory())
				{
					AGS_Character* SupporterChar = SRecord.Supporter.Get();
					// 자기 자신 및 피해자 제외
					if (!SupporterChar || SupporterChar == AttackerCharacter || SupporterChar == this)
					{
						continue;
					}

					if ((CurrentTime - SRecord.LastSupportTime) <= AssistWindowSeconds)
					{
						UniqueAssisters.Add(SupporterChar->GetCharacterName(), SupporterChar->IsPlayerControlled());
					}
				}
			}

			// 브로드캐스트
			for (const auto& Pair : UniqueAssisters)
			{
				FeedbackComp->NotifyAssist(Pair.Key, Pair.Value);
			}
		}

		// 사망 시 데미지 기록 초기화 (SupportHistory는 유지 - 킬러/어시스터에게 여전히 유효)
		DamageHistory.Empty();
	}

	return ActualDamage;
}

void AGS_Character::OnDamageStart()
{
	//
}

void AGS_Character::DisableHitReact(float CooldownTime)
{
	SetCanHitReact(false);
	GetWorld()->GetTimerManager().SetTimer(HitReactTimerHandle, [this]() { CanHitReact = true; }, CooldownTime, false);
}

void AGS_Character::DisableHitReact(bool bAllowHitReact)
{
	CanHitReact = bAllowHitReact;
}

void AGS_Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

bool AGS_Character::GetIsLockedRotationToController()
{
	return bLockRotationToController;
}

void AGS_Character::SetIsLockedRotationToController(bool InputIsRotationRoController)
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
		StatComp->OnCurrentHPChanged.AddUObject(HPTextWidget, &UGS_HPText::OnCurrentHPChanged);
	}
}

EKillFeedbackType AGS_Character::GetKillFeedbackType() const
{
	// Default behavior based on CharacterType
	switch (CharacterType)
	{
		case ECharacterType::SmallClaw:
		case ECharacterType::NeedleFang:
		case ECharacterType::IronFang:
		case ECharacterType::StoneClaw:
			return EKillFeedbackType::MonsterKill;
		case ECharacterType::ShadowFang:
			return EKillFeedbackType::EliteKill;
		case ECharacterType::Ares:
		case ECharacterType::Chan:
		case ECharacterType::Merci:
		case ECharacterType::Reina:
			return EKillFeedbackType::SeekerKill;
		case ECharacterType::Drakhar:
			return EKillFeedbackType::GuardianRepelled;
		default:
			return EKillFeedbackType::None;
	}
}

FString AGS_Character::GetCharacterName() const
{
	if (APlayerState* PS = GetPlayerState())
	{
		return PS->GetPlayerName();
	}

	// Falls back to data asset name if available
	if (CharacterData)
	{
		return CharacterData->CharacterName.ToString();
	}

	return GetName();
}


void AGS_Character::SetHPBarWidget(UGS_HPWidget* InHPBarWidget)
{
	UGS_HPWidget* HPBarWidget = Cast<UGS_HPWidget>(InHPBarWidget);
	if (IsValid(HPBarWidget))
	{
		HPBarWidget->InitializeHPWidget(GetStatComp());
		StatComp->OnCurrentHPChanged.AddUObject(HPBarWidget, &UGS_HPWidget::OnCurrentHPBarChanged);
	}
}

void AGS_Character::SetPlayerInfoWidget(UGS_PlayerInfoWidget* InPlayerInfoWidget)
{
	if (IsValid(InPlayerInfoWidget))
	{
		AGS_Player* Player = IsA<AGS_Player>() ? static_cast<AGS_Player*>(this) : nullptr;
		InPlayerInfoWidget->InitializePlayerInfoWidget(Player);
		StatComp->OnCurrentHPChanged.AddUObject(InPlayerInfoWidget, &UGS_PlayerInfoWidget::OnCurrentHPBarChanged);
	}
}

void AGS_Character::ServerRPCMeleeAttack_Implementation(AGS_Character* InDamagedCharacter)
{
	if (IsValid(InDamagedCharacter))
	{
		UGS_StatComp* DamagedCharacterStat = InDamagedCharacter->GetStatComp();
		if (IsValid(DamagedCharacterStat))
		{
			bool bIsCritical = false;
			float Damage = DamagedCharacterStat->CalculateDamage(this, InDamagedCharacter, bIsCritical);
			FGS_DamageEvent DamageEvent;
			DamageEvent.bIsCritical = bIsCritical;
			InDamagedCharacter->TakeDamage(Damage, DamageEvent, GetController(), this);

			// 공격이 성공했을 때 공격자에게 카메라 쉐이크 적용
			if (APlayerController* AttackerPC = Cast<APlayerController>(GetController()))
			{
				Client_PlayAttackSuccessShake(AttackerPC);
			}
		}
	}
}

void AGS_Character::Client_PlayTakeDamageShake_Implementation(APlayerController* TargetPC,
															  const FGS_CameraShakeInfo& ShakeInfo,
															  float KnockbackMultiplier)
{
	if (TargetPC && TargetPC->IsLocalController() && ShakeInfo.ShakeClass)
	{
		TargetPC->ClientStartCameraShake(ShakeInfo.ShakeClass, ShakeInfo.Intensity);
		ApplyCameraKnockback(KnockbackMultiplier);
	}
}

void AGS_Character::Client_PlayAttackSuccessShake_Implementation(APlayerController* TargetPC)
{
	if (TargetPC && TargetPC->IsLocalController() && AttackSuccessShake.ShakeClass)
	{
		TargetPC->ClientStartCameraShake(AttackSuccessShake.ShakeClass, AttackSuccessShake.Intensity);
	}
}

void AGS_Character::Client_PlayAttackSuccessShakeWithInfo_Implementation(APlayerController* TargetPC,
																		 const FGS_CameraShakeInfo& CustomShakeInfo)
{
	if (TargetPC && TargetPC->IsLocalController() && CustomShakeInfo.ShakeClass)
	{
		TargetPC->ClientStartCameraShake(CustomShakeInfo.ShakeClass, CustomShakeInfo.Intensity);
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
	return WeaponSlots.IsValidIndex(Index) ? WeaponSlots[Index].WeaponInstance : nullptr;
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
	if (InRatio >= SLOW_DEBUFF_SPEED_THRESHOLD && this->GetDebuffComp()->IsDebuffActive(EDebuffType::Slow))
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
	if (InRatio >= SLOW_DEBUFF_SPEED_THRESHOLD && this->GetDebuffComp()->IsDebuffActive(EDebuffType::Slow))
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

void AGS_Character::MulticastRPCPlaySkillMontage_Implementation(UAnimMontage* SkillMontage)
{
	PlayAnimMontage(SkillMontage);
}

void AGS_Character::MulicastRPCStopCurrentSkillMontage_Implementation(UAnimMontage* CurrentSkillMontage)
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
	// 보관을 위해 캡처
	FImpactVFXInfo CurrentVFXInfo = RepImpactVFX;

	// 최적화: 중요도가 너무 낮으면 (멀리 있으면) 이펙트 로딩/생성 스킵
	if (GetSignificance() < GS_Rendering::SIGNIFICANCE_THRESHOLD_ASYNC_LOAD)
	{
		return;
	}

	TWeakObjectPtr<AGS_Character> WeakThis(this);

	// 이전 비동기 로드 취소
	if (PendingImpactVFXLoad.IsValid())
	{
		PendingImpactVFXLoad->CancelHandle();
	}

	// 비동기 로드 시작
	PendingImpactVFXLoad = UGS_AssetLoader::AsyncLoadAsset<UNiagaraSystem>(
		CurrentVFXInfo.VFXAsset,
		[WeakThis, CurrentVFXInfo](UNiagaraSystem* LoadedVFX)
		{
			if (WeakThis.IsValid() && LoadedVFX)
			{
				UNiagaraComponent* SpawnedVFX =
					UNiagaraFunctionLibrary::SpawnSystemAttached(LoadedVFX,
																 WeakThis->GetRootComponent(),
																 NAME_None,
																 FVector::ZeroVector,
																 FRotator::ZeroRotator,
																 EAttachLocation::SnapToTarget,
																 true,						 // bAutoDestroy
																 true,						 // bAutoActivate
																 ENCPoolMethod::AutoRelease, // Pooling 활성화
																 true						 // bPreCullCheck
					);

				if (SpawnedVFX)
				{
					SpawnedVFX->SetWorldScale3D(CurrentVFXInfo.Scale);
				}
			}
		});
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
		Slot.WeaponInstance = World->SpawnActor<AGS_Weapon>(Slot.WeaponClass, Params);
		if (!Slot.WeaponInstance)
		{
			continue;
		}

		Slot.WeaponInstance->AttachToComponent(
			GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, Slot.SocketName);
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

void AGS_Character::SetWeaponHandlingState(EWeaponHandlingState InputWeaponHandlingState)
{
	WeaponHandlingState = InputWeaponHandlingState;
}

bool AGS_Character::ShouldPlayVFXAtLocation(const FVector& Location, float MaxDistance) const
{
	return UGS_VFX_FunctionLibrary::ShouldPlayVFXAtLocation(this, Location, MaxDistance, true);
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
	float CullDistance = 5000.0f;

	// 캐릭터 타입별 상수 거리 적용
	switch (CharacterType)
	{
		case ECharacterType::SmallClaw:
			CullDistance = GS_Rendering::MONSTER_SMALL_CULL_DISTANCE;
			break;
		case ECharacterType::NeedleFang:
			CullDistance = GS_Rendering::MONSTER_MEDIUM_CULL_DISTANCE;
			break;
		case ECharacterType::ShadowFang:
			CullDistance = GS_Rendering::MONSTER_LARGE_CULL_DISTANCE;
			break;
		case ECharacterType::Ares:
		case ECharacterType::Chan:
		case ECharacterType::Merci:
		case ECharacterType::Drakhar:
			CullDistance = 8000.0f;
			break; // 플레이어 캐릭터는 멀리서도 보여야 함
		default:
			break;
	}

	// [멀티플레이 대응] RTS 모드(가디언)일 경우 더 높은 곳에서 내려다보므로 컬링 거리 확장
	if (GS_Rendering::IsRTSMode(this))
	{
		CullDistance *= GS_Rendering::RTS_CULL_DISTANCE_SCALE;
	}

	// 점수 하락 곡선을 더 가파르게 하여 설정 거리 근처에서 확실히 0이 되도록 함
	float DistRatio = DistSq / FMath::Square(CullDistance);
	Score = FMath::Clamp(1.0f - DistRatio, 0.0f, 1.0f);

	return Score;
}

void AGS_Character::OnSignificanceChanged(float NewSignificance)
{
	CurrentSignificance = NewSignificance;

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
		return;

	// 1. 애니메이션 URO (Update Rate Optimization) 제어
	// 중요도가 임계값 미만이면 프레임 스킵 허용
	MeshComp->bEnableUpdateRateOptimizations = (NewSignificance < GS_Rendering::SIGNIFICANCE_THRESHOLD_URO);

	// 2. 중요도에 따른 애니메이션 틱 옵션 조정
	if (NewSignificance > GS_Rendering::SIGNIFICANCE_THRESHOLD_ANIM_HIGH)
	{
		// 중요할 때: 시각적 품질 유지
		MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	else
	{
		// 보통이거나 낮을 때: 화면에 보일 때만 갱신
		MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	}

	// 3. 중요도가 매우 낮으면 Tick 비활성화
	if (PrimaryActorTick.bCanEverTick)
	{
		SetActorTickEnabled(NewSignificance > GS_Rendering::SIGNIFICANCE_THRESHOLD_TICK);
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
		SetNetUpdateFrequency(GS_Rendering::CalculateNetUpdateFrequency(this, GetActorLocation()));
	}

	// 6. 가시성 컬링 (드로우콜 절감)
	// 중요도가 매우 낮으면 (카메라에서 매우 멀면) 메시를 숨김
	const bool bShouldShow = NewSignificance > GS_Rendering::SIGNIFICANCE_THRESHOLD_MESH_VISIBLE;
	if (MeshComp->GetVisibleFlag() != bShouldShow)
	{
		MeshComp->SetVisibility(bShouldShow, true); // true: 자식 컴포넌트(무기 등)도 함께 제어

		// 그림자 상태도 즉시 갱신
		UpdateShadowCulling();
	}
}

void AGS_Character::UpdateShadowCulling()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
		return;

	// Significance Manager가 놓치는 경우를 대비해 직접 거리 기반 가시성 체크 병행
	// IsPlayerControlled()는 멀티플레이에서 다른 클라이언트의 캐릭터를 감지 못함
	// 따라서 클래스 타입(AGS_Player)으로 직접 체크해야 함
	bool bIsPlayerCharacter = Cast<AGS_Player>(this) != nullptr;
	if (!IsLocallyControlled() && !bIsPlayerCharacter)
	{
		float DistSq = 0.0f;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager)
			{
				DistSq = FVector::DistSquared(GetActorLocation(), PC->PlayerCameraManager->GetCameraLocation());
			}
		}

		// CalculateSignificance와 동일한 기준 적용
		float BaseCullDist = 3000.0f;
		switch (CharacterType)
		{
			case ECharacterType::SmallClaw:
				BaseCullDist = GS_Rendering::MONSTER_SMALL_CULL_DISTANCE;
				break;
			case ECharacterType::NeedleFang:
				BaseCullDist = GS_Rendering::MONSTER_MEDIUM_CULL_DISTANCE;
				break;
			case ECharacterType::ShadowFang:
				BaseCullDist = GS_Rendering::MONSTER_LARGE_CULL_DISTANCE;
				break;
			default:
				BaseCullDist = 6000.0f;
				break;
		}

		if (GS_Rendering::IsRTSMode(this))
			BaseCullDist *= GS_Rendering::RTS_CULL_DISTANCE_SCALE;

		// 5% 여유 공간을 두고 가시성 강제 업데이트
		bool bInRange = DistSq < FMath::Square(BaseCullDist * 1.05f);
		if (MeshComp->GetVisibleFlag() != bInRange)
		{
			MeshComp->SetVisibility(bInRange, true);
		}
	}

	// 기존 그림자 컬링 로직 실행
	GS_Rendering::UpdateShadowCulling(this, MeshComp);
}
void AGS_Character::Multicast_ApplyHitStop_Implementation(float Duration, float TimeDilation, bool bPlayShake)
{
	if (IsRunningDedicatedServer())
		return;

	// 멀티플레이 최적화: 플레이어 캐릭터는 히트스톱 연출 적용
	// IsPlayerControlled()는 멀티플레이에서 다른 클라이언트 캐릭터를 감지 못하므로 클래스 타입으로 체크
	bool bIsPlayerCharacter = Cast<AGS_Player>(this) != nullptr;
	if (!IsLocallyControlled() && !bIsPlayerCharacter)
		return;

	// Tactile Camera: 본인일 때만 카메라 쉐이크
	if (bPlayShake && CameraShakeComp && IsLocallyControlled())
	{
		CameraShakeComp->PlayCameraShake(AttackSuccessShake);
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance)
		return;

	UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();
	if (!CurrentMontage)
		return;

	float OriginalPlayRate = AnimInstance->Montage_GetPlayRate(CurrentMontage);

	// 멀티플레이 유의: 완전 정지(0.0)는 렉처럼 보일 수 있으므로 아주 미세한 움직임(0.1) 유지
	float HitStopPlayRate = 0.1f;
	if (OriginalPlayRate <= HitStopPlayRate)
		return;

	AnimInstance->Montage_SetPlayRate(CurrentMontage, HitStopPlayRate);

	FTimerHandle HitStopTimer;
	TWeakObjectPtr<AGS_Character> WeakThis(this);
	TWeakObjectPtr<UAnimInstance> WeakAnimInstance(AnimInstance);
	TWeakObjectPtr<UAnimMontage> WeakMontage(CurrentMontage);

	GetWorldTimerManager().SetTimer(
		HitStopTimer,
		[WeakThis, WeakAnimInstance, WeakMontage, OriginalPlayRate]()
		{
			if (WeakThis.IsValid() && WeakAnimInstance.IsValid())
			{
				// 아직 같은 몽타주를 재생 중이라면 PlayRate 복구
				if (WeakAnimInstance->GetCurrentActiveMontage() == WeakMontage.Get())
				{
					WeakAnimInstance->Montage_SetPlayRate(WeakMontage.Get(), OriginalPlayRate);
				}
			}
		},
		Duration,
		false);
}

void AGS_Character::NotifyHealed(AGS_Character* Healer, float Amount)
{
	if (!HasAuthority() || !Healer || Healer == this || Amount <= 0.1f)
	{
		return;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();
	bool bFound = false;

	// 기존 기록 업데이트
	for (FSupportRecord& Record : SupportHistory)
	{
		if (Record.Supporter.Get() == Healer)
		{
			Record.SupportWeight += Amount;
			Record.LastSupportTime = CurrentTime;
			bFound = true;
			break;
		}
	}

	// 새로운 기록 추가
	if (!bFound)
	{
		FSupportRecord NewRecord;
		NewRecord.Supporter = Healer;
		NewRecord.SupportWeight = Amount;
		NewRecord.LastSupportTime = CurrentTime;
		SupportHistory.Add(NewRecord);
	}

	// 오래된 기록 정리 (최적화)
	SupportHistory.RemoveAll(
		[CurrentTime, this](const FSupportRecord& Record)
		{ return !Record.Supporter.IsValid() || (CurrentTime - Record.LastSupportTime) > AssistWindowSeconds; });
}

void AGS_Character::NotifyBuffed(AGS_Character* Buffer, EPositiveEffectType BuffType)
{
	if (!HasAuthority() || !Buffer || Buffer == this)
	{
		return;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();
	bool bFound = false;

	// 버프는 일정 수준의 가중치를 부여하여 어시스트 자격을 줌
	float BuffWeight = 100.0f;

	for (FSupportRecord& Record : SupportHistory)
	{
		if (Record.Supporter.Get() == Buffer)
		{
			Record.SupportWeight += BuffWeight;
			Record.LastSupportTime = CurrentTime;
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		FSupportRecord NewRecord;
		NewRecord.Supporter = Buffer;
		NewRecord.SupportWeight = BuffWeight;
		NewRecord.LastSupportTime = CurrentTime;
		SupportHistory.Add(NewRecord);
	}

	// 오래된 기록 정리 (최적화)
	SupportHistory.RemoveAll(
		[CurrentTime, this](const FSupportRecord& Record)
		{ return !Record.Supporter.IsValid() || (CurrentTime - Record.LastSupportTime) > AssistWindowSeconds; });
}
