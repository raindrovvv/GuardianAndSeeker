// Copyright

#include "Character/Component/GS_DeathCinematicComponent.h"
#include "Components/PostProcessComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UGS_DeathCinematicComponent::UGS_DeathCinematicComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UGS_DeathCinematicComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsPlaying)
	{
		TickUpdate(DeltaTime);
	}
}

void UGS_DeathCinematicComponent::InitializeForOwner(AActor* InOwner, UPostProcessComponent* InPostProcessComp,
                                                     UMaterialInterface* InMaterialOverride, USpringArmComponent* InSpringArm)
{
	ManagedPostProcessComp = InPostProcessComp;

	if (InMaterialOverride)
	{
		EffectMaterial = InMaterialOverride;
	}

	// 캐릭터 캐싱 및 로컬 플레이어 여부 저장 (죽을 때 컨트롤러가 분리되므로 미리 저장)
	OwnerCharacter = Cast<ACharacter>(InOwner);
	if (OwnerCharacter.IsValid())
	{
		bIsOwnedByLocalPlayer = OwnerCharacter->IsLocallyControlled();
	}

	// PostProcess 초기 설정
	if (ManagedPostProcessComp.IsValid())
	{
		ManagedPostProcessComp->bEnabled = false;
		ManagedPostProcessComp->BlendWeight = 1.0f;
	}

	// SpringArm 저장 (카메라 회전용)
	ManagedSpringArm = InSpringArm;
}

void UGS_DeathCinematicComponent::PlayDeathCinematic()
{
	if (bIsPlaying)
	{
		return;
	}

	if (!OwnerCharacter.IsValid())
	{
		return;
	}

	// 초기화 시점에 저장한 로컬 플레이어 여부로 체크
	if (!bIsOwnedByLocalPlayer)
	{
		return;
	}

	// 뷰 타겟을 죽은 캐릭터로 강제 유지 (관전자 모드 전환 방지)
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (PC->IsLocalController())
			{
				// 원래 ViewTarget 저장 (복원용)
				OriginalViewTarget = PC->GetViewTarget();
				PC->SetViewTargetWithBlend(OwnerCharacter.Get(), 0.0f);
			}
		}
	}

	bIsPlaying = true;
	ElapsedTime = 0.0f;
	TargetStrength = MaxEffectStrength;

	// PostProcess 활성화
	EnsureMID();
	if (ManagedPostProcessComp.IsValid())
	{
		ManagedPostProcessComp->Priority = 1000.0f;
		ManagedPostProcessComp->bEnabled = true;
		ManagedPostProcessComp->BlendWeight = 1.0f;
	}

	// 슬로우 모션 시작 (캐릭터별 CustomTimeDilation으로 멀티플레이어 호환)
	if (OwnerCharacter.IsValid())
	{
		OwnerCharacter->CustomTimeDilation = TimeDilationFactor;
	}

	// 업데이트 시작 (Tick 활성화)
	SetComponentTickEnabled(true);

	// 카메라 공전 시작
	if (bEnableCameraRotation)
	{
		StartCameraRotation();
	}
}

void UGS_DeathCinematicComponent::StopCinematic()
{
	if (!bIsPlaying)
	{
		return;
	}

	bIsPlaying = false;
	bIsCameraRotating = false;
	SetComponentTickEnabled(false);

	// CustomTimeDilation 복원
	if (OwnerCharacter.IsValid())
	{
		OwnerCharacter->CustomTimeDilation = 1.0f;
	}

	// 카메라 및 컨트롤러 설정 복구
	if (ManagedSpringArm.IsValid())
	{
		ManagedSpringArm->SetUsingAbsoluteRotation(false);

		// 컨트롤러 사용 여부 복구
		ManagedSpringArm->bUsePawnControlRotation = bOriginalUsePawnControlRotation;

		// 기본 위치로 부드럽게 돌아가도록 설정 (필요 시 상대 회전 초기화)
		ManagedSpringArm->SetRelativeRotation(FRotator::ZeroRotator);
	}

	// ViewTarget 복원 (관전자 모드로 전환 허용)
	if (OriginalViewTarget.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (PC->IsLocalController())
				{
					PC->SetViewTarget(OriginalViewTarget.Get());
				}
			}
		}
		OriginalViewTarget.Reset();
	}

	ApplyEffect(0.0f);
	if (ManagedPostProcessComp.IsValid())
	{
		ManagedPostProcessComp->bEnabled = false;
	}
}

void UGS_DeathCinematicComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCinematic();
	Super::EndPlay(EndPlayReason);
}

void UGS_DeathCinematicComponent::EnsureMID()
{
	if (DynamicMaterial || !EffectMaterial || !ManagedPostProcessComp.IsValid())
	{
		return;
	}

	DynamicMaterial = UMaterialInstanceDynamic::Create(EffectMaterial, this);
	if (DynamicMaterial)
	{
		ManagedPostProcessComp->Settings.WeightedBlendables.Array.Empty();
		ManagedPostProcessComp->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, DynamicMaterial));
		ApplyEffect(0.0f);
	}
}

void UGS_DeathCinematicComponent::TickUpdate(float DeltaTime)
{
	if (!bIsPlaying || !GetWorld())
		return;

	// 매 프레임 ViewTarget 강제 설정 (관전자 모드 시스템과의 경합 방지)
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->IsLocalController() && OwnerCharacter.IsValid())
		{
			// 관전자 모드가 뺏어가려 할 때 즉시 탈환
			if (PC->GetViewTarget() != OwnerCharacter.Get())
			{
				PC->SetViewTarget(OwnerCharacter.Get());
			}
		}
	}

	// 슬로우 모션 보정: CustomTimeDilation이 적용된 DeltaTime을 실제 시간으로 변환
	// DeltaTime = RealDeltaTime * CustomTimeDilation 이므로,
	// RealDeltaTime = DeltaTime / CustomTimeDilation
	float CharacterDilation = OwnerCharacter.IsValid()
	                              ? FMath::Max(OwnerCharacter->CustomTimeDilation, 0.001f)
	                              : 1.0f;
	float RealDeltaTime = DeltaTime / CharacterDilation;

	ElapsedTime += RealDeltaTime;

	// 카메라 회전 업데이트
	if (bIsCameraRotating)
	{
		UpdateCameraRotation(RealDeltaTime);
	}

	// 페이드 인/유지/아웃 단계
	if (ElapsedTime < FadeInDuration)
	{
		float Alpha = ElapsedTime / FadeInDuration;
		CurrentStrength = FMath::Lerp(0.0f, TargetStrength, Alpha);
	}
	else if (ElapsedTime < SlowMotionDuration - FadeOutDuration)
	{
		CurrentStrength = TargetStrength;
	}
	else if (ElapsedTime < SlowMotionDuration + FadeOutDuration)
	{
		float FadeOutElapsed = ElapsedTime - (SlowMotionDuration - FadeOutDuration);
		float Alpha = 1.0f - (FadeOutElapsed / (FadeOutDuration * 2.0f));
		CurrentStrength = TargetStrength * FMath::Max(Alpha, 0.0f);
	}
	else
	{
		StopCinematic();
		return;
	}

	ApplyEffect(CurrentStrength);
}
void UGS_DeathCinematicComponent::ApplyEffect(float Strength)
{
	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(StrengthParamName, Strength);
		DynamicMaterial->SetScalarParameterValue(DesaturationParamName, Strength * 0.5f);
	}
}

void UGS_DeathCinematicComponent::StartCameraRotation()
{
	if (!ManagedSpringArm.IsValid())
		return;

	// 현재 월드 회전 전체 저장 (Pitch/Roll 유지용)
	OriginalSpringArmRotation = ManagedSpringArm->GetComponentRotation();
	OriginalSpringArmYaw = OriginalSpringArmRotation.Yaw;
	TargetSpringArmYaw = OriginalSpringArmYaw + CameraRotationAngle;

	// 월드 회전 시스템 강제 활성화 (캐릭터의 몸부림/회전 무시)
	ManagedSpringArm->SetUsingAbsoluteRotation(true);

	bOriginalUsePawnControlRotation = ManagedSpringArm->bUsePawnControlRotation;
	ManagedSpringArm->bUsePawnControlRotation = false;

	CameraRotationElapsed = 0.0f;
	bIsCameraRotating = true;
}

void UGS_DeathCinematicComponent::UpdateCameraRotation(float RealDeltaTime)
{
	if (!ManagedSpringArm.IsValid())
	{
		bIsCameraRotating = false;
		return;
	}

	CameraRotationElapsed += RealDeltaTime;

	float Alpha = FMath::Clamp(CameraRotationElapsed / CameraRotationDuration, 0.0f, 1.0f);
	float SmoothedAlpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);

	// Yaw 보간 (캐릭터 앞면으로 공전)
	float NewYaw = FMath::Lerp(OriginalSpringArmYaw, TargetSpringArmYaw, SmoothedAlpha);

	// Pitch 보간 (쓰러진 캐릭터를 내려다봄)
	float TargetPitch = OriginalSpringArmRotation.Pitch + CameraPitchAngle;
	float NewPitch = FMath::Lerp(OriginalSpringArmRotation.Pitch, TargetPitch, SmoothedAlpha);

	// 월드 회전 설정 (Roll은 원래 값 유지)
	FRotator NewRot = OriginalSpringArmRotation;
	NewRot.Yaw = NewYaw;
	NewRot.Pitch = NewPitch;
	ManagedSpringArm->SetWorldRotation(NewRot);

	if (CameraRotationElapsed >= CameraRotationDuration)
	{
		bIsCameraRotating = false;
	}
}
