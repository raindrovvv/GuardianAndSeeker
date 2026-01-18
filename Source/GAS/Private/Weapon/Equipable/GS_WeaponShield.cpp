// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Equipable/GS_WeaponShield.h"
#include "Character/GS_Character.h"
#include "Character/Player/Guardian/GS_Guardian.h"
#include "Character/F_GS_DamageEvent.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Components/BoxComponent.h"
#include "Engine/DamageEvents.h"
#include "Character/Component/GS_StatComp.h"
#include "AkGameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/World.h"
#include "Character/F_GS_DamageEvent.h"
#include "ResourceSystem/Aether/GS_AetherExtractor.h"
#include "AI/RTS/GS_RTSController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/GS_SeekerAudioComponent.h"


// Sets default values
AGS_WeaponShield::AGS_WeaponShield()
{
	// server
	bReplicates = true;

	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// Set SKM
	ShieldMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ShieldMeshComponent"));
	RootComponent = ShieldMeshComponent;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
	    TEXT("/Game/Weapons/Shield_02/SKM_Shield_02_L.SKM_Shield_02_L"));

	if (MeshAsset.Succeeded())
	{
		ShieldMeshComponent->SetSkeletalMesh(MeshAsset.Object);
	}

	// 공격용 콜리전 (기존 HitBox)
	AttackHitBox = CreateDefaultSubobject<UBoxComponent>("AttackHitBox");
	AttackHitBox->SetupAttachment(ShieldMeshComponent);
	AttackHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttackHitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	AttackHitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AttackHitBox->SetGenerateOverlapEvents(true);

	// 공격용 콜리전 크기 설정
	AttackHitBox->SetBoxExtent(FVector(80.0f, 120.0f, 150.0f));
	AttackHitBox->SetRelativeLocation(FVector(50.0f, 0.0f, 0.0f));
	AttackHitBox->OnComponentBeginOverlap.AddDynamic(this, &AGS_WeaponShield::OnAttackHit);

	// 방어용 콜리전
	DefenseHitBox = CreateDefaultSubobject<UBoxComponent>("DefenseHitBox");
	DefenseHitBox->SetupAttachment(ShieldMeshComponent);
	DefenseHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DefenseHitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	DefenseHitBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	DefenseHitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DefenseHitBox->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Overlap);
	DefenseHitBox->SetGenerateOverlapEvents(true);

	// 방어용 콜리전 크기 설정 (방패 주변으로 최적화)
	DefenseHitBox->SetBoxExtent(FVector(50.0f, 70.0f, 90.0f));
	DefenseHitBox->SetRelativeLocation(FVector(20.0f, 0.0f, 0.0f));
	DefenseHitBox->OnComponentBeginOverlap.AddDynamic(this, &AGS_WeaponShield::OnDefenseHit);
	DefenseHitBox->OnComponentEndOverlap.AddDynamic(this, &AGS_WeaponShield::OnDefenseEndOverlap);
}

AGS_Character* AGS_WeaponShield::FindUltimateAttacker(AActor* InActor)
{
	AActor* CurrentActor = InActor;
	AGS_Character* FoundCharacter = nullptr;

	// 최대 10번의 연쇄만 탐색 (무한 루프 방지)
	for (int32 i = 0; i < 10 && IsValid(CurrentActor); ++i)
	{
		// 1. 현재 액터가 AGS_Character인지 확인
		FoundCharacter = Cast<AGS_Character>(CurrentActor);
		if (FoundCharacter)
		{
			// 찾았으면 즉시 반환
			return FoundCharacter;
		}

		// 2. (NEW) 현재 액터의 Instigator가 있는지 확인 (투사체 케이스)
		APawn* InstigatorPawn = CurrentActor->GetInstigator();
		if (IsValid(InstigatorPawn))
		{
			// Instigator가 Pawn이므로, 바로 AGS_Character로 캐스팅 시도
			FoundCharacter = Cast<AGS_Character>(InstigatorPawn);
			if (FoundCharacter)
			{
				// Instigator가 AGS_Character면 바로 반환
				return FoundCharacter;
			}
			else
			{
				// Instigator가 AGS_Character는 아니지만
				// 다음 탐색을 위해 CurrentActor를 Instigator로 설정
				CurrentActor = InstigatorPawn;
				continue; // 다음 루프 시작
			}
		}

		// 3. (Original) Instigator가 없으면, Owner를 탐색 (무기, 몬스터 콜리전 케이스)
		AActor* OwnerActor = CurrentActor->GetOwner();
		if (IsValid(OwnerActor))
		{
			// 3a. 소유자가 컨트롤러인지 확인 (몬스터 콜리전 케이스)
			AController* OwnerAsController = Cast<AController>(OwnerActor);
			if (IsValid(OwnerAsController))
			{
				// 컨트롤러가 빙의한 폰을 다음 탐색 대상으로 지정
				CurrentActor = OwnerAsController->GetPawn();
			}
			else
			{
				// 3b. 소유자가 컨트롤러가 아님 (무기 계층 케이스)
				CurrentActor = OwnerActor;
			}
		}
		else
		{
			// Owner도 없으면 탐색 종료
			CurrentActor = nullptr;
		}
	}

	// 탐색 실패
	return nullptr;
}

void AGS_WeaponShield::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

UBoxComponent* AGS_WeaponShield::GetHitBox() const
{
	return AttackHitBox;
}

// Called when the game starts or when spawned
void AGS_WeaponShield::BeginPlay()
{
	Super::BeginPlay();
	OwnerChar = Cast<AGS_Character>(GetOwner());

	if (AttackHitBox)
	{
		AttackHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (DefenseHitBox)
	{
		DefenseHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AGS_WeaponShield::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레벨 전환 시 타이머 정리
	ClearSafetyTimer();

	// 콜리전 비활성화로 추가적인 이벤트 방지
	DisableAllCollisions();

	// 히트 액터 목록 정리
	AttackHitActors.Empty();
	DefenseHitActors.Empty();

	Super::EndPlay(EndPlayReason);
}


void AGS_WeaponShield::OnAttackHit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                   int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	// 공격 애니메이션 노티파이가 활성화된 상태인지 확인
	if (ActiveNotifyCount <= 0)
	{
		return;
	}

	// 레벨 전환 시 null 참조 방지
	if (!IsValidForLevelTransition())
	{
		return;
	}

	// 중복 히트 및 자기 자신/소유자 히트 방지 (공격용)
	if (!OtherActor || OtherActor == this || OtherActor == OwnerChar || AttackHitActors.Contains(OtherActor))
	{
		return;
	}

	// 유효 타격 시 히트 액터 목록에 추가
	AttackHitActors.Add(OtherActor);

	AGS_Character* Damaged = Cast<AGS_Character>(OtherActor);
	AGS_Character* Attacker = OwnerChar;

	// 1. 에테르 추출기 처리
	if (!Damaged && Attacker)
	{
		if (AGS_AetherExtractor* AetherExtractor = Cast<AGS_AetherExtractor>(OtherActor))
		{
			float Damage = Attacker->GetStatComp()->GetAttackPower();
			AetherExtractor->TakeDamageBySeeker(Damage, OwnerChar);

			// 즉시 콜리전 차단 (중복 타격 방지)
			if (AttackHitBox)
				AttackHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			return;
		}
	}

	// 2. 적 캐릭터 타격 처리 (가장 높은 우선순위)
	if (Damaged && Attacker && Damaged->IsEnemy(Attacker))
	{
		// 데미지 적용
		UGS_StatComp* DamagedStat = Damaged->GetStatComp();
		if (DamagedStat)
		{
			bool bIsCritical = false;
			float Damage = DamagedStat->CalculateDamage(Attacker, Damaged, bIsCritical);
			FGS_DamageEvent DamageEvent;
			DamageEvent.HitReactType = EHitReactType::Interrupt;
			DamageEvent.bIsCritical = bIsCritical;
			Damaged->TakeDamage(Damage, DamageEvent, OwnerChar->GetController(), OwnerChar);

			// 피격자 히트스탑 적용
			Damaged->Multicast_ApplyHitStop(0.08f, 0.0f, false);
		}

		// 타격 성공 즉시 콜리전 비활성화
		if (AttackHitBox)
		{
			AttackHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		// 연출 처리 (사운드, VFX, 캐릭터 특수 효과)
		EShieldHitTargetType TargetType = DetermineTargetType(OtherActor);
		FHitResult CorrectHitResult = CreateCorrectHitResult(SweepResult, bFromSweep);

		Multicast_PlayHitSound(TargetType, CorrectHitResult);
		Multicast_PlayHitVFX(TargetType, CorrectHitResult);

		if (WeaponVFXComponent)
		{
			ESeekerAuraType AttackerAuraType = GetSeekerAuraType(OwnerChar);
			WeaponVFXComponent->PlaySlashVFX(CorrectHitResult, AttackerAuraType);
		}

		TriggerHitAuraOnHit(Damaged);

		int32 CurrentCombo = 0;
		if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(Attacker))
		{
			CurrentCombo = Seeker->CurrentComboIndex;
		}
		Attacker->OnAttackHitSuccess(CurrentCombo, CorrectHitResult);

		return;
	}

	// 3. 캐릭터가 아닌 환경/구조물 타격 처리
	EShieldHitTargetType TargetType = DetermineTargetType(OtherActor);
	FHitResult CorrectHitResult = CreateCorrectHitResult(SweepResult, bFromSweep);
	Multicast_PlayHitSound(TargetType, CorrectHitResult);
	Multicast_PlayHitVFX(TargetType, CorrectHitResult);
}

EShieldHitTargetType AGS_WeaponShield::DetermineTargetType(AActor* OtherActor) const
{
	if (Cast<AGS_Monster>(OtherActor))
	{
		return EShieldHitTargetType::DungeonMonster;
	}
	else if (Cast<AGS_Guardian>(OtherActor))
	{
		return EShieldHitTargetType::Guardian;
	}
	else if (Cast<AGS_Seeker>(OtherActor))
	{
		return EShieldHitTargetType::Seeker;
	}
	else if (OtherActor && OtherActor->ActorHasTag("Structure"))
	{
		return EShieldHitTargetType::Structure;
	}
	else
	{
		return EShieldHitTargetType::Other;
	}
}

void AGS_WeaponShield::PlayHitSound(EShieldHitTargetType TargetType, const FHitResult& SweepResult)
{
	UAkAudioEvent* SoundEventToPlay = nullptr;

	switch (TargetType)
	{
	case EShieldHitTargetType::Guardian:
	case EShieldHitTargetType::DungeonMonster:
		SoundEventToPlay = HitPawnSoundEvent;
		break;
	case EShieldHitTargetType::Structure:
		SoundEventToPlay = HitStructureSoundEvent;
		break;
	case EShieldHitTargetType::Seeker:
	case EShieldHitTargetType::Other:
		break;
	default:
		break;
	}

	if (SoundEventToPlay)
	{
		PlayHitSoundAtLocation(SoundEventToPlay, SweepResult.ImpactPoint);
	}
}

void AGS_WeaponShield::PlayHitVFX(EShieldHitTargetType TargetType, const FHitResult& SweepResult)
{
	UNiagaraSystem* VFXToPlay = nullptr;

	switch (TargetType)
	{
	case EShieldHitTargetType::Guardian:
	case EShieldHitTargetType::DungeonMonster:
		VFXToPlay = HitPawnVFX;
		break;
	case EShieldHitTargetType::Structure:
		VFXToPlay = HitStructureVFX;
		break;
	case EShieldHitTargetType::Seeker:
	case EShieldHitTargetType::Other:
		break;
	default:
		break;
	}

	if (VFXToPlay && IsValid(GetWorld()))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		    GetWorld(),
		    VFXToPlay,
		    SweepResult.ImpactPoint,
		    SweepResult.ImpactNormal.Rotation(),
		    FVector(1.0f),
		    true,
		    true);
	}
}

void AGS_WeaponShield::PlayGuardSuccessVFX(EShieldHitTargetType TargetType, const FHitResult& SweepResult)
{
	UNiagaraSystem* VFXToPlay = nullptr;

	switch (TargetType)
	{
	case EShieldHitTargetType::Guardian:
	case EShieldHitTargetType::DungeonMonster:
		VFXToPlay = GuardSuccessPawnVFX;
		break;
	case EShieldHitTargetType::Structure:
		VFXToPlay = GuardSuccessStructureVFX;
		break;
	case EShieldHitTargetType::Seeker:
	case EShieldHitTargetType::Other:
		VFXToPlay = GuardSuccessPawnVFX;
		break;
	default:
		break;
	}

	if (VFXToPlay && IsValid(GetWorld()))
	{
		FVector ShieldCenter = ShieldMeshComponent->GetComponentLocation();
		FRotator ShieldRotation = ShieldMeshComponent->GetComponentRotation();

		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		    GetWorld(),
		    VFXToPlay,
		    ShieldCenter,
		    ShieldRotation,
		    FVector(1.2f),
		    true,
		    true);
	}

	if (WeaponVFXComponent && OwnerChar)
	{
		ESeekerAuraType DefenderAuraType = GetSeekerAuraType(OwnerChar);
		WeaponVFXComponent->PlayGuardSuccessVFX(SweepResult, DefenderAuraType);
	}
}

void AGS_WeaponShield::PlayGuardSuccessSound(EShieldHitTargetType TargetType, const FHitResult& SweepResult)
{
	UAkAudioEvent* SoundEventToPlay = nullptr;

	switch (TargetType)
	{
	case EShieldHitTargetType::Guardian:
	case EShieldHitTargetType::DungeonMonster:
		SoundEventToPlay = GuardSuccessPawnSoundEvent;
		break;
	case EShieldHitTargetType::Structure:
		SoundEventToPlay = GuardSuccessStructureSoundEvent;
		break;
	default:
		break;
	}

	if (SoundEventToPlay)
	{
		PlayHitSoundAtLocation(SoundEventToPlay, SweepResult.ImpactPoint);
	}
}

bool AGS_WeaponShield::Multicast_PlayHitSound_Validate(EShieldHitTargetType TargetType, const FHitResult& SweepResult)
{
	return true;
}
void AGS_WeaponShield::Multicast_PlayHitSound_Implementation(EShieldHitTargetType TargetType, const FHitResult& SweepResult)
{
	if (!IsValidForLevelTransition())
		return;
	PlayHitSound(TargetType, SweepResult);
	PlayLayeredHitSound(SweepResult, SweepResult.GetActor());
}

bool AGS_WeaponShield::Multicast_PlayHitVFX_Validate(EShieldHitTargetType TargetType, const FHitResult& SweepResult)
{
	return true;
}
void AGS_WeaponShield::Multicast_PlayHitVFX_Implementation(EShieldHitTargetType TargetType, const FHitResult& SweepResult)
{
	if (!IsValidForLevelTransition())
		return;
	if (OwnerChar && OwnerChar->ShouldPlayVFXAtLocation(SweepResult.ImpactPoint, 3500.0f))
	{
		PlayHitVFX(TargetType, SweepResult);
	}
}

bool AGS_WeaponShield::Multicast_PlayGuardSuccessVFX_Validate(EShieldHitTargetType TargetType, const FHitResult& SweepResult)
{
	return true;
}
void AGS_WeaponShield::Multicast_PlayGuardSuccessVFX_Implementation(EShieldHitTargetType TargetType, const FHitResult& SweepResult)
{
	if (!IsValidForLevelTransition())
		return;
	if (OwnerChar && OwnerChar->ShouldPlayVFXAtLocation(SweepResult.ImpactPoint, 3500.0f))
	{
		PlayGuardSuccessVFX(TargetType, SweepResult);
	}
}

bool AGS_WeaponShield::Multicast_PlayGuardSuccessSound_Validate(EShieldHitTargetType TargetType, const FHitResult& SweepResult)
{
	return true;
}
void AGS_WeaponShield::Multicast_PlayGuardSuccessSound_Implementation(EShieldHitTargetType TargetType, const FHitResult& SweepResult)
{
	if (!IsValidForLevelTransition())
		return;
	PlayGuardSuccessSound(TargetType, SweepResult);
}

void AGS_WeaponShield::EnableAttackHit()
{
	if (!IsValidForLevelTransition())
		return;
	if (AttackHitBox && IsValid(AttackHitBox) && !AttackHitBox->IsBeingDestroyed())
	{
		AttackHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	AttackHitActors.Empty();
}

void AGS_WeaponShield::DisableAttackHit()
{
	if (!IsValidForLevelTransition())
		return;
	if (AttackHitBox && IsValid(AttackHitBox) && !AttackHitBox->IsBeingDestroyed())
	{
		AttackHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	ClearSafetyTimer();
	ActiveNotifyCount = 0;
}

void AGS_WeaponShield::EnableHit()
{
	if (HasAuthority())
	{
		ServerEnableAttackHit_Implementation();
	}
	else
	{
		ServerEnableAttackHit();
		EnableAttackHit();
	}
}

void AGS_WeaponShield::DisableHit()
{
	if (HasAuthority())
	{
		ServerDisableAttackHit_Implementation();
	}
	else
	{
		ServerDisableAttackHit();
		DisableAttackHit();
	}
}

void AGS_WeaponShield::ServerEnableHit_Implementation()
{
	ServerEnableAttackHit();
}
void AGS_WeaponShield::ServerDisableHit_Implementation()
{
	ServerDisableAttackHit();
	ClearSafetyTimer();
}

void AGS_WeaponShield::ServerEnableAttackHit_Implementation()
{
	if (!IsValidForLevelTransition() || IsActorBeingDestroyed())
		return;
	if (AttackHitBox && IsValid(AttackHitBox) && !AttackHitBox->IsBeingDestroyed())
	{
		AttackHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		AttackHitBox->UpdateOverlaps();
	}
	ActiveNotifyCount++;
	if (ActiveNotifyCount == 1)
	{
		AttackHitActors.Empty();
	}

	ClearSafetyTimer();
	if (UWorld* World = GetWorld(); World && !World->bIsTearingDown)
	{
		World->GetTimerManager().SetTimer(SafetyTimerHandle, this, &AGS_WeaponShield::ServerDisableHit, 0.3f, false);
	}

	if (AttackHitBox)
	{
		TArray<AActor*> OverlappingActors;
		AttackHitBox->GetOverlappingActors(OverlappingActors);
		for (AActor* Actor : OverlappingActors)
		{
			if (Actor && Actor != this && Actor != OwnerChar && !AttackHitActors.Contains(Actor))
			{
				if (IsInFrontAngle(Actor))
				{
					FHitResult Hit;
					Hit.HitObjectHandle = FActorInstanceHandle(Actor);
					Hit.Component = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
					OnAttackHit(AttackHitBox, Actor, Hit.Component.Get(), 0, false, Hit);
				}
			}
		}
	}
}

void AGS_WeaponShield::ServerDisableAttackHit_Implementation()
{
	if (!IsValidForLevelTransition() || IsActorBeingDestroyed())
		return;
	if (AttackHitBox && IsValid(AttackHitBox) && !AttackHitBox->IsBeingDestroyed())
	{
		AttackHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	ActiveNotifyCount = FMath::Max(0, ActiveNotifyCount - 1);
}

void AGS_WeaponShield::OnDefenseHit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                    int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !IsValidForLevelTransition())
		return;
	if (!OtherActor || OtherActor == this || !OtherComp)
		return;
	if (!OtherComp->ComponentHasTag("DEFENSIBLE_ATTACK"))
		return;
	if (OtherComp->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		return;
	if (!IsOwnerCharValid())
		return;

	AActor* AttackerActor = FindUltimateAttacker(OtherActor);
	if (DefenseHitActors.Contains(AttackerActor))
		return;

	FHitResult CorrectHitResult = CreateCorrectHitResult(SweepResult, bFromSweep);
	PlayDefenseEffects(AttackerActor, CorrectHitResult);
}

void AGS_WeaponShield::PlayDefenseEffects(AActor* AttackerActor, const FHitResult& HitResult)
{
	if (!HasAuthority() || !AttackerActor || !OwnerChar || AttackerActor == OwnerChar)
		return;
	if (DefenseHitActors.Contains(AttackerActor))
		return;

	bool bIsEnemy = true;
	if (AGS_Character* AttackerChar = Cast<AGS_Character>(AttackerActor))
	{
		bIsEnemy = OwnerChar->IsEnemy(AttackerChar);
	}

	if (bIsEnemy)
	{
		DefenseHitActors.Add(AttackerActor);
	}
	else
	{
		return;
	}

	AGS_Chan* Chan = Cast<AGS_Chan>(OwnerChar);
	if (!Chan || !Chan->bIsDefending)
	{
		DefenseHitActors.Remove(AttackerActor);
		return;
	}

	EShieldHitTargetType TargetType = DetermineTargetType(AttackerActor);
	Multicast_PlayGuardSuccessVFX(TargetType, HitResult);
	Multicast_PlayGuardSuccessSound(TargetType, HitResult);

	if (UGS_SeekerAudioComponent* SeekerAudio = Chan->GetComponentByClass<UGS_SeekerAudioComponent>())
	{
		SeekerAudio->PlayDefenseSound();
	}
}

void AGS_WeaponShield::OnDefenseEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority() || !IsValidForLevelTransition() || !OtherActor)
		return;
	AActor* AttackerActor = FindUltimateAttacker(OtherActor);
	DefenseHitActors.Remove(AttackerActor);
}

void AGS_WeaponShield::EnableDefenseHit()
{
	if (!IsValidForLevelTransition())
		return;
	if (DefenseHitBox && IsValid(DefenseHitBox) && !DefenseHitBox->IsBeingDestroyed())
	{
		DefenseHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	DefenseHitActors.Empty();
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().SetTimer(DefenseTimerHandle, this, &AGS_WeaponShield::OnDefenseTimer, 1.0f, true);
	}
}

void AGS_WeaponShield::DisableDefenseHit()
{
	if (!IsValidForLevelTransition())
		return;
	if (DefenseHitBox && IsValid(DefenseHitBox) && !DefenseHitBox->IsBeingDestroyed())
	{
		DefenseHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(DefenseTimerHandle);
	}
}

void AGS_WeaponShield::ServerEnableDefenseHit_Implementation()
{
	if (!IsValidForLevelTransition() || IsActorBeingDestroyed())
		return;
	EnableDefenseHit();
}

void AGS_WeaponShield::ServerDisableDefenseHit_Implementation()
{
	if (!IsValidForLevelTransition() || IsActorBeingDestroyed())
		return;
	DisableDefenseHit();
}

void AGS_WeaponShield::DisableAllCollisions()
{
	if (!IsValidForLevelTransition())
		return;
	if (AttackHitBox && IsValid(AttackHitBox) && !AttackHitBox->IsBeingDestroyed())
	{
		AttackHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (DefenseHitBox && IsValid(DefenseHitBox) && !DefenseHitBox->IsBeingDestroyed())
	{
		DefenseHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AGS_WeaponShield::OnDefenseTimer()
{
	DefenseHitActors.Empty();
}