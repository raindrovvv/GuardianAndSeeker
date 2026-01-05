#include "Character/Component/GS_DrakharVFXComponent.h"
#include "Character/Player/Guardian/GS_Drakhar.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/ArrowComponent.h"
#include "Character/Component/GS_CameraShakeComponent.h"
#include "Character/Component/GS_FootManagerComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetSystemLibrary.h"

UGS_DrakharVFXComponent::UGS_DrakharVFXComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // Tick 비활성화 - Timer 사용

	// Initialize component pointers
	ActiveWingRushVFXComponent = nullptr;
	ActiveDustVFXComponent = nullptr;
	ActiveGroundCrackVFXComponent = nullptr;
	ActiveDustCloudVFXComponent = nullptr;
	ActiveFlyingDustVFXComponent = nullptr;
}
// 파라미터명 정의
const FName UGS_DrakharVFXComponent::Param_DashDirection = FName("DashDirection");
const FName UGS_DrakharVFXComponent::Param_DashSpeed = FName("DashSpeed");
const FName UGS_DrakharVFXComponent::Param_Scale = FName("Scale");
const FName UGS_DrakharVFXComponent::Param_CrackIntensity = FName("CrackIntensity");
const FName UGS_DrakharVFXComponent::Param_CrackRadius = FName("CrackRadius");
const FName UGS_DrakharVFXComponent::Param_DustIntensity = FName("DustIntensity");
const FName UGS_DrakharVFXComponent::Param_DustRadius = FName("DustRadius");
const FName UGS_DrakharVFXComponent::Param_WindStrength = FName("WindStrength");

void UGS_DrakharVFXComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerDrakhar = Cast<AGS_Drakhar>(GetOwner());

	if (OwnerDrakhar)
	{
		FootManagerComponent = OwnerDrakhar->FindComponentByClass<UGS_FootManagerComponent>();

		// Find Arrow Components by name
		TArray<UArrowComponent*> ArrowComponents;
		OwnerDrakhar->GetComponents<UArrowComponent>(ArrowComponents);
		for (UArrowComponent* Arrow : ArrowComponents)
		{
			if (Arrow->GetFName() == TEXT("WingRushVFXSpawnPoint"))
			{
				WingRushVFXSpawnPoint = Arrow;
			}
			else if (Arrow->GetFName() == TEXT("EarthquakeVFXSpawnPoint"))
			{
				EarthquakeVFXSpawnPoint = Arrow;
			}
		}
	}
}

void UGS_DrakharVFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 타이머 정리
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FlyingDustUpdateTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UGS_DrakharVFXComponent::UpdateFlyingDustVFXLocation()
{
	if (ActiveFlyingDustVFXComponent && IsValid(ActiveFlyingDustVFXComponent) && OwnerDrakhar)
	{
		FVector Start = OwnerDrakhar->GetActorLocation();
		FVector End = Start - FVector(0, 0, OwnerDrakhar->FlyingDustTraceDistance);
		FHitResult HitResult;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(OwnerDrakhar);

		if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params))
		{
			ActiveFlyingDustVFXComponent->SetWorldLocation(HitResult.ImpactPoint);
			if (!ActiveFlyingDustVFXComponent->IsActive())
			{
				ActiveFlyingDustVFXComponent->Activate(true);
			}
		}
		else
		{
			if (ActiveFlyingDustVFXComponent->IsActive())
			{
				ActiveFlyingDustVFXComponent->Deactivate();
			}
		}
	}
}

void UGS_DrakharVFXComponent::OnFlyStart()
{
	if (!OwnerDrakhar)
		return;
	if (GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (OwnerDrakhar->FlyingDustVFX && !IsValid(ActiveFlyingDustVFXComponent))
	{
		FVector Location = OwnerDrakhar->GetActorLocation() - FVector(0, 0, OwnerDrakhar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

		// VFX 거리 기반 컬링 (비행 먼지 VFX)
		if (!OwnerDrakhar->ShouldPlayVFXAtLocation(Location, 4000.0f))
		{
			return;
		}

		ActiveFlyingDustVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), OwnerDrakhar->FlyingDustVFX, Location);
		if (ActiveFlyingDustVFXComponent)
		{
			ActiveFlyingDustVFXComponent->SetAutoDestroy(false);
			ActiveFlyingDustVFXComponent->Deactivate();

			// Timer 시작 (설정 간격마다 위치 업데이트)
			GetWorld()->GetTimerManager().SetTimer(
			    FlyingDustUpdateTimerHandle,
			    this,
			    &UGS_DrakharVFXComponent::UpdateFlyingDustVFXLocation,
			    FlyingDustUpdateInterval,
			    true // Loop
			);
		}
	}

	OwnerDrakhar->BP_OnFlyStart();
}

void UGS_DrakharVFXComponent::OnFlyEnd()
{
	if (!OwnerDrakhar)
		return;
	if (GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Timer 중지
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FlyingDustUpdateTimerHandle);
	}

	if (ActiveFlyingDustVFXComponent && IsValid(ActiveFlyingDustVFXComponent))
	{
		ActiveFlyingDustVFXComponent->DestroyComponent();
		ActiveFlyingDustVFXComponent = nullptr;
	}

	OwnerDrakhar->BP_OnFlyEnd();
}

void UGS_DrakharVFXComponent::OnUltimateStart()
{
	if (OwnerDrakhar)
	{
		OwnerDrakhar->BP_OnUltimateStart();
	}
}

void UGS_DrakharVFXComponent::OnEarthquakeStart()
{
	if (OwnerDrakhar)
	{
		UGS_CameraShakeComponent* CameraShakeComponent = OwnerDrakhar->GetCameraShakeComponent();
		if (CameraShakeComponent)
		{
			CameraShakeComponent->PlayCameraShake(OwnerDrakhar->EarthquakeShakeInfo);
		}

		StartGroundCrackVFX();
		StartDustCloudVFX();

		OwnerDrakhar->BP_OnEarthquakeStart();
	}
}

void UGS_DrakharVFXComponent::StartWingRushVFX()
{
	if (!OwnerDrakhar)
		return;
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		return;

	if (ActiveWingRushVFXComponent && IsValid(ActiveWingRushVFXComponent))
	{
		ActiveWingRushVFXComponent->DestroyComponent();
		ActiveWingRushVFXComponent = nullptr;
	}

	UNiagaraSystem* VFXToSpawn = OwnerDrakhar->GetIsFeverMode() ? OwnerDrakhar->FeverWingRushRibbonVFX : OwnerDrakhar->WingRushRibbonVFX;
	if (!VFXToSpawn)
		return;

	if (WingRushVFXSpawnPoint && IsValid(WingRushVFXSpawnPoint))
	{
		ActiveWingRushVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(VFXToSpawn, WingRushVFXSpawnPoint, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}
	else
	{
		ActiveWingRushVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(VFXToSpawn, OwnerDrakhar->GetMesh(), FName("foot_l"), FVector(-20.f, 0.f, 0.f), FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}

	if (ActiveWingRushVFXComponent)
	{
		FVector CurrentDashDirection = (OwnerDrakhar->DashEndLocation - OwnerDrakhar->DashStartLocation).GetSafeNormal();
		if (!CurrentDashDirection.IsZero())
		{
			ActiveWingRushVFXComponent->SetVectorParameter(Param_DashDirection, CurrentDashDirection);
		}

		float DashSpeed = OwnerDrakhar->DashPower / OwnerDrakhar->DashDuration;
		ActiveWingRushVFXComponent->SetFloatParameter(Param_DashSpeed, DashSpeed);
		ActiveWingRushVFXComponent->SetFloatParameter(Param_Scale, 2.0f);
		ActiveWingRushVFXComponent->SetWorldScale3D(FVector(3.0f, 3.0f, 3.0f));
	}
}

void UGS_DrakharVFXComponent::StopWingRushVFX()
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		return;

	if (ActiveWingRushVFXComponent && IsValid(ActiveWingRushVFXComponent))
	{
		ActiveWingRushVFXComponent->Deactivate();
		FTimerHandle VFXCleanupTimer;
		TWeakObjectPtr<UGS_DrakharVFXComponent> WeakThis = this;
		GetWorld()->GetTimerManager().SetTimer(VFXCleanupTimer, [WeakThis]()
		                                       {
			if (!WeakThis.IsValid()) return;

			if (WeakThis->ActiveWingRushVFXComponent && IsValid(WeakThis->ActiveWingRushVFXComponent))
			{
				WeakThis->ActiveWingRushVFXComponent->DestroyComponent();
			}
			WeakThis->ActiveWingRushVFXComponent = nullptr; }, WingRushCleanupDelay, false);
	}
}

void UGS_DrakharVFXComponent::StartDustVFX()
{
	if (!OwnerDrakhar)
		return;
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		return;

	if (ActiveDustVFXComponent && IsValid(ActiveDustVFXComponent))
	{
		ActiveDustVFXComponent->DestroyComponent();
		ActiveDustVFXComponent = nullptr;
	}

	UNiagaraSystem* VFXToSpawn = OwnerDrakhar->GetIsFeverMode() ? OwnerDrakhar->FeverDustVFX : OwnerDrakhar->DustVFX;
	if (!VFXToSpawn)
		return;

	if (WingRushVFXSpawnPoint && IsValid(WingRushVFXSpawnPoint))
	{
		ActiveDustVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(VFXToSpawn, WingRushVFXSpawnPoint, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}
	else
	{
		ActiveDustVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(VFXToSpawn, OwnerDrakhar->GetMesh(), FName("foot_l"), FVector(-20.f, 0.f, 0.f), FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}

	if (ActiveDustVFXComponent)
	{
		FVector CurrentDashDirection = (OwnerDrakhar->DashEndLocation - OwnerDrakhar->DashStartLocation).GetSafeNormal();
		if (!CurrentDashDirection.IsZero())
		{
			ActiveDustVFXComponent->SetVectorParameter(Param_DashDirection, CurrentDashDirection);
		}

		float DashSpeed = OwnerDrakhar->DashPower / OwnerDrakhar->DashDuration;
		ActiveDustVFXComponent->SetFloatParameter(Param_DashSpeed, DashSpeed);
		ActiveDustVFXComponent->SetFloatParameter(Param_DustIntensity, 3.0f);
		ActiveDustVFXComponent->SetWorldScale3D(FVector(2.0f, 2.0f, 2.0f));
	}
}

void UGS_DrakharVFXComponent::StopDustVFX()
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		return;

	if (ActiveDustVFXComponent && IsValid(ActiveDustVFXComponent))
	{
		ActiveDustVFXComponent->Deactivate();
		FTimerHandle DustVFXCleanupTimer;
		TWeakObjectPtr<UGS_DrakharVFXComponent> WeakThis = this;
		GetWorld()->GetTimerManager().SetTimer(DustVFXCleanupTimer, [WeakThis]()
		                                       {
			if (!WeakThis.IsValid()) return;

			if (WeakThis->ActiveDustVFXComponent && IsValid(WeakThis->ActiveDustVFXComponent))
			{
				WeakThis->ActiveDustVFXComponent->DestroyComponent();
			}
			WeakThis->ActiveDustVFXComponent = nullptr; }, DustCleanupDelay, false);
	}
}

void UGS_DrakharVFXComponent::StartGroundCrackVFX()
{
	if (!OwnerDrakhar)
		return;
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		return;

	if (ActiveGroundCrackVFXComponent && IsValid(ActiveGroundCrackVFXComponent))
	{
		ActiveGroundCrackVFXComponent->DestroyComponent();
		ActiveGroundCrackVFXComponent = nullptr;
	}

	if (!OwnerDrakhar->GroundCrackVFX)
		return;

	if (EarthquakeVFXSpawnPoint && IsValid(EarthquakeVFXSpawnPoint))
	{
		ActiveGroundCrackVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(OwnerDrakhar->GroundCrackVFX, EarthquakeVFXSpawnPoint, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}
	else
	{
		FVector SpawnLocation = OwnerDrakhar->GetActorLocation() + FVector(0.f, 0.f, -90.f);
		ActiveGroundCrackVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), OwnerDrakhar->GroundCrackVFX, SpawnLocation, OwnerDrakhar->GetActorRotation(), FVector(1.0f, 1.0f, 1.0f), true);
	}

	if (ActiveGroundCrackVFXComponent)
	{
		ActiveGroundCrackVFXComponent->SetFloatParameter(Param_CrackIntensity, OwnerDrakhar->EarthquakeShakeInfo.Intensity);
		ActiveGroundCrackVFXComponent->SetFloatParameter(Param_CrackRadius, OwnerDrakhar->EarthquakeShakeInfo.MaxDistance * 0.5f);
		ActiveGroundCrackVFXComponent->SetWorldScale3D(FVector(2.0f, 2.0f, 1.0f));
	}
}

void UGS_DrakharVFXComponent::StopGroundCrackVFX()
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		return;

	if (ActiveGroundCrackVFXComponent && IsValid(ActiveGroundCrackVFXComponent))
	{
		ActiveGroundCrackVFXComponent->Deactivate();
		FTimerHandle GroundCrackVFXCleanupTimer;
		TWeakObjectPtr<UGS_DrakharVFXComponent> WeakThis = this;
		GetWorld()->GetTimerManager().SetTimer(GroundCrackVFXCleanupTimer, [WeakThis]()
		                                       {
            if (!WeakThis.IsValid()) return;

            if (WeakThis->ActiveGroundCrackVFXComponent && IsValid(WeakThis->ActiveGroundCrackVFXComponent))
            {
                WeakThis->ActiveGroundCrackVFXComponent->DestroyComponent();
            }
            WeakThis->ActiveGroundCrackVFXComponent = nullptr; }, GroundCrackCleanupDelay, false);
	}
}

void UGS_DrakharVFXComponent::StartDustCloudVFX()
{
	if (!OwnerDrakhar)
		return;
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		return;

	if (ActiveDustCloudVFXComponent && IsValid(ActiveDustCloudVFXComponent))
	{
		ActiveDustCloudVFXComponent->DestroyComponent();
		ActiveDustCloudVFXComponent = nullptr;
	}

	if (!OwnerDrakhar->DustCloudVFX)
		return;

	if (EarthquakeVFXSpawnPoint && IsValid(EarthquakeVFXSpawnPoint))
	{
		ActiveDustCloudVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(OwnerDrakhar->DustCloudVFX, EarthquakeVFXSpawnPoint, NAME_None, FVector(0.f, 0.f, 50.f), FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}
	else
	{
		FVector SpawnLocation = OwnerDrakhar->GetActorLocation() + FVector(0.f, 0.f, -40.f);
		ActiveDustCloudVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), OwnerDrakhar->DustCloudVFX, SpawnLocation, OwnerDrakhar->GetActorRotation(), FVector(1.5f, 1.5f, 1.5f), true);
	}

	if (ActiveDustCloudVFXComponent)
	{
		ActiveDustCloudVFXComponent->SetFloatParameter(Param_DustIntensity, OwnerDrakhar->EarthquakeShakeInfo.Intensity * 1.5f);
		ActiveDustCloudVFXComponent->SetFloatParameter(Param_DustRadius, OwnerDrakhar->EarthquakeShakeInfo.MaxDistance * 0.3f);
		ActiveDustCloudVFXComponent->SetFloatParameter(Param_WindStrength, 5.0f);
		ActiveDustCloudVFXComponent->SetWorldScale3D(FVector(3.0f, 3.0f, 2.0f));

		FTimerHandle DustCloudAutoStopTimer;
		GetWorld()->GetTimerManager().SetTimer(DustCloudAutoStopTimer, this, &UGS_DrakharVFXComponent::StopDustCloudVFX, 2.0f, false);
	}
}

void UGS_DrakharVFXComponent::StopDustCloudVFX()
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		return;

	if (ActiveDustCloudVFXComponent && IsValid(ActiveDustCloudVFXComponent))
	{
		ActiveDustCloudVFXComponent->Deactivate();
		FTimerHandle DustCloudVFXCleanupTimer;
		TWeakObjectPtr<UGS_DrakharVFXComponent> WeakThis = this;
		GetWorld()->GetTimerManager().SetTimer(DustCloudVFXCleanupTimer, [WeakThis]()
		                                       {
			if (!WeakThis.IsValid()) return;

			if (WeakThis->ActiveDustCloudVFXComponent && IsValid(WeakThis->ActiveDustCloudVFXComponent))
			{
				WeakThis->ActiveDustCloudVFXComponent->DestroyComponent();
			}
			WeakThis->ActiveDustCloudVFXComponent = nullptr; }, 1.5f, false);
	}
}

void UGS_DrakharVFXComponent::PlayAttackHitVFX(FVector ImpactPoint)
{
	if (!OwnerDrakhar)
		return;
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		return;

	// VFX 거리 기반 컬링
	if (!OwnerDrakhar->ShouldPlayVFXAtLocation(ImpactPoint, 4000.0f))
	{
		return;
	}

	UNiagaraSystem* VFXToPlay = OwnerDrakhar->GetIsFeverMode() ? OwnerDrakhar->FeverAttackHitVFX : OwnerDrakhar->NormalAttackHitVFX;
	if (VFXToPlay)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFXToPlay, ImpactPoint);
	}
}

void UGS_DrakharVFXComponent::PlayEarthquakeImpactVFX(const FVector& ImpactLocation)
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		return;

	// VFX 거리 기반 컬링 (스킬 이펙트)
	if (OwnerDrakhar && !OwnerDrakhar->ShouldPlayVFXAtLocation(ImpactLocation, 5000.0f))
	{
		return;
	}

	if (OwnerDrakhar && OwnerDrakhar->EarthquakeImpactVFX && GetWorld())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), OwnerDrakhar->EarthquakeImpactVFX, ImpactLocation, FRotator::ZeroRotator, FVector(1.0f), true, true, ENCPoolMethod::AutoRelease, true);
	}
}

void UGS_DrakharVFXComponent::PlayFeverEarthquakeImpactVFX(const FVector& ImpactLocation)
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		return;

	// VFX 거리 기반 컬링 (궁극기 이펙트)
	if (OwnerDrakhar && !OwnerDrakhar->ShouldPlayVFXAtLocation(ImpactLocation, 5000.0f))
	{
		return;
	}

	if (OwnerDrakhar && OwnerDrakhar->FeverEarthquakeImpactVFX && GetWorld())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), OwnerDrakhar->FeverEarthquakeImpactVFX, ImpactLocation, FRotator::ZeroRotator, FVector(1.5f), true, true, ENCPoolMethod::AutoRelease, true);
	}
}

void UGS_DrakharVFXComponent::HandleDraconicProjectileImpact(const FVector& ImpactLocation, const FVector& ImpactNormal, bool bHitCharacter)
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer || !OwnerDrakhar)
		return;

	// 피버모드 상태 확인
	bool bIsFeverMode = OwnerDrakhar->GetIsFeverMode();

	// 피버모드와 노멀모드에 따라 다른 VFX 선택
	UNiagaraSystem* VFXToPlay = nullptr;
	if (bHitCharacter)
	{
		// 캐릭터 히트 시 폭발 VFX
		VFXToPlay = bIsFeverMode ? OwnerDrakhar->FeverDraconicProjectileExplosionVFX : OwnerDrakhar->DraconicProjectileExplosionVFX;
	}
	else
	{
		// 일반 임팩트 VFX
		VFXToPlay = bIsFeverMode ? OwnerDrakhar->FeverDraconicProjectileImpactVFX : OwnerDrakhar->DraconicProjectileImpactVFX;
	}

	if (VFXToPlay && GetWorld())
	{
		// 피버모드에 따른 VFX 스케일 및 강도 조정
		float BaseScale = bHitCharacter ? 1.5f : 1.0f;
		float FeverModeMultiplier = bIsFeverMode ? 1.5f : 1.0f;
		float FinalScale = BaseScale * FeverModeMultiplier;

		float BaseIntensity = bHitCharacter ? 2.0f : 1.0f;
		float FinalIntensity = BaseIntensity * (bIsFeverMode ? 1.3f : 1.0f);

		UNiagaraComponent* ImpactVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		    GetWorld(),
		    VFXToPlay,
		    ImpactLocation,
		    FRotationMatrix::MakeFromZ(ImpactNormal).Rotator(),
		    FVector(FinalScale, FinalScale, FinalScale),
		    true, true, ENCPoolMethod::AutoRelease, true);

		if (ImpactVFXComponent)
		{
			ImpactVFXComponent->SetVectorParameter(FName("ImpactNormal"), ImpactNormal);
			ImpactVFXComponent->SetFloatParameter(FName("ImpactIntensity"), FinalIntensity);

			// 피버모드에 따른 색상 변경
			FLinearColor ImpactColor;
			if (bIsFeverMode)
			{
				// 피버모드 시 더 강렬한 색상 (붉은 불꽃)
				ImpactColor = bHitCharacter ? FLinearColor(1.0f, 0.2f, 0.0f, 1.0f) : FLinearColor(1.0f, 0.4f, 0.0f, 1.0f);
			}
			else
			{
				// 노멀모드 시 일반적인 색상
				ImpactColor = bHitCharacter ? FLinearColor::Red : FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);
			}
			ImpactVFXComponent->SetColorParameter(FName("ImpactColor"), ImpactColor);

			// 피버모드 시 추가적인 파라미터 설정
			if (bIsFeverMode)
			{
				ImpactVFXComponent->SetFloatParameter(FName("FeverMode"), 1.0f);
				ImpactVFXComponent->SetFloatParameter(FName("EmissionRate"), 2.0f);
			}
		}
	}

	// 카메라 쉐이크 - 피버모드 시 더 강한 효과
	if (bHitCharacter && OwnerDrakhar && OwnerDrakhar->GetCameraShakeComponent())
	{
		FGS_CameraShakeInfo ImpactShakeInfo;
		ImpactShakeInfo.Intensity = bIsFeverMode ? 6.0f : 4.0f;
		ImpactShakeInfo.MaxDistance = bIsFeverMode ? 1500.0f : 1000.0f;
		ImpactShakeInfo.MinDistance = 100.0f;
		ImpactShakeInfo.PropagationSpeed = 500000.0f;
		ImpactShakeInfo.bUseFalloff = true;
		OwnerDrakhar->GetCameraShakeComponent()->PlayCameraShakeAtLocation(ImpactShakeInfo, ImpactLocation);
	}
}

void UGS_DrakharVFXComponent::OnFeverModeChanged(bool bIsFeverMode)
{
	if (FootManagerComponent)
	{
		if (bIsFeverMode)
		{
			FootManagerComponent->OverrideFootDustEffect(OwnerDrakhar->FeverFootstepVFX);
		}
		else
		{
			FootManagerComponent->ClearFootDustEffectOverride();
		}
	}

	if (bIsFeverMode)
	{
		ApplyFeverModeOverlay();
	}
	else
	{
		RemoveFeverModeOverlay();
	}

	if (OwnerDrakhar && OwnerDrakhar->HasAuthority() && !bIsFeverMode)
	{
		if (IsValid(ActiveDustVFXComponent) && ActiveDustVFXComponent->GetAsset() == OwnerDrakhar->FeverDustVFX)
		{
			StopDustVFX();
		}
	}
}

void UGS_DrakharVFXComponent::ApplyFeverModeOverlay()
{
	if (!OwnerDrakhar)
		return;
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		return;

	if (!OwnerDrakhar->FeverModeOverlayMaterial)
		return;

	USkeletalMeshComponent* MeshComp = OwnerDrakhar->GetMesh();
	if (!MeshComp)
		return;

	if (!FeverModeOverlayMID)
	{
		FeverModeOverlayMID = UMaterialInstanceDynamic::Create(OwnerDrakhar->FeverModeOverlayMaterial, this);
	}

	if (FeverModeOverlayMID)
	{
		FeverModeOverlayMID->SetScalarParameterValue(FName("Intensity"), OwnerDrakhar->FeverOverlayIntensity);
		FeverModeOverlayMID->SetVectorParameterValue(FName("OverlayColor"), OwnerDrakhar->FeverOverlayColor);
		MeshComp->SetOverlayMaterial(FeverModeOverlayMID);
	}
}

void UGS_DrakharVFXComponent::RemoveFeverModeOverlay()
{
	if (!OwnerDrakhar)
		return;
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		return;

	if (USkeletalMeshComponent* MeshComp = OwnerDrakhar->GetMesh())
	{
		MeshComp->SetOverlayMaterial(nullptr);
	}
	FeverModeOverlayMID = nullptr;
}