// Copyright

#include "Character/Component/GS_RevivalEffectComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

UGS_RevivalEffectComponent::UGS_RevivalEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGS_RevivalEffectComponent::InitializeForOwner(AActor* InOwner, UPostProcessComponent* InPostProcessComp,
                                                    UMaterialInterface* InMaterialOverride)
{
	OwnerActor = InOwner;
	ManagedPostProcessComp = InPostProcessComp;

	if (InMaterialOverride)
	{
		EffectMaterial = InMaterialOverride;
	}

	// PostProcess 초기 설정
	if (ManagedPostProcessComp.IsValid())
	{
		ManagedPostProcessComp->bEnabled = false;
		ManagedPostProcessComp->BlendWeight = 1.0f;
	}
}

void UGS_RevivalEffectComponent::PlayRevivalEffect()
{
	if (bIsPlaying)
	{
		return;
	}

	if (!OwnerActor.IsValid())
	{
		return;
	}

	// 로컬 플레이어 체크 - 멀티플레이 핵심 로직
	ACharacter* OwnerCharacter = Cast<ACharacter>(OwnerActor.Get());
	if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled())
	{
		return;
	}

	bIsPlaying = true;
	ElapsedTime = 0.0f;
	CurrentStrength = FlashIntensity;

	// PostProcess 활성화
	EnsureMID();
	if (ManagedPostProcessComp.IsValid())
	{
		ManagedPostProcessComp->bEnabled = true;
	}

	// 초기 플래시 적용
	ApplyEffect(CurrentStrength, RevivalGlowColor);

	// 페이드 아웃 타이머 시작
	StartTimer();
}

void UGS_RevivalEffectComponent::StopEffect()
{
	if (!bIsPlaying)
	{
		return;
	}

	bIsPlaying = false;
	StopTimer();

	// 효과 즉시 제거
	ApplyEffect(0.0f, RevivalGlowColor);
	if (ManagedPostProcessComp.IsValid())
	{
		ManagedPostProcessComp->bEnabled = false;
	}
}

void UGS_RevivalEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopEffect();
	Super::EndPlay(EndPlayReason);
}

void UGS_RevivalEffectComponent::EnsureMID()
{
	if (DynamicMaterial)
	{
		return;
	}

	if (!EffectMaterial || !ManagedPostProcessComp.IsValid())
	{
		return;
	}

	DynamicMaterial = UMaterialInstanceDynamic::Create(EffectMaterial, this);
	if (DynamicMaterial)
	{
		// 기존 머티리얼 제거 후 추가
		ManagedPostProcessComp->Settings.WeightedBlendables.Array.Empty();
		ManagedPostProcessComp->Settings.WeightedBlendables.Array.Add(
		    FWeightedBlendable(1.0f, DynamicMaterial));

		// 초기 강도 0
		ApplyEffect(0.0f, RevivalGlowColor);
	}
}

void UGS_RevivalEffectComponent::StartTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
		    UpdateTimerHandle,
		    this,
		    &UGS_RevivalEffectComponent::TickUpdate,
		    0.016f, // ~60fps
		    true);
	}
}

void UGS_RevivalEffectComponent::StopTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}
}

void UGS_RevivalEffectComponent::TickUpdate()
{
	if (!bIsPlaying)
	{
		StopTimer();
		return;
	}

	constexpr float DeltaTime = 0.016f;
	ElapsedTime += DeltaTime;

	// 페이드 아웃 (선형 보간 + 이즈 아웃)
	float Alpha = FMath::Clamp(ElapsedTime / FadeOutDuration, 0.0f, 1.0f);

	// Ease Out Quad for smoother transition
	float EasedAlpha = 1.0f - FMath::Square(1.0f - Alpha);

	CurrentStrength = FMath::Lerp(FlashIntensity, 0.0f, EasedAlpha);

	if (CurrentStrength <= 0.01f)
	{
		// 페이드 아웃 완료
		CurrentStrength = 0.0f;
		bIsPlaying = false;
		StopTimer();

		if (ManagedPostProcessComp.IsValid())
		{
			ManagedPostProcessComp->bEnabled = false;
		}
	}

	ApplyEffect(CurrentStrength, RevivalGlowColor);
}

void UGS_RevivalEffectComponent::ApplyEffect(float Strength, const FLinearColor& Color)
{
	if (!DynamicMaterial)
	{
		return;
	}

	DynamicMaterial->SetScalarParameterValue(StrengthParamName, Strength);
	DynamicMaterial->SetVectorParameterValue(ColorParamName, Color);
}
