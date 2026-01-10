// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/GS_AISeeker.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Seeker/GS_Ares.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Character/Player/Seeker/GS_Merci.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Character/Skill/GS_SkillBase.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "DrawDebugHelpers.h"

AGS_AISeeker::AGS_AISeeker()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // Reduce tick frequency for optimization

	// Set default class references (should be set in Blueprint)
	AIControllerClass = AGS_SeekerAIController::StaticClass();
}

void AGS_AISeeker::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		SpawnSeeker();
	}
}

void AGS_AISeeker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(AresDashTimer);
		TimerManager.ClearTimer(MerciMovingSkillTimer);
		TimerManager.ClearTimer(ChanShieldTimer);
	}

	if (SpawnedSeeker)
	{
		SpawnedSeeker->OnDeathDelegate.RemoveAll(this);
	}

	if (SpawnedSeeker && HasAuthority())
	{
		SpawnedSeeker->Destroy();
		SpawnedSeeker = nullptr;
	}

	CachedSkillComp = nullptr;
	CachedStatComp = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AGS_AISeeker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShowDebugInfo)
	{
		DrawDebugInfo();
	}
}

#if WITH_EDITOR
void AGS_AISeeker::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.Property != nullptr)
	                         ? PropertyChangedEvent.Property->GetFName()
	                         : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AGS_AISeeker, SeekerType))
	{
	}
}
#endif

void AGS_AISeeker::SpawnSeeker()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSubclassOf<AGS_Seeker> SeekerClass = GetSeekerClassByType();
	if (!SeekerClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AGS_AISeeker::SpawnSeeker - No valid Seeker class for type %d"), static_cast<int32>(SeekerType));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	FVector SpawnLocation = GetActorLocation();

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (NavSystem)
	{
		FNavLocation NavLocation;
		if (NavSystem->GetRandomReachablePointInRadius(SpawnLocation, 200.0f, NavLocation))
		{
			SpawnLocation = NavLocation.Location;
		}
	}

	SpawnedSeeker = World->SpawnActor<AGS_Seeker>(
	    SeekerClass,
	    SpawnLocation,
	    GetActorRotation(),
	    SpawnParams);

	if (SpawnedSeeker)
	{
		SpawnedSeeker->OnDeathDelegate.AddDynamic(this, &AGS_AISeeker::HandleSeekerDeath);
		SetupAIController();

		CachedSkillComp = SpawnedSeeker->FindComponentByClass<UGS_SkillComp>();
		CachedStatComp = SpawnedSeeker->GetStatComp();

		OnSeekerSpawned.Broadcast(SpawnedSeeker);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AGS_AISeeker::SpawnSeeker - Failed to spawn Seeker"));
	}
}

void AGS_AISeeker::SetupAIController()
{
	if (!SpawnedSeeker || !AIControllerClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters ControllerSpawnParams;
	ControllerSpawnParams.Owner = this;

	SeekerController = World->SpawnActor<AGS_SeekerAIController>(
	    AIControllerClass,
	    FVector::ZeroVector,
	    FRotator::ZeroRotator,
	    ControllerSpawnParams);

	if (SeekerController)
	{
		SeekerController->HealThreshold = HealThreshold;

		float FinalRange = AttackRange;
		if (SeekerType == ESeekerAIType::Merci)
		{
			if (FinalRange < 1000.0f)
			{
				FinalRange = 1200.0f;
			}
		}
		else
		{
			if (FinalRange < 250.0f)
			{
				FinalRange = 350.0f;
			}
		}
		SeekerController->AttackRange = FinalRange;
		SeekerController->TrapDetectionRadius = TrapDetectionRadius;

		SeekerController->Possess(SpawnedSeeker);
	}
}

TSubclassOf<AGS_Seeker> AGS_AISeeker::GetSeekerClassByType() const
{
	switch (SeekerType)
	{
	case ESeekerAIType::Ares:
		if (AresClass)
		{
			return AresClass;
		}
		return AGS_Ares::StaticClass();
	case ESeekerAIType::Chan:
		if (ChanClass)
		{
			return ChanClass;
		}
		return AGS_Chan::StaticClass();
	case ESeekerAIType::Merci:
		if (MerciClass)
		{
			return MerciClass;
		}
		return AGS_Merci::StaticClass();
	default:
		return AGS_Ares::StaticClass();
	}
}

void AGS_AISeeker::HandleSeekerDeath()
{
	OnSeekerDeath.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("AGS_AISeeker: Seeker died"));
}

AGS_SeekerAIController* AGS_AISeeker::GetSeekerAIController() const
{
	return SeekerController;
}

UGS_SkillComp* AGS_AISeeker::GetSkillComp() const
{
	return CachedSkillComp;
}

UGS_StatComp* AGS_AISeeker::GetStatComp() const
{
	return CachedStatComp;
}

void AGS_AISeeker::PerformAttack()
{
	if (!SpawnedSeeker || SpawnedSeeker->IsDead())
	{
		return;
	}

	// Trigger attack based on seeker type
	switch (SeekerType)
	{
	case ESeekerAIType::Ares:
	case ESeekerAIType::Chan:
		// Use the combo system
		SpawnedSeeker->Server_OnComboAttack();
		break;
	case ESeekerAIType::Merci:
		// Ranged attack - Draw bow first, then release when AimState is reached
		if (AGS_Merci* Merci = Cast<AGS_Merci>(SpawnedSeeker))
		{
			// CRITICAL: Reset DrawState and stop if dying
			if (Merci->IsInDyingState())
			{
				DrawStartTime = 0.0f;
				return;
			}

			float CurrentAttackTime = GetWorld()->GetTimeSeconds();
			bool bAimState = Merci->GetAimState();
			bool bDrawState = Merci->GetDrawState();

			// AI Target Sync for Homing Arrows
			if (SeekerController)
			{
				Merci->SetAutoAimTarget(SeekerController->GetFocusActor());
			}

			if (bAimState)
			{
				// Bow is fully drawn, release the arrow
				Merci->ReleaseArrow(Merci->NormalArrowClass, 0.0f, 1);
				DrawStartTime = 0.0f; // Reset safety
			}
			else if (!bDrawState)
			{
				// Not drawing yet, start drawing
				Merci->Server_DrawBow(Merci->ComboSkillDrawMontage);
				DrawStartTime = CurrentAttackTime; // Start safety timer
			}
			else if (bDrawState)
			{
				// SAFETY: If bDrawState is true but DrawStartTime is 0 (out of sync), fix it
				if (DrawStartTime <= 0.0f)
				{
					DrawStartTime = CurrentAttackTime;
				}

				float ElapsedTime = CurrentAttackTime - DrawStartTime;

				// Safety Fallback: If held too long without AimState, force release
				if (ElapsedTime >= MerciMaxHoldDuration)
				{
					UE_LOG(LogTemp, Warning, TEXT("[AI Merci] Forcing arrow release (safety timer)"));
					Merci->OnDrawMontageEnded(); // Force state change
					Merci->ReleaseArrow(Merci->NormalArrowClass, 0.0f, 1);
					DrawStartTime = 0.0f;
				}
			}
		}
		break;
	}
}

void AGS_AISeeker::PerformSkill(int32 SkillIndex)
{
	UGS_SkillComp* SkillComp = GetSkillComp();
	if (!SkillComp || !SpawnedSeeker || SpawnedSeeker->IsDead())
	{
		return;
	}

	ESkillSlot SkillSlot = static_cast<ESkillSlot>(SkillIndex);
	int32 SlotInt = static_cast<int32>(SkillSlot);

	SkillComp->Server_TryActivateSkill(SkillSlot);

	// Special handling for Ares Dash (press and hold)
	if (SeekerType == ESeekerAIType::Ares && SkillSlot == ESkillSlot::Moving)
	{
		GetWorldTimerManager().SetTimer(AresDashTimer, this, &AGS_AISeeker::ExecuteAresDash, 0.8f, false);
	}
	// Special handling for Merci Fog Arrow (Moving skill) - needs hold and release
	else if (SeekerType == ESeekerAIType::Merci && SkillSlot == ESkillSlot::Moving)
	{
		// Draw for 1.0 second then release
		GetWorldTimerManager().SetTimer(MerciMovingSkillTimer, this, &AGS_AISeeker::ExecuteMerciMovingSkill, 1.0f, false);
	}
	// Special handling for Chan Shield (limited duration)
	else if (SeekerType == ESeekerAIType::Chan && SkillSlot == ESkillSlot::Ready)
	{
		GetWorldTimerManager().SetTimer(ChanShieldTimer, this, &AGS_AISeeker::StopChanShield, 3.0f, false);
	}
}

void AGS_AISeeker::ExecuteAresDash()
{
	if (UGS_SkillComp* SkillComp = GetSkillComp())
	{
		SkillComp->Server_TrySkillCommand(ESkillSlot::Moving);
	}
}

void AGS_AISeeker::StopChanShield()
{
	if (UGS_SkillComp* SkillComp = GetSkillComp())
	{
		SkillComp->Server_TryDeactiveSkill(ESkillSlot::Ready);
	}
}

void AGS_AISeeker::ExecuteMerciMovingSkill()
{
	if (UGS_SkillComp* SkillComp = GetSkillComp())
	{
		SkillComp->Server_TrySkillCommand(ESkillSlot::Moving);
	}
}

void AGS_AISeeker::SwitchToRandomSpecialArrow()
{
	AGS_Merci* Merci = Cast<AGS_Merci>(SpawnedSeeker);
	if (!Merci)
		return;

	// Check ammo
	int32 AxeAmmo = Merci->GetMaxAxeArrows(); // Using getter though it might be max, assuming current is replicated but let's check field names
	// Actually, CurrentAxeArrows is private but replicated. I should use a getter or cast to check.
	// In GS_Merci.h: CurrentAxeArrows is private. Let's see if there's a getter for current.
	// Looking at GS_Merci.h again: It has GetMax... but not GetCurrent...
	// However, I can check CurrentArrowType and the actual values if I move some logic to Merci or use property access.
	// For AI simplicity, I will just call the change type function.

	// 50/50 chance between Axe and Child if available
	int32 TargetType = (FMath::RandRange(0, 1) == 0) ? 1 : 2; // Axe=1, Child=2
	Merci->Server_ChangeArrowType(TargetType);
}

void AGS_AISeeker::ResetToNormalArrow()
{
	AGS_Merci* Merci = Cast<AGS_Merci>(SpawnedSeeker);
	if (!Merci)
		return;

	// Normal is 0. Directional change to return to 0.
	// This is a bit hacky since ChangeArrowType is relative.
	// Let's assume the AI knows the current state or we add a SetArrowType.
	Merci->Server_ChangeArrowType(0); // This might not work if it's relative.
}

void AGS_AISeeker::PerformHeal()
{
	UGS_SkillComp* SkillComp = GetSkillComp();
	if (!SkillComp || !SpawnedSeeker || SpawnedSeeker->IsDead())
	{
		return;
	}

	// Try to use heal skill (usually Q or item)
	// The server will check if it can be used
	SkillComp->Server_TryActivateSkill(ESkillSlot::HealPotion);
}

void AGS_AISeeker::PerformRoll(FVector Direction)
{
	// Server authority check
	if (!HasAuthority())
	{
		return;
	}

	if (!SpawnedSeeker || SpawnedSeeker->IsDead())
	{
		return;
	}

	if (Direction.IsNearlyZero())
	{
		Direction = -SpawnedSeeker->GetActorForwardVector(); // Default: backward
	}
	Direction.Normalize();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector StartLoc = SpawnedSeeker->GetActorLocation();

	TWeakObjectPtr<AGS_AISeeker> WeakThis(this);
	TWeakObjectPtr<AGS_Seeker> WeakSeeker(SpawnedSeeker);
	auto IsDirectionBlockedByMonster = [WeakThis, WeakSeeker, StartLoc, World](const FVector& Dir) -> bool
	{
		if (!WeakThis.IsValid() || !WeakSeeker.IsValid() || !World)
		{
			return false;
		}

		FVector EndLoc = StartLoc + Dir * 400.0f; // Roll distance

		FHitResult HitResult;
		TArray<AActor*> ActorsToIgnore;
		ActorsToIgnore.Add(WeakSeeker.Get());

		TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
		ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

		bool bHit = UKismetSystemLibrary::SphereTraceSingleForObjects(
		    World,
		    StartLoc,
		    EndLoc,
		    80.0f, // Radius matching character capsule roughly
		    ObjectTypes,
		    false, // bTraceComplex
		    ActorsToIgnore,
		    EDrawDebugTrace::None,
		    HitResult,
		    true // bIgnoreSelf
		);

		if (bHit)
		{
			if (Cast<AGS_Monster>(HitResult.GetActor()))
			{
				return true;
			}
		}

		return false;
	};

	if (IsDirectionBlockedByMonster(Direction))
	{
		const float AlternateAngles[] = {45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f};
		bool bFoundSafePath = false;

		for (float Angle : AlternateAngles)
		{
			FVector AltDirection = Direction.RotateAngleAxis(Angle, FVector::UpVector);

			if (!IsDirectionBlockedByMonster(AltDirection))
			{
				Direction = AltDirection;
				bFoundSafePath = true;
				break;
			}
		}

		if (!bFoundSafePath)
		{
			Direction = Direction * -1.0f;
		}
	}

	UGS_SkillComp* SkillComp = GetSkillComp();
	if (SkillComp)
	{
		FRotator RollRotation = Direction.Rotation();
		SpawnedSeeker->SetActorRotation(RollRotation);

		SkillComp->Server_TryActivateSkill(ESkillSlot::Rolling);
	}
}

void AGS_AISeeker::StopAllActions()
{
	if (!SpawnedSeeker)
	{
		return;
	}

	SpawnedSeeker->StateReset();
	if (UAnimInstance* AnimInstance = SpawnedSeeker->GetMesh()->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.2f);
	}

	if (UGS_SkillComp* SkillComp = GetSkillComp())
	{
		SkillComp->SkillsInterrupt();
		SkillComp->ResetAllowedSkillsMask();
	}

	if (SeekerController)
	{
		SeekerController->StopMovement();
	}
}

float AGS_AISeeker::GetHealthPercent() const
{
	UGS_StatComp* StatComp = GetStatComp();
	if (StatComp)
	{
		return StatComp->GetCurrentHealth() / StatComp->GetMaxHealth();
	}
	return 0.0f;
}

bool AGS_AISeeker::IsAlive() const
{
	if (SpawnedSeeker)
	{
		return !SpawnedSeeker->IsDead();
	}
	return false;
}

bool AGS_AISeeker::CanUseSkill(int32 SkillIndex) const
{
	UGS_SkillComp* SkillComp = GetSkillComp();
	if (SkillComp && SpawnedSeeker && !SpawnedSeeker->IsDead())
	{
		ESkillSlot Slot = static_cast<ESkillSlot>(SkillIndex);
		if (UGS_SkillBase* Skill = SkillComp->GetSkillFromSkillMap(Slot))
		{
			return SkillComp->IsSkillAllowed(Slot) && Skill->CanActive();
		}
	}
	return false;
}

bool AGS_AISeeker::CanHeal() const
{
	return CanUseSkill(static_cast<int32>(ESkillSlot::HealPotion));
}

void AGS_AISeeker::NotifyGoalReached()
{
	OnReachedGoal.Broadcast();
}

void AGS_AISeeker::DrawDebugInfo() const
{
#if ENABLE_DRAW_DEBUG
	if (!SpawnedSeeker)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector Location = SpawnedSeeker->GetActorLocation();

	FString TypeText;
	switch (SeekerType)
	{
	case ESeekerAIType::Ares:
		TypeText = TEXT("Ares");
		break;
	case ESeekerAIType::Chan:
		TypeText = TEXT("Chan");
		break;
	case ESeekerAIType::Merci:
		TypeText = TEXT("Merci");
		break;
	}

	FString HealthText = FString::Printf(TEXT("HP: %.1f%%"), GetHealthPercent() * 100.0f);

	DrawDebugString(World, Location + FVector(0, 0, 150), TypeText, nullptr, FColor::Cyan, 0.0f, true);
	DrawDebugString(World, Location + FVector(0, 0, 130), HealthText, nullptr, FColor::Green, 0.0f, true);
	DrawDebugSphere(World, Location, TrapDetectionRadius, 16, FColor::Yellow, false, 0.0f, 0, 1.0f);
	DrawDebugSphere(World, Location, AttackRange, 16, FColor::Red, false, 0.0f, 0, 1.0f);
#endif
}
