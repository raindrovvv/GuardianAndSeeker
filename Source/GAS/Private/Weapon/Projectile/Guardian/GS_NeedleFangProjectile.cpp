// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Projectile/Guardian/GS_NeedleFangProjectile.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/GS_Character.h"
#include "Engine/DamageEvents.h"
#include "AkGameplayStatics.h"
#include "AkAudioEvent.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Character/F_GS_DamageEvent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/SphereComponent.h"
#include "Weapon/Equipable/GS_WeaponShield.h"
#include "VFX/GS_VFX_FunctionLibrary.h"
#include "GameFramework/ProjectileMovementComponent.h"

AGS_NeedleFangProjectile::AGS_NeedleFangProjectile()
{
	ProjectileLifeTime = 1.5f;
}

void AGS_NeedleFangProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->Activate(true);
	}

	// === 투사체 콜리전에 방어 가능 태그 추가 ===
	if (CollisionComponent)
	{
		CollisionComponent->ComponentTags.AddUnique(FName("DEFENSIBLE_ATTACK"));

		// 카메라 채널 무시
		CollisionComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

		// 기존 Overlap 설정 (유지)
		CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AGS_NeedleFangProjectile::OnBeginOverlap);
		CollisionComponent->SetGenerateOverlapEvents(true);
	}

	// 기존 타이머 설정 (유지)
	GetWorld()->GetTimerManager().SetTimer(DestroyTimerHandle, this, &AGS_NeedleFangProjectile::HandleProjectileDestroy, ProjectileLifeTime, false);
}

void AGS_NeedleFangProjectile::OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 이미 히트했거나 자기 자신/발사자라면 무시
	if (bHasHit || !OtherActor || OtherActor == this || OtherActor == GetOwner())
	{
		return;
	}

	AGS_Character* DamagedCharacter = Cast<AGS_Character>(OtherActor);
	// 캐릭터가 맞은 경우, 메시나 캡슐이 아니면(즉, 보이지 않는 트리거라면) 무시하고 통과
	if (DamagedCharacter)
	{
		if (OtherComp != DamagedCharacter->GetMesh() && OtherComp != (UPrimitiveComponent*)DamagedCharacter->GetRootComponent())
		{
			// 플레이어의 트리거 영역에 닿은 것이므로 무시하고 비행 유지
			return;
		}
	}

	// 여기까지 왔다면 "진짜" 히트
	bHasHit = true;

	// Chan의 방패인지 확인 (캐릭터가 직접 맞지 않은 경우)
	if (!DamagedCharacter)
	{
		if (AGS_WeaponShield* ChanShield = Cast<AGS_WeaponShield>(OtherActor))
		{
			DamagedCharacter = ChanShield->GetOwnerChar();
		}
	}

	AGS_Character* OwnerCharacter = Cast<AGS_Character>(GetOwner());

	FVector ImpactPoint = SweepResult.ImpactPoint;
	FVector ImpactNormal = SweepResult.ImpactNormal;

	// 즉시 충돌 등으로 임팩트 포인트가 무효한 경우 보정 (상대방 위치 기반)
	if (ImpactPoint.IsZero() && OtherActor)
	{
		ImpactPoint = OtherActor->GetActorLocation() + FVector(0, 0, 50); // 허리 높이 보정
		ImpactNormal = (GetActorLocation() - OtherActor->GetActorLocation()).GetSafeNormal();
	}

	// [추가] 환경 히트 (벽, 바닥 등)에도 레이어드 사운드 재생
	if (HasAuthority())
	{
		Multicast_PlayLayeredHitSound(SweepResult, OtherActor);
	}

	if (DamagedCharacter && OwnerCharacter && DamagedCharacter->IsEnemy(OwnerCharacter) && DamagedCharacter->GetStatComp())
	{
		Multicast_PlayHitSound(ImpactPoint);

		UGS_StatComp* DamagedStat = DamagedCharacter->GetStatComp();
		float Damage = DamagedStat->CalculateDamage(OwnerCharacter, DamagedCharacter);
		FGS_DamageEvent DamageEvent;
		DamageEvent.HitReactType = EHitReactType::DamageOnly;
		DamageEvent.bSuppressCameraEffects = true; // 작은 투사체에 의한 카메라 어지러움 방지

		float ActualDamage = DamagedCharacter->TakeDamage(Damage, DamageEvent, GetOwner()->GetInstigatorController(), this);

		// 실제로 데미지가 적용된 경우에만 혈흔 이펙트 재생
		if (ActualDamage > 0.0f)
		{
			Multicast_PlayBloodEffect(ImpactPoint, ImpactNormal);
		}
	}

	// 소프트 파괴 처리 (시각 효과 보장)
	if (HasAuthority())
	{
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		if (ProjectileMovementComponent)
		{
			ProjectileMovementComponent->StopMovementImmediately();
			ProjectileMovementComponent->Deactivate();
		}
		SetLifeSpan(0.2f);
	}
}

void AGS_NeedleFangProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                     FVector NormalImpulse, const FHitResult& Hit)
{
}

void AGS_NeedleFangProjectile::HandleProjectileDestroy()
{
	Destroy();
}

void AGS_NeedleFangProjectile::Multicast_PlayHitSound_Implementation(FVector HitLocation)
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (HitSoundEvent)
	{
		// 사운드 거리 기반 컬링 (60m 전역 상수 사용)
		if (GetWorld())
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				FVector CameraLoc;
				FRotator CameraRot;
				PC->GetPlayerViewPoint(CameraLoc, CameraRot);
				float DistSq = FVector::DistSquared(CameraLoc, HitLocation);

				if (DistSq > FMath::Square(GS_Rendering::VFX_DISABLE_DISTANCE))
				{
					return;
				}
			}
		}

		UAkGameplayStatics::PostEventAtLocation(
		    HitSoundEvent,
		    HitLocation,
		    FRotator::ZeroRotator,
		    GetWorld());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("NeedleFang HitSoundEvent is null"));
	}
}

void AGS_NeedleFangProjectile::Multicast_PlayBloodEffect_Implementation(FVector HitLocation, FVector HitNormal)
{
	if (AGS_Character* OwnerChar = Cast<AGS_Character>(GetOwner()))
	{
		// 거리 기반 VFX 컬링 (60m 전역 상수 사용)
		if (OwnerChar->ShouldPlayVFXAtLocation(HitLocation, GS_Rendering::VFX_DISABLE_DISTANCE))
		{
			UGS_VFX_FunctionLibrary::PlayBloodEffect(this, BloodEffectSystem, HitLocation, FRotationMatrix::MakeFromZ(HitNormal).Rotator());
		}
	}
}