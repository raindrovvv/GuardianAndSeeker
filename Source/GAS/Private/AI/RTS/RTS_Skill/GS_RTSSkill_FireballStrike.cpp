// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/RTS/RTS_Skill/GS_RTSSkill_FireballStrike.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillComponent.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillData.h"
#include "Character/GS_Character.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Components/DecalComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"
#include "Engine/DamageEvents.h"

UGS_RTSSkill_FireballStrike::UGS_RTSSkill_FireballStrike()
{
	PendingTargetLocation = FVector::ZeroVector;
}

void UGS_RTSSkill_FireballStrike::ActivateSkill(UGS_RTSSkillComponent* SkillComponent, const FVector& TargetLocation)
{
	Super::ActivateSkill(SkillComponent, TargetLocation);

	// 서버에서만 실행
	if (!SkillComponent || !SkillComponent->GetOwner()->HasAuthority())
	{
		return;
	}

	ShowWarningAndSpawnFireball(TargetLocation);
}

void UGS_RTSSkill_FireballStrike::ShowWarningAndSpawnFireball(const FVector& TargetLocation)
{
	const UGS_RTSSkillData_Fireball* FireballData = GetFireballData();
	if (!FireballData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_FireballStrike: Missing fireball data asset"));
		return;
	}

	UWorld* World = GetSkillWorld();
	if (!World)
	{
		return;
	}

	if (USoundBase* CastSound = GetCastSound())
	{
		PlaySkillSound(CastSound, TargetLocation);
	}

	const float WarningDuration = FireballData->WarningDuration;
	const float EffectRadius = GetEffectRadius();

	if (UMaterialInterface* WarningDecalMaterial = FireballData->WarningDecalMaterial)
	{
		FVector DecalLocation = TargetLocation;
		DecalLocation.Z += 5.f;

		UDecalComponent* WarningDecal = UGameplayStatics::SpawnDecalAtLocation(
			World,
			WarningDecalMaterial,
			FVector(EffectRadius, EffectRadius, 100.f),
			DecalLocation,
			FRotator(-90.f, 0.f, 0.f),
			WarningDuration + 0.5f
		);

		if (WarningDecal)
		{
			WarningDecal->SetFadeScreenSize(0.f);
		}
	}

	PendingTargetLocation = TargetLocation;

	if (WarningDuration <= 0.f)
	{
		SpawnFireball();
	}
	else
	{
		World->GetTimerManager().SetTimer(
			SpawnFireballTimerHandle,
			this,
			&UGS_RTSSkill_FireballStrike::SpawnFireball,
			WarningDuration,
			false
		);
	}

	UE_LOG(LogTemp, Log, TEXT("UGS_RTSSkill_FireballStrike: Warning shown, fireball will spawn in %.1f seconds"), WarningDuration);
}

void UGS_RTSSkill_FireballStrike::SpawnFireball()
{
	const UGS_RTSSkillData_Fireball* FireballData = GetFireballData();
	if (!FireballData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkill_FireballStrike: Cannot spawn fireball without valid data asset"));
		return;
	}

	UWorld* World = GetSkillWorld();
	if (!World)
	{
		return;
	}

	const FVector TargetLocation = PendingTargetLocation;
	const float FallStartHeight = FireballData->FallStartHeight;
	const float FallSpeed = FireballData->FallSpeed;

	const FVector SpawnLocation = TargetLocation + FVector(0.f, 0.f, FallStartHeight);
	const FRotator SpawnRotation = FRotator(-90.f, 0.f, 0.f);

	if (FireballData->ProjectileClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Fireball = World->SpawnActor<AActor>(
			FireballData->ProjectileClass,
			SpawnLocation,
			SpawnRotation,
			SpawnParams
		);

		if (Fireball)
		{
			if (UProjectileMovementComponent* ProjectileMovement = Fireball->FindComponentByClass<UProjectileMovementComponent>())
			{
				ProjectileMovement->InitialSpeed = FallSpeed;
				ProjectileMovement->MaxSpeed = FallSpeed;
				ProjectileMovement->Velocity = FVector(0.f, 0.f, -FallSpeed);
			}

			UE_LOG(LogTemp, Log, TEXT("UGS_RTSSkill_FireballStrike: Spawned fireball projectile at %s"), *SpawnLocation.ToString());
		}

		return;
	}

	if (FireballData->TrailVFX)
	{
		PlaySkillVFX(FireballData->TrailVFX, SpawnLocation);
	}

	if (FireballData->FallSound)
	{
		PlaySkillSound(FireballData->FallSound, TargetLocation);
	}

	const float FallTime = (!FMath::IsNearlyZero(FallSpeed)) ? FallStartHeight / FallSpeed : 0.f;

	FTimerHandle ExplosionTimerHandle;
	TWeakObjectPtr<UGS_RTSSkill_FireballStrike> WeakThis = this;
	World->GetTimerManager().SetTimer(
		ExplosionTimerHandle,
		[WeakThis, TargetLocation]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			const UGS_RTSSkillData_Fireball* FireballDataInner = WeakThis->GetFireballData();
			if (!FireballDataInner)
			{
				return;
			}

			UWorld* WorldContext = WeakThis->GetSkillWorld();
			if (!WorldContext)
			{
				return;
			}

			if (FireballDataInner->ExplosionVFX)
			{
				WeakThis->PlaySkillVFX(FireballDataInner->ExplosionVFX, TargetLocation);
			}

			if (FireballDataInner->ExplosionSound)
			{
				WeakThis->PlaySkillSound(FireballDataInner->ExplosionSound, TargetLocation);
			}

			TArray<FOverlapResult> OverlapResults;
			FCollisionShape CollisionShape = FCollisionShape::MakeSphere(WeakThis->GetEffectRadius());
			FCollisionQueryParams QueryParams;

			if (WorldContext->OverlapMultiByChannel(
				OverlapResults,
				TargetLocation,
				FQuat::Identity,
				ECC_Pawn,
				CollisionShape,
				QueryParams))
			{
				const float EffectRadiusInner = WeakThis->GetEffectRadius();
				const float SkillPowerInner = WeakThis->GetSkillPower();

				for (const FOverlapResult& Result : OverlapResults)
				{
					if (AGS_Character* HitCharacter = Cast<AGS_Character>(Result.GetActor()))
					{
						if (HitCharacter->IsA(AGS_Seeker::StaticClass()))
						{
							const float Distance = FVector::Dist(HitCharacter->GetActorLocation(), TargetLocation);
							const float DamageMultiplier = (EffectRadiusInner > KINDA_SMALL_NUMBER)
								? 1.f - FMath::Clamp(Distance / EffectRadiusInner, 0.f, 1.f)
								: 1.f;
							const float FinalDamage = SkillPowerInner * DamageMultiplier;

							FDamageEvent DamageEvent;
							HitCharacter->TakeDamage(FinalDamage, DamageEvent, nullptr, nullptr);

							UE_LOG(LogTemp, Log, TEXT("Fireball hit %s for %.1f damage"),
								*HitCharacter->GetName(), FinalDamage);
						}
					}
				}
			}
		},
		FallTime,
		false
	);

	UE_LOG(LogTemp, Log, TEXT("UGS_RTSSkill_FireballStrike: Direct explosion scheduled at %s in %.2f seconds"),
		*TargetLocation.ToString(), FallTime);
}

const UGS_RTSSkillData_Fireball* UGS_RTSSkill_FireballStrike::GetFireballData() const
{
	return Cast<UGS_RTSSkillData_Fireball>(GetSkillData());
}
