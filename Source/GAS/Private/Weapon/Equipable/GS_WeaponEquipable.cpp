// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Equipable/GS_WeaponEquipable.h"
#include "Weapon/Component/GS_WeaponVFXComponent.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Character/Player/Seeker/GS_Ares.h"
#include "Character/Player/Seeker/GS_Merci.h"
#include "Character/Player/Guardian/GS_Guardian.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "AI/RTS/GS_RTSController.h"
#include "Rendering/GS_RenderingConstants.h"
#include "AkGameplayStatics.h"

AGS_WeaponEquipable::AGS_WeaponEquipable()
{
	OwnerChar = nullptr;
	bReplicates = true;

	// 무기 VFX 컴포넌트 생성
	WeaponVFXComponent = CreateDefaultSubobject<UGS_WeaponVFXComponent>(TEXT("WeaponVFXComponent"));
}

void AGS_WeaponEquipable::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Set Server option
	SetReplicateMovement(true); // Replicate Actor Rotation & Transition
}

void AGS_WeaponEquipable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGS_WeaponEquipable, ActiveNotifyCount);
}

// 헬퍼 함수 구현
bool AGS_WeaponEquipable::IsOwnerCharValid() const
{
	return OwnerChar != nullptr && IsValid(OwnerChar) && !OwnerChar->IsActorBeingDestroyed();
}

void AGS_WeaponEquipable::ClearHitActors()
{
	HitActors.Empty();
}

void AGS_WeaponEquipable::ClearSafetyTimer()
{
	// 안전한 타이머 정리 - 레벨 전환 시에도 안전하게 처리
	if (UWorld* World = GetWorld(); World && IsValid(World) && !World->bIsTearingDown)
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (&TimerManager && SafetyTimerHandle.IsValid())
		{
			TimerManager.ClearTimer(SafetyTimerHandle);
		}
	}

	// 타이머 핸들 무효화
	SafetyTimerHandle.Invalidate();
}

bool AGS_WeaponEquipable::IsInFrontAngle(AActor* TargetActor, float AngleDegrees) const
{
	if (!OwnerChar || !TargetActor)
	{
		return true; // 안전하게 허용 (판단을 건너뜀)
	}

	// 1. 공격자에서 타겟으로의 방향 (Z값 무시하여 수평 평면상으로 계산)
	FVector CharacterLocation = OwnerChar->GetActorLocation();
	FVector TargetLocation = TargetActor->GetActorLocation();

	FVector DirectionToTarget = (TargetLocation - CharacterLocation);
	DirectionToTarget.Z = 0.0f;
	DirectionToTarget = DirectionToTarget.GetSafeNormal();

	// 2. 공격자의 앞방향
	FVector Forward = OwnerChar->GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();

	// 3. 내적(Dot Product)을 통해 각도 확인
	float Dot = FVector::DotProduct(Forward, DirectionToTarget);

	// 각도 임계값 계산 (cos(각도/2))
	float HalfAngleRadians = FMath::DegreesToRadians(AngleDegrees * 0.5f);
	float Threshold = FMath::Cos(HalfAngleRadians);

	return Dot >= Threshold;
}

FHitResult AGS_WeaponEquipable::CreateCorrectHitResult(const FHitResult& OriginalResult, bool bFromSweep) const
{
	if (!bFromSweep && OriginalResult.GetActor())
	{
		// Overlap 상황에서 더 정확한 히트 포인트를 계산하여 반환
		return CalculateMoreAccurateHitPoint(OriginalResult.GetActor());
	}

	FHitResult CorrectHitResult = OriginalResult;
	if (!bFromSweep)
	{
		CorrectHitResult.ImpactPoint = GetActorLocation();
		CorrectHitResult.Location = GetActorLocation();
		CorrectHitResult.ImpactNormal = FVector::UpVector;
		CorrectHitResult.Normal = FVector::UpVector;
	}
	return CorrectHitResult;
}

FHitResult AGS_WeaponEquipable::CalculateMoreAccurateHitPoint(AActor* OtherActor) const
{
	FHitResult ResultHit;
	UBoxComponent* HitBox = GetHitBox();

	if (!OtherActor || !HitBox || !GetWorld())
	{
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
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bTraceComplex = false;

	// Line Trace 실행 (벽이나 캐릭터 모두 감지할 수 있도록 전용 채널 활용)
	bool bHit = GetWorld()->LineTraceSingleByChannel(
	    ResultHit,
	    TraceStart,
	    TraceEnd,
	    COLLISION_WEAPON_HIT, // 타격 전용 채널 사용
	    QueryParams);

	if (bHit && ResultHit.GetActor() == OtherActor)
	{
		// 트레이스가 성공하고 올바른 타겟을 맞췄다면 해당 결과 사용
	}
	else
	{
		// 트레이스가 실패했다면 두 객체 간의 중점 계산 (Fallback)
		FVector MidPoint = (HitBoxLocation + TargetLocation) * 0.5f;
		FVector ToTarget = (TargetLocation - HitBoxLocation).GetSafeNormal();

		ResultHit.ImpactPoint = MidPoint;
		ResultHit.Location = MidPoint;
		ResultHit.ImpactNormal = -ToTarget;
		ResultHit.Normal = -ToTarget;
		ResultHit.HitObjectHandle = FActorInstanceHandle(OtherActor);
	}

	return ResultHit;
}

void AGS_WeaponEquipable::TriggerHitAuraOnHit(AGS_Character* HitTarget)
{
	if (!ShouldTriggerAuraOnHit(HitTarget) || !WeaponVFXComponent)
	{
		return;
	}

	// 시커 캐릭터 타입 확인
	ESeekerAuraType AuraType = GetSeekerAuraType(OwnerChar);
	if (AuraType != ESeekerAuraType::Default)
	{
		// 타격 시 아우라 활성화
		WeaponVFXComponent->ActivateHitAura(AuraType);
	}
}

ESeekerAuraType AGS_WeaponEquipable::GetSeekerAuraType(AGS_Character* SeekerChar) const
{
	if (!SeekerChar)
	{
		return ESeekerAuraType::Default;
	}

	// 시커 캐릭터 타입에 따른 아우라 타입 결정
	if (Cast<AGS_Chan>(SeekerChar))
	{
		return ESeekerAuraType::Chan;
	}
	else if (Cast<AGS_Ares>(SeekerChar))
	{
		return ESeekerAuraType::Ares;
	}
	else if (Cast<AGS_Merci>(SeekerChar))
	{
		return ESeekerAuraType::Merci;
	}

	return ESeekerAuraType::Default;
}

bool AGS_WeaponEquipable::ShouldTriggerAuraOnHit(AGS_Character* HitTarget) const
{
	// 기본 조건: 가디언이나 몬스터를 타격했을 때만 아우라 활성화
	return Cast<AGS_Guardian>(HitTarget) || Cast<AGS_Monster>(HitTarget);
}

void AGS_WeaponEquipable::SafeDisableHitBoxCollision(UBoxComponent* InHitBox)
{
	if (!InHitBox || !IsValid(InHitBox))
	{
		return;
	}

	// 기존 예약된 타이머가 있다면 정리
	ClearSafetyTimer();

	// 다음 프레임 근처에서 안전하게 콜리전 비활성화 (물리 쿼리 충돌 방지)
	// SetTimerForNextTick 대신 SafetyTimerHandle을 사용하여 중간에 취소 가능하게 함
	if (UWorld* World = GetWorld(); World && !World->bIsTearingDown)
	{
		TWeakObjectPtr<UBoxComponent> WeakHitBox = InHitBox;
		World->GetTimerManager().SetTimer(SafetyTimerHandle, [WeakHitBox]()
		                                  {
			if (WeakHitBox.IsValid() && IsValid(WeakHitBox.Get()) && !WeakHitBox->IsBeingDestroyed())
			{
				WeakHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			} }, 0.01f, false); // 아주 짧은 지연시간 후 비활성화
	}
}

void AGS_WeaponEquipable::ServerEnableHit_Implementation()
{
	ActiveNotifyCount++;

	// [중요] 중복 타격 방지: 공격 시퀀스가 처음 시작될 때만 히트 목록 초기화
	// 여러 노티파이가 겹쳐도 하나의 스윙 내에서는 동일 대상을 재타격하지 않도록 함
	if (ActiveNotifyCount == 1)
	{
		ClearHitActors();
	}
}

void AGS_WeaponEquipable::ServerDisableHit_Implementation()
{
	ActiveNotifyCount = FMath::Max(0, ActiveNotifyCount - 1);
}

void AGS_WeaponEquipable::ForceDisableHit()
{
	if (HasAuthority())
	{
		ServerForceDisableHit_Implementation();
	}
	else
	{
		ServerForceDisableHit();

		// 로컬에서도 즉시 비활성화 시도 (자식 클래스에서 오버라이드한 DisableHit 호출)
		DisableHit();
	}
}

void AGS_WeaponEquipable::ServerForceDisableHit_Implementation()
{
	// 카운트 강제 초기화
	ActiveNotifyCount = 0;

	// 자식 클래스의 비활성화 로직 호출 (ServerDisableHit_Implementation 호출)
	ServerDisableHit_Implementation();

	// 물리적 콜리전도 즉시 비활성화 시도 (GetHitBox 활용)
	if (UBoxComponent* HitBox = GetHitBox())
	{
		SafeDisableHitBoxCollision(HitBox);
	}
}
bool AGS_WeaponEquipable::GetListenerLocation(FVector& OutLocation) const
{
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		// 로컬 플레이어의 컨트롤러를 찾아야 함
		APlayerController* LocalPC = nullptr;
		for (auto It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->IsLocalController())
			{
				LocalPC = PC;
				break;
			}
		}

		if (!LocalPC)
		{
			LocalPC = World->GetFirstPlayerController();
		}

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
	}

	return false;
}

void AGS_WeaponEquipable::PlayHitSoundAtLocation(UAkAudioEvent* SoundEvent, const FVector& Location)
{
	if (!SoundEvent || !IsValid(GetWorld()))
	{
		return;
	}

	FVector ListenerLocation;
	if (GetListenerLocation(ListenerLocation))
	{
		// 자신이 조종하는 캐릭터일 때만 60m(VFX_DISABLE_DISTANCE) 적용, 아니면 40m로 제한
		float BaseDistance = (OwnerChar && OwnerChar->IsLocallyControlled()) ? GS_Rendering::VFX_DISABLE_DISTANCE : GS_Rendering::MONSTER_SMALL_CULL_DISTANCE;
		const float MaxDistance = GS_Rendering::CalculateCullDistance(this, BaseDistance);
		const float DistanceToListener = FVector::Dist(Location, ListenerLocation);

		if (DistanceToListener <= MaxDistance)
		{
			UAkGameplayStatics::PostEventAtLocation(
			    SoundEvent,
			    Location,
			    FRotator::ZeroRotator,
			    GetWorld());
		}
	}
	else
	{
		// Fallback: 리스너 위치를 찾지 못한 경우 로컬 플레이어만 재생 (비로컬은 무시)
		if (OwnerChar && OwnerChar->IsLocallyControlled())
		{
			UAkGameplayStatics::PostEventAtLocation(
			    SoundEvent,
			    Location,
			    FRotator::ZeroRotator,
			    GetWorld());
		}
	}
}
