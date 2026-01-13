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
#include "System/Utility/GS_AssetLoader.h"
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

	UMaterialInterface* WarningDecalMat = CachedWarningDecalMaterial;
	if (!WarningDecalMat && !FireballData->WarningDecalMaterial.IsNull())
	{
		WarningDecalMat = UGS_AssetLoader::SyncLoadAsset(FireballData->WarningDecalMaterial);
	}

	if (WarningDecalMat)
	{
		FVector DecalLocation = TargetLocation;
		DecalLocation.Z += 5.f;

		UDecalComponent* WarningDecal = UGameplayStatics::SpawnDecalAtLocation(
		    World,
		    WarningDecalMat,
		    FVector(EffectRadius, EffectRadius, 100.f),
		    DecalLocation,
		    FRotator(-90.f, 0.f, 0.f),
		    WarningDuration + 0.5f);

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
		    false);
	}
}

void UGS_RTSSkill_FireballStrike::PreloadAssets()
{
	Super::PreloadAssets();

	const UGS_RTSSkillData_Fireball* FireballData = GetFireballData();
	if (!FireballData)
		return;

	TArray<FSoftObjectPath> Paths;
	if (!FireballData->ProjectileClass.IsNull())
		Paths.Add(FireballData->ProjectileClass.ToSoftObjectPath());
	if (!FireballData->WarningDecalMaterial.IsNull())
		Paths.Add(FireballData->WarningDecalMaterial.ToSoftObjectPath());
	if (!FireballData->TrailVFX.IsNull())
		Paths.Add(FireballData->TrailVFX.ToSoftObjectPath());
	if (!FireballData->ExplosionVFX.IsNull())
		Paths.Add(FireballData->ExplosionVFX.ToSoftObjectPath());
	if (!FireballData->FallSound_TPS.IsNull())
		Paths.Add(FireballData->FallSound_TPS.ToSoftObjectPath());
	if (!FireballData->FallSound_RTS.IsNull())
		Paths.Add(FireballData->FallSound_RTS.ToSoftObjectPath());
	if (!FireballData->ExplosionSound_TPS.IsNull())
		Paths.Add(FireballData->ExplosionSound_TPS.ToSoftObjectPath());
	if (!FireballData->ExplosionSound_RTS.IsNull())
		Paths.Add(FireballData->ExplosionSound_RTS.ToSoftObjectPath());

	if (Paths.Num() > 0)
	{
		TWeakObjectPtr<UGS_RTSSkill_FireballStrike> WeakThis(this);
		UGS_AssetLoader::AsyncLoadMultipleAssets(Paths, [WeakThis]()
		                                         {
			if (UGS_RTSSkill_FireballStrike* StrongThis = WeakThis.Get())
			{
				const UGS_RTSSkillData_Fireball* FData = StrongThis->GetFireballData();
				if (FData)
				{
					StrongThis->CachedProjectileClass = FData->ProjectileClass.Get();
					StrongThis->CachedWarningDecalMaterial = FData->WarningDecalMaterial.Get();
					StrongThis->CachedTrailVFX = FData->TrailVFX.Get();
					StrongThis->CachedExplosionVFX = FData->ExplosionVFX.Get();
					StrongThis->CachedFallSound_TPS = FData->FallSound_TPS.Get();
					StrongThis->CachedFallSound_RTS = FData->FallSound_RTS.Get();
					StrongThis->CachedExplosionSound_TPS = FData->ExplosionSound_TPS.Get();
					StrongThis->CachedExplosionSound_RTS = FData->ExplosionSound_RTS.Get();
				}
			} });
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

	// Projectile Class
	TSubclassOf<AActor> ProjectileClass = CachedProjectileClass ? CachedProjectileClass->GetClass() : nullptr;
	if (!ProjectileClass && !FireballData->ProjectileClass.IsNull())
	{
		ProjectileClass = UGS_AssetLoader::SyncLoadAsset(FireballData->ProjectileClass);
	}

	if (ProjectileClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Fireball = World->SpawnActor<AActor>(
		    ProjectileClass,
		    SpawnLocation,
		    SpawnRotation,
		    SpawnParams);

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

	// Trail VFX
	UNiagaraSystem* TrailVFX = CachedTrailVFX;
	if (!TrailVFX && !FireballData->TrailVFX.IsNull())
	{
		TrailVFX = UGS_AssetLoader::SyncLoadAsset(FireballData->TrailVFX);
	}

	if (TrailVFX)
	{
		PlaySkillVFX(TrailVFX, SpawnLocation);
	}

	// Fall Sound
	UAkAudioEvent* FallSoundTPS = CachedFallSound_TPS;
	UAkAudioEvent* FallSoundRTS = CachedFallSound_RTS;
	if (!FallSoundTPS && !FireballData->FallSound_TPS.IsNull())
		FallSoundTPS = UGS_AssetLoader::SyncLoadAsset(FireballData->FallSound_TPS);
	if (!FallSoundRTS && !FireballData->FallSound_RTS.IsNull())
		FallSoundRTS = UGS_AssetLoader::SyncLoadAsset(FireballData->FallSound_RTS);

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

		    // Explosion VFX
		    UNiagaraSystem* ExplosionVFX = WeakThis->CachedExplosionVFX;
		    if (!ExplosionVFX && !FireballDataInner->ExplosionVFX.IsNull())
		    {
			    ExplosionVFX = UGS_AssetLoader::SyncLoadAsset(FireballDataInner->ExplosionVFX);
		    }

		    if (ExplosionVFX)
		    {
			    WeakThis->PlaySkillVFX(ExplosionVFX, TargetLocation);
		    }

		    // Explosion Sound
		    UAkAudioEvent* ExpSoundTPS = WeakThis->CachedExplosionSound_TPS;
		    UAkAudioEvent* ExpSoundRTS = WeakThis->CachedExplosionSound_RTS;
		    if (!ExpSoundTPS && !FireballDataInner->ExplosionSound_TPS.IsNull())
			    ExpSoundTPS = UGS_AssetLoader::SyncLoadAsset(FireballDataInner->ExplosionSound_TPS);
		    if (!ExpSoundRTS && !FireballDataInner->ExplosionSound_RTS.IsNull())
			    ExpSoundRTS = UGS_AssetLoader::SyncLoadAsset(FireballDataInner->ExplosionSound_RTS);

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
	    false);
}

const UGS_RTSSkillData_Fireball* UGS_RTSSkill_FireballStrike::GetFireballData() const
{
	return Cast<UGS_RTSSkillData_Fireball>(GetSkillData());
}
