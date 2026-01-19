// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Character/Player/Seeker/GS_Chan.h"
#include "Character/Component/Seeker/GS_ChanSkillInputHandlerComp.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Character/Skill/Seeker/Chan/GS_ChanUltimateSkill.h"
#include "Weapon/Equipable/GS_WeaponAxe.h"
#include "Weapon/Equipable/GS_WeaponShield.h"
#include "Sound/GS_SeekerAudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "UI/Character/GS_ChanAimingSkillBar.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"

AGS_Chan::AGS_Chan()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	CharacterType = ECharacterType::Chan;

	// Create defensive skill handler and overlap collision
	SkillInputHandlerComponent = CreateDefaultSubobject<UGS_ChanSkillInputHandlerComp>(TEXT("SkillInputHandlerComp"));
	UltimateCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("UltimateCollision"));
	UltimateCollision->SetupAttachment(GetRootComponent());

	// Default target magnetism (short range, wide angle for axe swings)
	MagnetismDistance = 350.0f;
	MagnetismAngle = 75.0f;

	ManualRowName = FName("Chan");
}

void AGS_Chan::BeginPlay()
{
	Super::BeginPlay();

	SetReplicateMovement(true);
	if (GetMesh())
		GetMesh()->SetIsReplicated(true);

	// Setup critical/ultimate skill overlap handling
	UltimateCollision->OnComponentBeginOverlap.AddDynamic(this, &AGS_Chan::HandleUltimateOverlap);

	CurrentStamina = MaxStamina;
	if (UGS_StatComp* StatComponent = GetStatComp())
	{
		BaseMaxHealth = StatComponent->GetMaxHealth();
	}
}

void AGS_Chan::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StaminaCycleTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AGS_Chan::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGS_Chan::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGS_Chan, bIsDefending);
	DOREPLIFETIME(AGS_Chan, CurrentStamina);
	DOREPLIFETIME(AGS_Chan, MaxStamina);
}

void AGS_Chan::RestoreStaminaFully()
{
	CurrentStamina = MaxStamina;
	Client_UpdateSkillBarProgress(1.0f);
}

void AGS_Chan::AdjustStamina(float NewValue, bool bTriggeredByDamage)
{
	CurrentStamina = FMath::Clamp(NewValue, 0.0f, MaxStamina);
	Client_UpdateSkillBarProgress(CurrentStamina / MaxStamina);

	// Fire events if stamina is fully depleted
	if (HasAuthority() && CurrentStamina <= SMALL_NUMBER)
	{
		OnStaminaDepleted.Broadcast(bTriggeredByDamage);
		// Force deactivate defensive/aiming skills on depletion
		if (UGS_SkillComp* SkillComponent = GetSkillComp())
		{
			SkillComponent->Server_TryDeactiveSkill(ESkillSlot::Ready);
		}
	}
}

void AGS_Chan::ProcessStaminaDrain()
{
	AdjustStamina(CurrentStamina - (StaminaDrainPerTick * 0.1f), false);
}

void AGS_Chan::ProcessStaminaRegen()
{
	AdjustStamina(CurrentStamina + (StaminaRegenPerTick * 0.1f), false);
	if (CurrentStamina >= MaxStamina)
	{
		GetWorldTimerManager().ClearTimer(StaminaCycleTimerHandle);
	}
}

void AGS_Chan::HandleUltimateOverlap(UPrimitiveComponent* OverlappedComp,
									 AActor* OtherActor,
									 UPrimitiveComponent* OtherComp,
									 int32 OtherBodyIndex,
									 bool bFromSweep,
									 const FHitResult& SweepResult)
{
	if (SkillComp)
	{
		if (UGS_ChanUltimateSkill* UltimateSkill =
				Cast<UGS_ChanUltimateSkill>(SkillComp->GetSkillFromSkillMap(ESkillSlot::Ultimate)))
		{
			UltimateSkill->HandleUltimateCollision(OtherActor, OtherComp, SweepResult);
		}
	}
}

void AGS_Chan::MulticastPlayComboSection_Implementation(int32 ComboIndex)
{
	Super::MulticastPlayComboSection_Implementation(ComboIndex);

	if (SeekerAudioComponent)
	{
		SeekerAudioComponent->PlayChanComboAttackSound(ComboIndex + 1);
	}
}

void AGS_Chan::Multicast_HandleAttackHitEffects_Implementation(int32 ComboIndex)
{
	// Trigger specialized audio for the 4th combo hit (axe slam)
	if (ComboIndex == 4 && SeekerAudioComponent)
	{
		SeekerAudioComponent->PlayChanFinalAttackSound();
	}

	// Calculate hit-stop duration: heavier finishers result in longer pauses
	float BasePause = 0.05f;
	if (ComboIndex == 4)
	{
		BasePause = 0.09f;
	}
	else
	{
		// Progressive pausing intensity
		BasePause *= (1.0f + (FMath::Clamp(ComboIndex - 1, 0, 2) * 0.1f));
	}

	Multicast_ApplyHitStop(BasePause, 0.0f, true);

	// Locally apply screen shake on the attacker's client
	if (HasAuthority())
	{
		if (APlayerController* AttackerPC = Cast<APlayerController>(GetController()))
		{
			if (ComboIndex == 4)
			{
				FGS_CameraShakeInfo HeavyShake = AttackSuccessShake;
				HeavyShake.Intensity *= 1.5f;
				Client_PlayAttackSuccessShakeWithInfo(AttackerPC, HeavyShake);
			}
			else
			{
				Client_PlayAttackSuccessShake(AttackerPC);
			}
		}
	}
}

void AGS_Chan::OnAttackHitSuccess(int32 ComboIndex, const FHitResult& HitResult)
{
	// Hits 3 (Shield) and 4 (Axe Slam) generate special visual and physical feedback
	if (ComboIndex == 3 || ComboIndex == 4)
	{
		Multicast_HandleAttackHitEffects(ComboIndex);

		if (ComboIndex == 4 && FinisherHitVFX)
		{
			if (AGS_Weapon* ActiveWeapon = GetWeaponByIndex(0))
			{
				ActiveWeapon->Multicast_PlaySpecialHitVFX(FinisherHitVFX, HitResult);
			}
		}
	}
}

void AGS_Chan::HandleJumpAttackSkillStart()
{
	Multicast_SetMontageSlot(ESeekerMontageSlot::FullBody);
}

void AGS_Chan::HandleJumpAttackSkillEnd()
{
	Multicast_SetMontageSlot(ESeekerMontageSlot::None);
	StopAnimMontage();
}

void AGS_Chan::TransitionToIdle()
{
	Multicast_StopSkillMontage(GetCurrentMontage());
	Multicast_SetMontageSlot(ESeekerMontageSlot::None);
	SetMoveControlValue(true, true);
	SetLookControlValue(true, true);
}

float AGS_Chan::TakeDamage(float DamageAmount,
						   struct FDamageEvent const& DamageEvent,
						   class AController* EventInstigator,
						   AActor* DamageCauser)
{
	// If currently blocking, mitigate damage and drain stamina instead of playing hit reactions
	if (bIsDefending)
	{
		// Convert health damage to stamina damage
		if (BaseMaxHealth > SMALL_NUMBER)
		{
			float StaminaCost = DamageAmount * (MaxStamina / BaseMaxHealth);
			AdjustStamina(CurrentStamina - StaminaCost, true);
		}

		// Visually trigger defense effects on the shield
		for (int32 i = 0; i < 5; ++i)
		{
			if (AGS_WeaponShield* Shield = Cast<AGS_WeaponShield>(GetWeaponByIndex(i)))
			{
				FHitResult BlockHit;
				BlockHit.ImpactPoint = Shield->GetActorLocation() + (Shield->GetActorForwardVector() * 50.0f);
				BlockHit.ImpactNormal = -Shield->GetActorForwardVector();
				Shield->PlayDefenseEffects(DamageCauser, BlockHit);
				break;
			}
		}

		// Full absorption: no HP damage is taken while blocking
		return 0.0f;
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AGS_Chan::SetDefending(bool bEnabled)
{
	if (!HasAuthority() || bIsDefending == bEnabled)
	{
		return;
	}

	bIsDefending = bEnabled;
	GetWorldTimerManager().ClearTimer(StaminaCycleTimerHandle);

	// Sycnronize shield collision states
	for (int32 i = 0; i < 5; ++i)
	{
		if (AGS_WeaponShield* Shield = Cast<AGS_WeaponShield>(GetWeaponByIndex(i)))
		{
			if (bIsDefending)
			{
				Shield->ServerEnableDefenseHit();
				// Start periodic stamina drain while shield is up
				GetWorldTimerManager().SetTimer(
					StaminaCycleTimerHandle, this, &AGS_Chan::ProcessStaminaDrain, 0.05f, true);
			}
			else
			{
				Shield->ServerDisableDefenseHit();
				// Switch to stamina regeneration mode
				GetWorldTimerManager().SetTimer(
					StaminaCycleTimerHandle, this, &AGS_Chan::ProcessStaminaRegen, 0.05f, true);
			}
			break;
		}
	}
}

void AGS_Chan::OnRep_IsDefending()
{
	// Additional UI/Visual logic when defense state replicates to clients can be placed here
}

bool AGS_Chan::ValidateBlockDetection(const FVector& ImpactLocation) const
{
	// Find the shield and check if the impact is within its effective protection arc
	for (int32 i = 0; i < 5; ++i)
	{
		if (AGS_WeaponShield* Shield = Cast<AGS_WeaponShield>(GetWeaponByIndex(i)))
		{
			if (Shield && Shield->DefenseHitBox)
			{
				const FVector ToImpact = (ImpactLocation - Shield->GetActorLocation()).GetSafeNormal();
				const float Alignment = FVector::DotProduct(Shield->GetActorForwardVector(), ToImpact);

				// Accept blocks within a ~120 degree frontal arc
				const float BlockArcThreshold = FMath::Cos(FMath::DegreesToRadians(60.0f));
				return (Alignment >= BlockArcThreshold);
			}
			break;
		}
	}
	return false;
}

void AGS_Chan::Client_UpdateSkillBarProgress_Implementation(float NormalizedValue)
{
	if (LinkedSkillBarWidget)
		LinkedSkillBarWidget->SetAimingProgress(NormalizedValue);
}

void AGS_Chan::Client_UpdateSkillBarDamageFlash_Implementation(float NormalizedValue)
{
	if (LinkedSkillBarWidget)
		LinkedSkillBarWidget->SetAimingProgressByDamage(NormalizedValue);
}

void AGS_Chan::Client_SetSkillBarVisibility_Implementation(bool bIsVisible)
{
	if (LinkedSkillBarWidget)
		LinkedSkillBarWidget->ShowSkillBar(bIsVisible);
}

void AGS_Chan::Multicast_DrawSkillRange_Implementation(FVector Center, float Radius, FColor Color, float Duration)
{
	// Debug visualization for skill range - draws a circle on the ground
	if (GetWorld())
	{
		DrawDebugCircle(GetWorld(), Center, Radius, 32, Color, false, Duration, 0, 2.0f, FVector(0, 1, 0), FVector(1, 0, 0));
	}
}

void AGS_Chan::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// Chan-specific input setup can be added here if needed
}
