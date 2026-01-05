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

FVector UGS_RTSSkill_FireballStrike::ActivateSkill(UGS_RTSSkillComponent* SkillComponent, const FVector& TargetLocation)
{
	Super::ActivateSkill(SkillComponent, TargetLocation);

	// 서버에서만 실행
	if (!SkillComponent || !SkillComponent->GetOwner()->HasAuthority())
	{
		return TargetLocation;
	}

	ShowWarningAndSpawnFireball(TargetLocation);

	return TargetLocation;
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

	// CastSound는 Base의 GetCastSound에서 이미 TPS/RTS 분기 처리됨
	if (UAkAudioEvent* CastSound = GetCastSound())
	{
		PlaySkillSound(CastSound, TargetLocation);
	}

	const float WarningDuration = FireballData->WarningDuration;
	const float EffectRadius = GetEffectRadius();

	// Soft Reference 로드
	UMaterialInterface* WarningDecalMaterial = FireballData->WarningDecalMaterial.IsNull() ? nullptr : FireballData->WarningDecalMaterial.LoadSynchronous();
	if (WarningDecalMaterial)
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

	// Soft Reference 로드
	TSubclassOf<AActor> LoadedProjectileClass = FireballData->ProjectileClass.IsNull() ? nullptr : FireballData->ProjectileClass.LoadSynchronous();
	if (LoadedProjectileClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Fireball = World->SpawnActor<AActor>(
			LoadedProjectileClass,
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
	}

	return;
	}

	// Soft Reference 로드
	UNiagaraSystem* LoadedTrailVFX = FireballData->TrailVFX.IsNull() ? nullptr : FireballData->TrailVFX.LoadSynchronous();
	if (LoadedTrailVFX)
	{
		PlaySkillVFX(LoadedTrailVFX, SpawnLocation);
	}

	// Soft Reference 로드
	UAkAudioEvent* FallSoundTPS = FireballData->FallSound_TPS.IsNull() ? nullptr : FireballData->FallSound_TPS.LoadSynchronous();
	UAkAudioEvent* FallSoundRTS = FireballData->FallSound_RTS.IsNull() ? nullptr : FireballData->FallSound_RTS.LoadSynchronous();
	UAkAudioEvent* FallSound = SelectSoundEvent(FallSoundTPS, FallSoundRTS);
	if (FallSound)
	{
		PlaySkillSound(FallSound, TargetLocation);
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

			// Soft Reference 로드
			UNiagaraSystem* LoadedExplosionVFX = FireballDataInner->ExplosionVFX.IsNull() ? nullptr : FireballDataInner->ExplosionVFX.LoadSynchronous();
			if (LoadedExplosionVFX)
			{
				WeakThis->PlaySkillVFX(LoadedExplosionVFX, TargetLocation);
			}

			// Soft Reference 로드
			UAkAudioEvent* ExpSoundTPS = FireballDataInner->ExplosionSound_TPS.IsNull() ? nullptr : FireballDataInner->ExplosionSound_TPS.LoadSynchronous();
			UAkAudioEvent* ExpSoundRTS = FireballDataInner->ExplosionSound_RTS.IsNull() ? nullptr : FireballDataInner->ExplosionSound_RTS.LoadSynchronous();
			UAkAudioEvent* ExpSound = WeakThis->SelectSoundEvent(ExpSoundTPS, ExpSoundRTS);
			if (ExpSound)
			{
				WeakThis->PlaySkillSound(ExpSound, TargetLocation);
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

							HitCharacter->TakeDamage(FinalDamage, DamageEvent, nullptr, nullptr);
						}
					}
				}
			}
		},
		FallTime,
		false
	);
}

const UGS_RTSSkillData_Fireball* UGS_RTSSkill_FireballStrike::GetFireballData() const
{
	return Cast<UGS_RTSSkillData_Fireball>(GetSkillData());
}
