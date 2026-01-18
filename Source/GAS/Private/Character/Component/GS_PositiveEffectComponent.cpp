#include "Character/Component/GS_PositiveEffectComponent.h"
#include "Character/Component/GS_DamageNumberComponent.h"
#include "UI/Damage/EDamageNumberType.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "Character/GS_Character.h"

UGS_PositiveEffectComponent::UGS_PositiveEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGS_PositiveEffectComponent::InitializeForOwner(AActor* InOwner, UPostProcessComponent* InPostProcessComp,
                                                     UMaterialInterface* InMaterialOverride)
{
	OwnerActor = InOwner;
	ManagedPostProcessComp = InPostProcessComp;

	if (ManagedPostProcessComp.IsValid())
	{
		ManagedPostProcessComp->bEnabled = false;
		ManagedPostProcessComp->Priority = 8;
		ManagedPostProcessComp->BlendWeight = 1.0f;
		ManagedPostProcessComp->bUnbound = true;
		ManagedPostProcessComp->Settings.WeightedBlendables.Array.Empty();
	}

	if (InMaterialOverride)
	{
		EffectMaterial = InMaterialOverride;
	}

	EnsureMID();
}

void UGS_PositiveEffectComponent::EnsureMID()
{
	if (!DynamicMaterial && EffectMaterial && OwnerActor.IsValid())
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(EffectMaterial, OwnerActor.Get());
		if (ManagedPostProcessComp.IsValid() && DynamicMaterial)
		{
			ManagedPostProcessComp->Settings.WeightedBlendables.Array.Empty();
			ManagedPostProcessComp->Settings.AddBlendable(DynamicMaterial, 1.0f);
		}
	}
}

void UGS_PositiveEffectComponent::OnHealed(float HealAmount)
{
	if (!OwnerActor.IsValid() || !ManagedPostProcessComp.IsValid())
	{
		return;
	}

	// 힐 효과 트리거
	CurrentColor = HealColor;
	TargetStrength = 1.0f;
	RemainingDuration = EffectDuration;

	EnsureMID();
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(ColorParamName, CurrentColor);
	}

	if (ManagedPostProcessComp.IsValid() && !bIsActive)
	{
		bIsActive = true;
		ManagedPostProcessComp->bEnabled = true;
		StartTimer();
	}

	// 힐 숫자 팝업 표시 (머리 위치에 표시)
	if (HealAmount > 0.0f)
	{
		if (AGS_Character* OwnerCharacter = Cast<AGS_Character>(OwnerActor.Get()))
		{
			if (UGS_DamageNumberComponent* DmgNumComp = OwnerCharacter->GetDamageNumberComponent())
			{
				// 캐릭터 위치 (캡슐 높이 - 너무 높지 않게)
				FVector HeadLocation = OwnerCharacter->GetActorLocation();
				HeadLocation.Z += OwnerCharacter->GetDefaultHalfHeight(); // 1.0x로 낮춤

				DmgNumComp->ShowDamageNumber(HealAmount, EDamageNumberType::Heal, HeadLocation);
			}
		}
	}
}

void UGS_PositiveEffectComponent::OnBuffReceived(EPositiveEffectType BuffType)
{
	if (!OwnerActor.IsValid() || !ManagedPostProcessComp.IsValid() || BuffType == EPositiveEffectType::None)
	{
		return;
	}

	// 버프 타입에 맞는 색상 설정
	CurrentColor = GetColorForType(BuffType);
	TargetStrength = 1.0f;
	RemainingDuration = EffectDuration;

	EnsureMID();
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(ColorParamName, CurrentColor);
	}

	if (ManagedPostProcessComp.IsValid() && !bIsActive)
	{
		bIsActive = true;
		ManagedPostProcessComp->bEnabled = true;
		StartTimer();
	}

	UE_LOG(LogTemp, Log, TEXT("[PositiveEffect] Buff triggered - Type: %d"), (int32)BuffType);
}

void UGS_PositiveEffectComponent::StopEffect()
{
	StopTimer();
	ApplyEffect(0.0f, CurrentColor);

	if (ManagedPostProcessComp.IsValid())
	{
		ManagedPostProcessComp->bEnabled = false;
	}

	bIsActive = false;
	CurrentStrength = 0.0f;
	TargetStrength = 0.0f;
}

FLinearColor UGS_PositiveEffectComponent::GetColorForType(EPositiveEffectType Type) const
{
	switch (Type)
	{
	case EPositiveEffectType::Heal:
		return HealColor;
	case EPositiveEffectType::AttackBuff:
		return AttackBuffColor;
	case EPositiveEffectType::DefenseBuff:
		return DefenseBuffColor;
	case EPositiveEffectType::SpeedBuff:
		return SpeedBuffColor;
	default:
		return HealColor;
	}
}

void UGS_PositiveEffectComponent::StartTimer()
{
	if (!OwnerActor.IsValid())
	{
		return;
	}

	if (UWorld* World = OwnerActor->GetWorld())
	{
		World->GetTimerManager().SetTimer(
		    UpdateTimerHandle,
		    this,
		    &UGS_PositiveEffectComponent::TickUpdate,
		    UpdateInterval,
		    true);
	}
}

void UGS_PositiveEffectComponent::StopTimer()
{
	if (!OwnerActor.IsValid())
	{
		return;
	}

	if (UWorld* World = OwnerActor->GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}
}

void UGS_PositiveEffectComponent::TickUpdate()
{
	// 액터가 파괴 중이거나 유효하지 않으면 즉시 중지
	if (!OwnerActor.IsValid() || OwnerActor->IsPendingKillPending())
	{
		StopTimer();
		return;
	}

	// 지속 시간 감소
	RemainingDuration -= UpdateInterval;

	// 페이드 인/아웃 로직 개선 (더 부드러운 감쇄)
	// 전체 지속 시간의 앞쪽 30%는 빠르게 페이드 인, 뒤쪽 70%는 부드럽게 페이드 아웃
	const float FadeOutPoint = EffectDuration * 0.7f;
	float CurrentTarget = 0.0f;
	float InterpFactor = 1.0f;

	if (RemainingDuration > FadeOutPoint)
	{
		// 페이드 인 구간: 설정된 속도로 빠르게 도달
		CurrentTarget = TargetStrength;
		InterpFactor = 1.5f; // 나타날 때는 더 선명하게
	}
	else
	{
		// 페이드 아웃 구간: 보간 속도를 대폭 줄여서 여운을 남김
		CurrentTarget = 0.0f;
		InterpFactor = 0.4f; // 사라질 때는 매우 부드럽게 (0.4배속)
	}

	CurrentStrength = FMath::FInterpTo(CurrentStrength, CurrentTarget, UpdateInterval, EffectInterpSpeed * InterpFactor);

	ApplyEffect(CurrentStrength, CurrentColor);

	// 효과 종료 체크 (매우 낮은 값까지 내려갔을 때 종료)
	if (RemainingDuration <= 0.0f && CurrentStrength < 0.01f)
	{
		bIsActive = false;

		if (ManagedPostProcessComp.IsValid())
		{
			ManagedPostProcessComp->bEnabled = false;
		}

		StopTimer();
	}
}

void UGS_PositiveEffectComponent::ApplyEffect(float Strength, const FLinearColor& Color)
{
	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(StrengthParamName, Strength);
		DynamicMaterial->SetVectorParameterValue(ColorParamName, Color);
	}
}

void UGS_PositiveEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopTimer();

	if (ManagedPostProcessComp.IsValid())
	{
		ManagedPostProcessComp->bEnabled = false;
		ManagedPostProcessComp->Settings.WeightedBlendables.Array.Empty();
	}

	Super::EndPlay(EndPlayReason);
}
