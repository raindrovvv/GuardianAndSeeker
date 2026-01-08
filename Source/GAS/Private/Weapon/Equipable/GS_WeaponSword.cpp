// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Equipable/GS_WeaponSword.h"
#include "Character/GS_Character.h"
#include "Character/Player/Guardian/GS_Guardian.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Seeker/GS_Ares.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Character/Component/GS_StatComp.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "Engine/DamageEvents.h"
#include "AkGameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/World.h"
#include "Character/F_GS_DamageEvent.h"
#include "AI/RTS/GS_RTSController.h"
#include "Kismet/GameplayStatics.h"
#include "ResourceSystem/Aether/GS_AetherExtractor.h"

AGS_WeaponSword::AGS_WeaponSword()
{
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>("Mesh");
	RootComponent = Mesh;

	HitBox = CreateDefaultSubobject<UBoxComponent>("HitBox");
	HitBox->SetupAttachment(Mesh);
	HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitBox->OnComponentBeginOverlap.AddDynamic(this, &AGS_WeaponSword::OnHit);
}


void AGS_WeaponSword::BeginPlay()
{
	Super::BeginPlay();

	OwnerChar = Cast<AGS_Character>(GetOwner());

	// === HitBox에 방어 가능 태그 추가 ===
	if (HitBox)
	{
		HitBox->ComponentTags.AddUnique(FName("DEFENSIBLE_ATTACK"));
	}
}

void AGS_WeaponSword::EnableHit()
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

	// 히트 액터 목록 초기화 (새로운 공격 시작 시)
	ClearHitActors();

	// === 안전장치: 3초 후 자동 비활성화 ===
	ClearSafetyTimer();
	if (UWorld* World = GetWorld(); World && !World->bIsTearingDown)
	{
		World->GetTimerManager().SetTimer(SafetyTimerHandle, this, &AGS_WeaponSword::DisableHit, DEFAULT_SAFETY_DURATION, false);
	}
}

void AGS_WeaponSword::DisableHit()
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

void AGS_WeaponSword::ServerEnableHit_Implementation()
{
	// 레벨 전환 시 null 참조 방지
	if (!IsValidForLevelTransition())
	{
		return;
	}

	// 추가 안전성 검사: 액터와 컴포넌트 유효성 확인
	if (!IsValid(this) || IsActorBeingDestroyed())
	{
		return;
	}

	// 현재 활성화된 노티파이 카운트 증가 (중첩 보호)
	ActiveNotifyCount++;

	// HitBox 안전하게 활성화
	if (HitBox && IsValid(HitBox) && !HitBox->IsBeingDestroyed())
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

		// [중요] 물리 상태 즉시 업데이트 (다음 틱까지 기다리지 않고 갱신)
		HitBox->UpdateOverlaps();
	}

	// [중요] 중복 타격 방지: 공격 시퀀스가 처음 시작될 때만 히트 목록 초기화
	if (ActiveNotifyCount == 1)
	{
		ClearHitActors();
	}

	// === 안전장치: 3초 후 자동 비활성화 (노티파이가 씹힐 경우 대비) ===
	ClearSafetyTimer();
	if (UWorld* World = GetWorld(); World && !World->bIsTearingDown)
	{
		World->GetTimerManager().SetTimer(SafetyTimerHandle, this, &AGS_WeaponSword::ServerDisableHit, DEFAULT_SAFETY_DURATION, false);
	}

	// [중요] 이미 겹쳐 있는 액터들에 대해 강제 히트 판정
	if (HitBox)
	{
		HitBox->UpdateOverlaps();

		TArray<AActor*> OverlappingActors;
		HitBox->GetOverlappingActors(OverlappingActors);

		for (AActor* Actor : OverlappingActors)
		{
			if (Actor && Actor != this && Actor != OwnerChar && !HitActors.Contains(Actor))
			{
				FHitResult Hit;
				Hit.HitObjectHandle = FActorInstanceHandle(Actor);
				Hit.Component = Cast<UPrimitiveComponent>(Actor->GetRootComponent());

				OnHit(HitBox, Actor, Hit.Component.Get(), 0, false, Hit);
			}
		}
	}
}

void AGS_WeaponSword::ServerDisableHit_Implementation()
{
	// 레벨 전환 시 null 참조 방지
	if (!IsValidForLevelTransition())
	{
		return;
	}

	// 추가 안전성 검사
	if (!IsValid(this) || IsActorBeingDestroyed())
	{
		return;
	}

	// 현재 활성화된 노티파이 카운트 감소
	ActiveNotifyCount = FMath::Max(0, ActiveNotifyCount - 1);

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

void AGS_WeaponSword::OnHit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
		return;

	// [안전장치] 현재 공격 애니메이션 노티파이가 활성화된 상태가 아니면 무시
	if (ActiveNotifyCount <= 0)
	{
		return;
	}

	// [필수] 레벨 전환 및 소유자 본인 타격 철저 차단
	if (!IsValidForLevelTransition() || !OtherActor || OtherActor == this || OtherActor == OwnerChar || HitActors.Contains(OtherActor))
	{
		return;
	}

	if (!IsOwnerCharValid())
		return;

	// [필수] 환경(땅, 벽 등)을 포함한 모든 유효 타격에 대해 사운드 및 VFX 재생
	HitActors.Add(OtherActor);
	ESwordHitTargetType TargetType = DetermineTargetType(OtherActor);
	FHitResult CorrectHitResult = CreateCorrectHitResult(SweepResult, bFromSweep);

	Multicast_PlayHitSound(TargetType, CorrectHitResult);
	Multicast_PlayHitVFX(TargetType, CorrectHitResult);

	// [핵심] 캐릭터와 특수 오브젝트가 아닌 경우 여기서 종료 (데미지 처리 스킵)
	AGS_Character* Damaged = Cast<AGS_Character>(OtherActor);
	AGS_AetherExtractor* Extractor = Cast<AGS_AetherExtractor>(OtherActor);

	if (!Damaged && !Extractor)
	{
		return;
	}

	AGS_Character* Attacker = OwnerChar;

	// 캐릭터 대상일 때만 데미지 및 특수 효과 처리
	if (Damaged && Attacker && Damaged->IsEnemy(Attacker))
	{
		// [지연 해소] 데미지 계산 및 적용을 최상위로 이동 (연출보다 먼저 처리)
		UGS_StatComp* DamagedStat = Damaged->GetStatComp();
		if (DamagedStat)
		{
			float Damage = DamagedStat->CalculateDamage(Attacker, Damaged);
			FGS_DamageEvent DamageEvent;
			DamageEvent.HitReactType = EHitReactType::Interrupt;
			Damaged->TakeDamage(Damage, DamageEvent, OwnerChar->GetController(), OwnerChar);

			// 피격자 히트스탑 (안정적인 피드백 제공)
			Damaged->Multicast_ApplyHitStop(0.06f, 0.0f, false);
		}

		// [중복 방지] 타격 성공 즉시 콜리전 비활성화 (지연 없이 즉각 차단)
		if (HitBox)
		{
			HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		// 2. 슬래시 이펙트 재생 (실제 데미지가 발생할 때만, 피격자가 방어 중이 아닐 때만)
		if (WeaponVFXComponent && !Damaged->IsDefending())
		{
			ESeekerAuraType AttackerAuraType = GetSeekerAuraType(OwnerChar);
			WeaponVFXComponent->PlaySlashVFX(CorrectHitResult, AttackerAuraType);
		}

		// 3. 아우라 이펙트 트리거 (가디언이나 몬스터를 타격했을 때)
		TriggerHitAuraOnHit(Damaged);

		// 4. 캐릭터별 특수 공격 효과 처리 (다형성 활용)
		int32 CurrentCombo = 0;
		if (AGS_Seeker* SeekerAttacker = Cast<AGS_Seeker>(Attacker))
		{
			CurrentCombo = SeekerAttacker->CurrentComboIndex;
		}
		Attacker->OnAttackHitSuccess(CurrentCombo, CorrectHitResult);
	}
}

ESwordHitTargetType AGS_WeaponSword::DetermineTargetType(AActor* OtherActor) const
{
	if (Cast<AGS_Monster>(OtherActor))
	{
		return ESwordHitTargetType::DungeonMonster;
	}
	else if (Cast<AGS_Guardian>(OtherActor))
	{
		return ESwordHitTargetType::Guardian;
	}
	else if (Cast<AGS_Seeker>(OtherActor))
	{
		return ESwordHitTargetType::Seeker;
	}
	else if (Cast<AGS_Character>(OtherActor))
	{
		return ESwordHitTargetType::Other;
	}
	else
	{
		return ESwordHitTargetType::Structure;
	}
}

void AGS_WeaponSword::PlayHitSound(ESwordHitTargetType TargetType, const FHitResult& SweepResult)
{
	UAkAudioEvent* SoundEventToPlay = nullptr;

	switch (TargetType)
	{
	case ESwordHitTargetType::Guardian:
	case ESwordHitTargetType::DungeonMonster:
		SoundEventToPlay = HitPawnSoundEvent;
		break;
	case ESwordHitTargetType::Seeker:
		SoundEventToPlay = HitSeekerSoundEvent;
		break;
	case ESwordHitTargetType::Structure:
		SoundEventToPlay = HitStructureSoundEvent;
		break;
	case ESwordHitTargetType::Other:
		break;
	default:
		break;
	}

	if (SoundEventToPlay)
	{
		PlayHitSoundAtLocation(SoundEventToPlay, SweepResult.ImpactPoint);
	}
}

void AGS_WeaponSword::PlayHitVFX(ESwordHitTargetType TargetType, const FHitResult& SweepResult)
{
	UNiagaraSystem* VFXToPlay = nullptr;

	switch (TargetType)
	{
	case ESwordHitTargetType::Guardian:
	case ESwordHitTargetType::DungeonMonster:
		VFXToPlay = HitPawnVFX;
		break;
	case ESwordHitTargetType::Seeker:
		VFXToPlay = HitSeekerVFX;
		break;
	case ESwordHitTargetType::Structure:
		VFXToPlay = HitStructureVFX;
		break;
	case ESwordHitTargetType::Other:
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

// 멀티캐스트 함수 구현
bool AGS_WeaponSword::Multicast_PlayHitSound_Validate(ESwordHitTargetType TargetType, const FHitResult& SweepResult)
{
	return true;
}

void AGS_WeaponSword::Multicast_PlayHitSound_Implementation(ESwordHitTargetType TargetType, const FHitResult& SweepResult)
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

bool AGS_WeaponSword::Multicast_PlayHitVFX_Validate(ESwordHitTargetType TargetType, const FHitResult& SweepResult)
{
	return true;
}

void AGS_WeaponSword::Multicast_PlayHitVFX_Implementation(ESwordHitTargetType TargetType, const FHitResult& SweepResult)
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


// 히트 박스 반환 구현
UBoxComponent* AGS_WeaponSword::GetHitBox() const
{
	return HitBox;
}

// 특화 헬퍼 함수 구현
FHitResult AGS_WeaponSword::CreateCorrectHitResult(const FHitResult& OriginalResult, bool bFromSweep) const
{
	if (!bFromSweep && IsOwnerCharValid() && OriginalResult.GetActor())
	{
		// Sword의 경우 더 정확한 히트 포인트 계산을 위해 전용 함수 활용
		return CalculateMoreAccurateHitPoint(OriginalResult.GetActor());
	}

	// 부모 클래스의 기본 구현 사용
	return Super::CreateCorrectHitResult(OriginalResult, bFromSweep);
}