// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Equipable/GS_WeaponSword.h"
#include "Character/GS_Character.h"
#include "Character/Player/Guardian/GS_Guardian.h"
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

		// [중요] 물리 상태 즉시 업데이트 (다음 틱까지 기다리지 않고 겹침 상태 갱신)
		HitBox->UpdateOverlaps();
	}

	// 히트 액터 목록 초기화 (새로운 공격 시작 시)
	ClearHitActors();

	// 예약된 히트박스 비활성화가 있다면 취소 (아레스 3타 등 연속 공격 지원)
	ClearSafetyTimer();

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

	// 추가 안전성 검사: 액터와 컴포넌트 유효성 확인
	if (!IsValid(this) || IsActorBeingDestroyed())
	{
		return;
	}

	// 현재 활성화된 노티파이 카운트 감소
	ActiveNotifyCount = FMath::Max(0, ActiveNotifyCount - 1);

	// 아직 다른 활성 노티파이가 있다면 비활성화 건너뜀 (섹션 전환 시 씹힘 방지)
	if (ActiveNotifyCount > 0)
	{
		return;
	}

	// 모든 노티파이가 끝났을 때만 안전하게 비활성화
	if (HitBox && IsValid(HitBox) && !HitBox->IsBeingDestroyed())
	{
		SafeDisableHitBoxCollision(HitBox);
	}
}

void AGS_WeaponSword::OnHit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
		return;

	// [필수] 레벨 전환 및 소유자 본인 타격 철저 차단
	if (!IsValidForLevelTransition() || !OtherActor || OtherActor == this || OtherActor == OwnerChar || HitActors.Contains(OtherActor))
	{
		return;
	}

	if (!IsOwnerCharValid())
		return;

	// [핵심] 환경(땅, 벽 등) 타격 무시 - 오직 캐릭터와 특수 오브젝트만 처리
	AGS_Character* Damaged = Cast<AGS_Character>(OtherActor);
	AGS_AetherExtractor* Extractor = Cast<AGS_AetherExtractor>(OtherActor);

	if (!Damaged && !Extractor)
	{
		return; // 환경 오브젝트는 HitActors에 추가하지 않음
	}

	// 유효한 타겟만 HitActors에 추가 (중복 타격 방지)
	HitActors.Add(OtherActor);

	ESwordHitTargetType TargetType = DetermineTargetType(OtherActor);

	// HitResult 보정 로직
	FHitResult CorrectHitResult = SweepResult;

	// [핵심] 수동 판정이나 Sweep 정보가 없는 경우, VFX가 묻히지 않도록 표면 지점 강제 계산
	if (Damaged && (!bFromSweep || CorrectHitResult.ImpactPoint.IsNearlyZero()))
	{
		FVector TargetLoc = Damaged->GetActorLocation();
		FVector WeaponLoc = HitBox ? HitBox->GetComponentLocation() : GetActorLocation();

		FVector Dir = (WeaponLoc - TargetLoc).GetSafeNormal();
		if (Dir.IsNearlyZero())
			Dir = OwnerChar->GetActorForwardVector() * -1.0f;

		float Radius = 50.0f;
		if (UCapsuleComponent* Cap = Damaged->GetCapsuleComponent())
		{
			Radius = Cap->GetScaledCapsuleRadius();
		}

		CorrectHitResult.ImpactPoint = TargetLoc + (Dir * Radius);
		CorrectHitResult.ImpactNormal = Dir;
		CorrectHitResult.Location = CorrectHitResult.ImpactPoint;
	}

	Multicast_PlayHitSound(TargetType, CorrectHitResult);
	Multicast_PlayHitVFX(TargetType, CorrectHitResult);

	AGS_Character* Attacker = OwnerChar;

	// 캐릭터 대상일 때만 데미지 및 특수 효과 처리
	if (Damaged && Attacker && Damaged->IsEnemy(Attacker))
	{
		// 데미지 계산 수행
		UGS_StatComp* DamagedStat = Damaged->GetStatComp();
		if (!DamagedStat)
		{
			return;
		}

		float Damage = DamagedStat->CalculateDamage(Attacker, Damaged);

		// 2. 슬래시 이펙트 재생 (실제 데미지가 발생할 때만, 찬이 방어 중이 아닐 때만)
		bool bShouldPlaySlashVFX = (Damage > 0.0f && WeaponVFXComponent);

		// 찬이 방어 상태인지 확인하여 혈흔 이펙트 제거
		if (AGS_Chan* ChanTarget = Cast<AGS_Chan>(Damaged))
		{
			if (ChanTarget->bIsDefending)
			{
				bShouldPlaySlashVFX = false; // 찬이 가드 상태일 때는 혈흔 이펙트 없음
			}
		}

		if (bShouldPlaySlashVFX)
		{
			// 공격자(OwnerChar)의 시커 타입을 직접 전달
			ESeekerAuraType AttackerAuraType = GetSeekerAuraType(OwnerChar);
			WeaponVFXComponent->PlaySlashVFX(CorrectHitResult, AttackerAuraType);
		}

		// 3. 아우라 이펙트 트리거 (가디언이나 몬스터를 타격했을 때)
		TriggerHitAuraOnHit(Damaged);

		// 4. '아레스'의 특수 공격일 경우 추가 효과(사운드, VFX) 재생
		if (AGS_Ares* Ares = Cast<AGS_Ares>(Attacker))
		{
			if (Ares->CurrentComboIndex == 4)
			{
				// 추가 사운드
				Ares->Multicast_OnAttackHit(Ares->CurrentComboIndex);

				// 추가 VFX
				if (Ares->FinalAttackHitVFX)
				{
					Multicast_PlaySpecialHitVFX(Ares->FinalAttackHitVFX, CorrectHitResult);
				}
			}
		}

		FVector ShotDir = (Damaged->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal();
		/*FPointDamageEvent DamageEvent;
	DamageEvent.ShotDirection = ShotDir;
	DamageEvent.HitInfo = SweepResult;
	DamageEvent.DamageTypeClass = UDamageType::StaticClass();*/
		FGS_DamageEvent DamageEvent;
		DamageEvent.HitReactType = EHitReactType::Interrupt;
		Damaged->TakeDamage(Damage, DamageEvent, OwnerChar->GetController(), OwnerChar);

		// Reaction Sync: 피격자에게도 짧은 히트스탑 적용 (멀티플레이 밸런스 조정: 0.14 -> 0.06)
		Damaged->Multicast_ApplyHitStop(0.06f, 0.0f, false);

		// 한 번의 공격에 한 명의 적만 맞도록 히트박스 비활성화 (다음 프레임에 안전하게)
		SafeDisableHitBoxCollision(HitBox);
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

	if (SoundEventToPlay && GetWorld())
	{
		FVector ListenerLocation;
		if (GetListenerLocation(ListenerLocation))
		{
			// RTS 모드와 TPS 모드에 따른 거리 체크
			const bool bRTS = IsRTSMode();
			const float MaxDistance = bRTS ? 10000.0f : 2000.0f; // RTS: 100m, TPS: 20m

			const float DistanceToListener = FVector::Dist(SweepResult.ImpactPoint, ListenerLocation);

			if (DistanceToListener <= MaxDistance)
			{
				UAkGameplayStatics::PostEventAtLocation(
				    SoundEventToPlay,
				    SweepResult.ImpactPoint,
				    FRotator::ZeroRotator,
				    GetWorld());
			}
		}
		else
		{
			// Fallback: 리스너 위치를 찾지 못할 경우 거리 체크 없이 재생
			UAkGameplayStatics::PostEventAtLocation(
			    SoundEventToPlay,
			    SweepResult.ImpactPoint,
			    FRotator::ZeroRotator,
			    GetWorld());
		}
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

	// 서버가 호출한 타격 이펙트는 거리 상관없이 무조건 재생 (필터링 완화)
	PlayHitVFX(TargetType, SweepResult);
}

void AGS_WeaponSword::Multicast_PlaySpecialHitVFX_Implementation(UNiagaraSystem* VFXToPlay, const FHitResult& HitResult)
{
	// 레벨 전환 시 null 참조 방지
	if (!IsValidForLevelTransition())
	{
		return;
	}

	if (AGS_Character* Character = Cast<AGS_Character>(GetOwner()))
	{
		if (Character->ShouldPlayVFXAtLocation(HitResult.ImpactPoint, 4000.0f))
		{
			if (VFXToPlay && GetWorld())
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				    GetWorld(),
				    VFXToPlay,
				    HitResult.ImpactPoint,
				    HitResult.ImpactNormal.Rotation(),
				    FVector(1.0f),
				    true,
				    true);
			}
		}
	}
}

// 히트 포인트 계산 구현
FHitResult AGS_WeaponSword::CalculateMoreAccurateHitPoint(AActor* OtherActor) const
{
	FHitResult ResultHit;

	if (!OtherActor || !HitBox || !GetWorld())
	{
		// 기본값으로 무기 위치 사용
		ResultHit.ImpactPoint = GetActorLocation();
		ResultHit.Location = GetActorLocation();
		ResultHit.ImpactNormal = FVector::UpVector;
		ResultHit.Normal = FVector::UpVector;
		return ResultHit;
	}

	// HitBox의 월드 위치와 타겟의 위치 계산
	FVector HitBoxLocation = HitBox->GetComponentLocation();
	FVector TargetLocation = OtherActor->GetActorLocation();

	// HitBox에서 타겟으로의 방향 벡터
	FVector TraceDirection = (TargetLocation - HitBoxLocation).GetSafeNormal();

	// Line Trace 거리 (HitBox 크기의 2배 정도)
	float TraceDistance = FVector::Dist(HitBoxLocation, TargetLocation) + 100.0f;

	// Line Trace 시작점과 끝점
	FVector TraceStart = HitBoxLocation;
	FVector TraceEnd = HitBoxLocation + (TraceDirection * TraceDistance);

	// Line Trace 파라미터 설정
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this); // 무기 자체는 무시
	QueryParams.AddIgnoredActor(GetOwner()); // 무기 소유자도 무시
	QueryParams.bTraceComplex = false;

	// Line Trace 실행
	bool bHit = GetWorld()->LineTraceSingleByChannel(
	    ResultHit,
	    TraceStart,
	    TraceEnd,
	    ECC_Pawn, // Pawn 채널로 트레이스
	    QueryParams);

	if (bHit && ResultHit.GetActor() == OtherActor)
	{
		// 트레이스가 성공하고 올바른 타겟을 맞췄다면 해당 결과 사용
	}
	else
	{
		// 트레이스가 실패했다면 두 객체 간의 중점 계산
		FVector MidPoint = (HitBoxLocation + TargetLocation) * 0.5f;
		FVector ToTarget = (TargetLocation - HitBoxLocation).GetSafeNormal();

		ResultHit.ImpactPoint = MidPoint;
		ResultHit.Location = MidPoint;
		ResultHit.ImpactNormal = -ToTarget; // 타겟을 향하는 반대 방향
		ResultHit.Normal = -ToTarget;
	}

	return ResultHit;
}

bool AGS_WeaponSword::GetListenerLocation(FVector& OutLocation) const
{
	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!LocalPC)
	{
		return false;
	}

	if (AGS_RTSController* RTSController = Cast<AGS_RTSController>(LocalPC))
	{
		if (RTSController->GetViewTarget())
		{
			OutLocation = RTSController->GetViewTarget()->GetActorLocation();
			return true;
		}
	}
	else if (LocalPC->GetPawn())
	{
		OutLocation = LocalPC->GetPawn()->GetActorLocation();
		return true;
	}

	return false;
}

bool AGS_WeaponSword::IsRTSMode() const
{
	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	return LocalPC && Cast<AGS_RTSController>(LocalPC) != nullptr;
}

// 특화 헬퍼 함수 구현
FHitResult AGS_WeaponSword::CreateCorrectHitResult(const FHitResult& OriginalResult, bool bFromSweep) const
{
	if (!bFromSweep && IsOwnerCharValid() && OriginalResult.GetActor())
	{
		// Sword의 경우 더 정확한 히트 포인트 계산을 위해 기존 함수 활용
		return CalculateMoreAccurateHitPoint(OriginalResult.GetActor());
	}

	// 부모 클래스의 기본 구현 사용
	return Super::CreateCorrectHitResult(OriginalResult, bFromSweep);
}