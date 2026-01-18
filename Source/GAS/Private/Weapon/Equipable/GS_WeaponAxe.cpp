// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Equipable/GS_WeaponAxe.h"
#include "Character/GS_Character.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Character/Player/Guardian/GS_Guardian.h"
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
#include "DrawDebugHelpers.h"

// Sets default values
AGS_WeaponAxe::AGS_WeaponAxe()
{
	// Set this actor to call Tick() every frame.
	PrimaryActorTick.bCanEverTick = false;

	AxeMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AxeMeshComponent"));
	RootComponent = AxeMeshComponent;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
	    TEXT("/Game/Weapons/Greataxe_01/SKM_Greataxe_01.SKM_Greataxe_01"));
	if (MeshAsset.Succeeded())
	{
		AxeMeshComponent->SetSkeletalMesh(MeshAsset.Object);
	}

	HitBox = CreateDefaultSubobject<UBoxComponent>("HitBox");
	HitBox->SetupAttachment(AxeMeshComponent);
	HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitBox->OnComponentBeginOverlap.AddDynamic(this, &AGS_WeaponAxe::OnHit);
}

void AGS_WeaponAxe::OnHit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                          int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	// [안전장치] 현재 공격 시퀀스가 활성화된 상태인지 확인
	if (ActiveNotifyCount <= 0)
	{
		return;
	}

	// 레벨 전환 시 null 참조 방지
	if (!IsValidForLevelTransition())
	{
		return;
	}

	// 중복 히트 및 자기 자신/소유자 히트 방지
	if (!OtherActor || OtherActor == this || OtherActor == OwnerChar || HitActors.Contains(OtherActor))
	{
		return;
	}

	// OwnerChar 유효성 확인 (레벨 전환 시 null일 수 있음)
	if (!IsOwnerCharValid())
	{
		return;
	}

	// [필수] 환경(땅, 벽 등)을 포함한 모든 유효 타격에 대해 사운드 및 VFX 재생
	HitActors.Add(OtherActor);
	EAxeHitTargetType TargetType = DetermineTargetType(OtherActor);
	FHitResult CorrectHitResult = CreateCorrectHitResult(SweepResult, bFromSweep);

	Multicast_PlayHitSound(TargetType, CorrectHitResult);
	Multicast_PlayHitVFX(TargetType, CorrectHitResult);

	AGS_Character* Damaged = Cast<AGS_Character>(OtherActor);
	AGS_Character* Attacker = OwnerChar;

	// 에테르 추출기 처리
	if (!Damaged && Attacker)
	{
		if (AGS_AetherExtractor* AetherExtractor = Cast<AGS_AetherExtractor>(OtherActor))
		{
			float Damage = Attacker->GetStatComp()->GetAttackPower();
			AetherExtractor->TakeDamageBySeeker(Damage, OwnerChar);
			SafeDisableHitBoxCollision(HitBox);
		}
	}

	// [핵심] 캐릭터와 특수 오브젝트가 아닌 경우 여기서 종료 (데미지 처리 스킵)
	if (!Damaged || !Attacker || !Damaged->IsEnemy(Attacker))
	{
		return;
	}

	// [중요] 캐릭터 앞방향 체크 (뒤에 있는 적 무시)
	// 약 140도 범위 내에 있는 타겟만 공격 허용
	if (!IsInFrontAngle(Damaged))
	{
		return;
	}

	// --- 여기서부터는 유효한 적을 타격한 경우 ---

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

	// [중복 방지] 타격 성공 즉시 콜리전 비활성화 (지연 없이 즉각 차단)
	if (HitBox)
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 1. 기본 VFX 및 슬래시 이펙트 재생
	Multicast_PlayHitVFX(TargetType, CorrectHitResult);
	if (WeaponVFXComponent)
	{
		ESeekerAuraType AttackerAuraType = GetSeekerAuraType(OwnerChar);
		WeaponVFXComponent->PlaySlashVFX(CorrectHitResult, AttackerAuraType);
	}

	// 2. 아우라 이펙트 트리거
	TriggerHitAuraOnHit(Damaged);

	// 캐릭터 클래스에 위임하여 특수 효과 처리
	int32 CurrentCombo = 0;
	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(Attacker))
	{
		CurrentCombo = Seeker->CurrentComboIndex;
	}
	Attacker->OnAttackHitSuccess(CurrentCombo, CorrectHitResult);
}

EAxeHitTargetType AGS_WeaponAxe::DetermineTargetType(AActor* OtherActor) const
{
	if (Cast<AGS_Monster>(OtherActor))
	{
		return EAxeHitTargetType::DungeonMonster;
	}
	else if (Cast<AGS_Guardian>(OtherActor))
	{
		return EAxeHitTargetType::Guardian;
	}
	else if (Cast<AGS_Seeker>(OtherActor))
	{
		return EAxeHitTargetType::Seeker;
	}
	else if (Cast<AGS_Character>(OtherActor))
	{
		return EAxeHitTargetType::Other;
	}
	else
	{
		return EAxeHitTargetType::Structure;
	}
}

void AGS_WeaponAxe::PlayHitSound(EAxeHitTargetType TargetType, const FHitResult& SweepResult)
{
	UAkAudioEvent* SoundEventToPlay = nullptr;

	switch (TargetType)
	{
	case EAxeHitTargetType::Guardian:
	case EAxeHitTargetType::DungeonMonster:
		SoundEventToPlay = HitPawnSoundEvent;
		break;
	case EAxeHitTargetType::Structure:
		SoundEventToPlay = HitStructureSoundEvent;
		break;
	case EAxeHitTargetType::Seeker:
	case EAxeHitTargetType::Other:
		break;
	default:
		break;
	}

	if (SoundEventToPlay)
	{
		PlayHitSoundAtLocation(SoundEventToPlay, SweepResult.ImpactPoint);
	}
}

void AGS_WeaponAxe::PlayHitVFX(EAxeHitTargetType TargetType, const FHitResult& SweepResult)
{
	UNiagaraSystem* VFXToPlay = nullptr;

	switch (TargetType)
	{
	case EAxeHitTargetType::Guardian:
	case EAxeHitTargetType::DungeonMonster:
		VFXToPlay = HitPawnVFX;
		break;
	case EAxeHitTargetType::Structure:
		VFXToPlay = HitStructureVFX;
		break;
	case EAxeHitTargetType::Seeker:
	case EAxeHitTargetType::Other:
		break;
	default:
		break;
	}

	if (VFXToPlay && GetWorld())
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

// 멀티캐스트 함수 구현
bool AGS_WeaponAxe::Multicast_PlayHitSound_Validate(EAxeHitTargetType TargetType, const FHitResult& SweepResult)
{
	return true;
}

void AGS_WeaponAxe::Multicast_PlayHitSound_Implementation(EAxeHitTargetType TargetType, const FHitResult& SweepResult)
{
	// 레벨 전환 시 null 참조 방지
	if (!IsValidForLevelTransition())
	{
		return;
	}

	PlayHitSound(TargetType, SweepResult);

	/** 멀티 레이어 사운드 (피격/잔향) 재생 */
	PlayLayeredHitSound(SweepResult, SweepResult.GetActor());
}

bool AGS_WeaponAxe::Multicast_PlayHitVFX_Validate(EAxeHitTargetType TargetType, const FHitResult& SweepResult)
{
	return true;
}

void AGS_WeaponAxe::Multicast_PlayHitVFX_Implementation(EAxeHitTargetType TargetType, const FHitResult& SweepResult)
{
	// 레벨 전환 시 null 참조 방지
	if (!IsValidForLevelTransition())
	{
		return;
	}

	// 거리 기반 VFX 컬링 (전역 상수 60m 사용)
	if (OwnerChar && OwnerChar->ShouldPlayVFXAtLocation(SweepResult.ImpactPoint, GS_Rendering::VFX_DISABLE_DISTANCE))
	{
		PlayHitVFX(TargetType, SweepResult);
	}
}

UBoxComponent* AGS_WeaponAxe::GetHitBox() const
{
	return HitBox;
}


void AGS_WeaponAxe::EnableHit()
{
	// 레벨 전환 중인 경우 안전하게 종료
	if (!IsValidForLevelTransition())
	{
		return;
	}

	// HitBox 안전하게 활성화
	if (HitBox && IsValid(HitBox) && !HitBox->IsBeingDestroyed())
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	// [중요] 중복 타격 방지: 공격 시퀀스가 처음 시작될 때만 히트 목록 초기화
	if (ActiveNotifyCount == 1)
	{
		ClearHitActors();
	}

	// 안전장치: 3초 후에 자동으로 비활성화
	ClearSafetyTimer();

	// 레벨 전환 중이 아닌 경우에만 타이머 설정
	if (UWorld* World = GetWorld(); World && !World->bIsTearingDown && IsValid(World))
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (&TimerManager)
		{
			TimerManager.SetTimer(SafetyTimerHandle, this, &AGS_WeaponAxe::DisableHit, DEFAULT_SAFETY_DURATION, false);
		}
	}
}

void AGS_WeaponAxe::DisableHit()
{
	// 레벨 전환 중인 경우 안전하게 종료
	if (!IsValidForLevelTransition())
	{
		return;
	}

	// HitBox 안전하게 비활성화
	if (HitBox && IsValid(HitBox) && !HitBox->IsBeingDestroyed())
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 타이머 및 노티파이 카운트 정리
	ClearSafetyTimer();
	ActiveNotifyCount = 0;
}

void AGS_WeaponAxe::ServerDisableHit_Implementation()
{
	Super::ServerDisableHit_Implementation();

	// 아직 다른 활성 노티파이가 있다면 비활성화 건너뜀
	if (ActiveNotifyCount > 0)
	{
		return;
	}

	// 모든 노티파이가 끝났을 때만 안전하게 비활성화
	if (HitBox && IsValid(HitBox) && !HitBox->IsBeingDestroyed())
	{
		SafeDisableHitBoxCollision(HitBox);
	}

	// 타이머 정리
	ClearSafetyTimer();
}

void AGS_WeaponAxe::ServerEnableHit_Implementation()
{
	Super::ServerEnableHit_Implementation();

	// HitBox 안전하게 활성화
	if (HitBox && IsValid(HitBox) && !HitBox->IsBeingDestroyed())
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

		// 물리 상태 즉시 업데이트
		HitBox->UpdateOverlaps();
	}

	// === 안전장치: 3초 후 자동 비활성화 ===
	ClearSafetyTimer();
	if (UWorld* World = GetWorld(); World && !World->bIsTearingDown)
	{
		World->GetTimerManager().SetTimer(SafetyTimerHandle, this, &AGS_WeaponAxe::ServerDisableHit, DEFAULT_SAFETY_DURATION, false);
	}

	// 이미 겹쳐 있는 액터들에 대해 강제 히트 판정
	// UpdateOverlaps()만으로는 이미 겹친 상태에서 BeginOverlap 이벤트가 발생하지 않으므로
	// 수동으로 겹침 검사 후 OnHit 호출
	if (HitBox)
	{
		TArray<AActor*> OverlappingActors;
		HitBox->GetOverlappingActors(OverlappingActors);

		for (AActor* Actor : OverlappingActors)
		{
			if (Actor && Actor != this && Actor != OwnerChar && !HitActors.Contains(Actor))
			{
				// 캐릭터 앞방향 체크 (뒤에 있는 적 무시)
				if (!IsInFrontAngle(Actor))
				{
					continue;
				}

				FHitResult Hit;
				Hit.HitObjectHandle = FActorInstanceHandle(Actor);
				Hit.Component = Cast<UPrimitiveComponent>(Actor->GetRootComponent());

				// OnHit 수동 호출 (bFromSweep = false)
				OnHit(HitBox, Actor, Hit.Component.Get(), 0, false, Hit);
			}
		}
	}
}

// Called when the game starts or when spawned
void AGS_WeaponAxe::BeginPlay()
{
	Super::BeginPlay();

	// OwnerChar 설정
	OwnerChar = Cast<AGS_Character>(GetOwner());

	// === HitBox에 방어 가능 태그 추가 ===
	if (HitBox)
	{
		HitBox->ComponentTags.AddUnique(FName("DEFENSIBLE_ATTACK"));
	}
}

// 특화 헬퍼 함수 구현 (타이머 관련)
void AGS_WeaponAxe::ClearSafetyTimer()
{
	Super::ClearSafetyTimer();
}