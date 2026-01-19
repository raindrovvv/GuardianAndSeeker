// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Character/Player/Seeker/GS_Ares.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Character/Skill/Seeker/Ares/GS_AresMovingSkill.h"
#include "Character/Component/GS_StatComp.h"
#include "Sound/GS_SeekerAudioComponent.h"
#include "Weapon/GS_Weapon.h"
#include "GameFramework/PlayerController.h"

AGS_Ares::AGS_Ares()
{
	PrimaryActorTick.bCanEverTick = true;
	CharacterType = ECharacterType::Ares;

	// Default configuration for Ares
	ManualRowName = FName("Ares");

	// Heavy melee target magnetism (long range, narrow angle)
	MagnetismDistance = 500.0f;
	MagnetismAngle = 45.0f;
}

void AGS_Ares::BeginPlay()
{
	Super::BeginPlay();

	// Ensure movement and mesh are replicated for multiplayer consistency
	SetReplicateMovement(true);
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetIsReplicated(true);
	}

	// Propagate camera settings to the moving skill component if available
	if (SkillComp)
	{
		if (UGS_AresMovingSkill* MovingSkill =
				Cast<UGS_AresMovingSkill>(SkillComp->GetSkillFromSkillMap(ESkillSlot::Moving)))
		{
			MovingSkill->SetCameraSettings(DashZoomDistance,
										   DashZoomCurve,
										   bEnableDashMotionBlur,
										   DashMotionBlurIntensity,
										   DashMotionBlurCurve,
										   DashMotionBlurExponent);
		}
	}
}

void AGS_Ares::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGS_Ares::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AGS_Ares::ServerAttackMontage()
{
	Super::ServerAttackMontage();
}

void AGS_Ares::MulticastPlayComboSection_Implementation(int32 ComboIndex)
{
	Super::MulticastPlayComboSection_Implementation(ComboIndex);

	// Trigger audio feedback via the SeekerAudioComponent
	if (SeekerAudioComponent)
	{
		// Combo sounds are 1-based (AresCombo01, etc.)
		SeekerAudioComponent->PlayAresComboAttackSoundWithExtra(ComboIndex + 1);
	}
}

void AGS_Ares::Multicast_HandleAttackHitEffects_Implementation(int32 ComboIndex)
{
	// Calculate hit-stop duration based on combo progress for 'weighty' feel
	// Finisher hit (ComboIndex 4) has a longer, more impactful duration
	float StaggerDuration = (ComboIndex >= 4) ? 0.11f : 0.07f;

	if (ComboIndex < 4)
	{
		// Progressive scaling for intermediate hits
		const float ProgressionScale = 1.0f + (FMath::Clamp(ComboIndex - 1, 0, 2) * 0.1f);
		StaggerDuration *= ProgressionScale;
	}

	// Apply hit-stop locally to all clients for visual impact
	Multicast_ApplyHitStop(StaggerDuration, 0.0f, true);

	// Apply screen shake only to the local attacker
	if (HasAuthority())
	{
		if (APlayerController* AttackerPC = Cast<APlayerController>(GetController()))
		{
			if (ComboIndex >= 4)
			{
				// Heavy finisher shake
				FGS_CameraShakeInfo FinisherShakeInfo = AttackSuccessShake;
				FinisherShakeInfo.Intensity *= 1.6f;
				Client_PlayAttackSuccessShakeWithInfo(AttackerPC, FinisherShakeInfo);
			}
			else
			{
				// Standard attack shake
				Client_PlayAttackSuccessShake(AttackerPC);
			}
		}
	}
}

float AGS_Ares::TakeDamage(float DamageAmount,
						   struct FDamageEvent const& DamageEvent,
						   class AController* EventInstigator,
						   AActor* DamageCauser)
{
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AGS_Ares::Multicast_RestoreDashCameraZoom_Implementation()
{
	// Camera state restoration is only relevant for the local controlling client
	if (!IsLocallyControlled())
	{
		return;
	}

	if (UGS_SkillComp* SkillComponent = GetSkillComp())
	{
		if (UGS_AresMovingSkill* MovingSkill =
				Cast<UGS_AresMovingSkill>(SkillComponent->GetSkillFromSkillMap(ESkillSlot::Moving)))
		{
			MovingSkill->RestoreCameraZoom(true);
		}
	}
}

void AGS_Ares::OnAttackHitSuccess(int32 ComboIndex, const FHitResult& HitResult)
{
	// Final combo hit (Index 4) triggers additional special effects
	if (ComboIndex == 4)
	{
		// Trigger the multi-client hit effects (hit-stop, shakes)
		Multicast_HandleAttackHitEffects(ComboIndex);

		// Synchronize the special finisher VFX on the hit target's location
		if (FinisherHitVFX)
		{
			if (AGS_Weapon* ActiveWeapon = GetWeaponByIndex(0))
			{
				ActiveWeapon->Multicast_PlaySpecialHitVFX(FinisherHitVFX, HitResult);
			}
		}
	}
}
