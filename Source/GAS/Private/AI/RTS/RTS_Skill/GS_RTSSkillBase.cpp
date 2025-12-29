// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/RTS/RTS_Skill/GS_RTSSkillBase.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillComponent.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillData.h"
#include "AI/RTS/GS_RTSController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

UGS_RTSSkillBase::UGS_RTSSkillBase()
	: SkillSlotIndex(-1)
{
	SkillData = nullptr;
}

FText UGS_RTSSkillBase::GetSkillName() const
{
	return SkillData ? SkillData->SkillName : FText::GetEmpty();
}

FText UGS_RTSSkillBase::GetSkillDescription() const
{
	return SkillData ? SkillData->SkillDescription : FText::GetEmpty();
}

UTexture2D* UGS_RTSSkillBase::GetSkillIcon() const
{
	return SkillData ? SkillData->SkillIcon.Get() : nullptr;
}

float UGS_RTSSkillBase::GetAetherCost() const
{
	return SkillData ? SkillData->AetherCost : 0.f;
}

float UGS_RTSSkillBase::GetCooldownTime() const
{
	return SkillData ? SkillData->CooldownTime : 0.f;
}

ERTSSkillTargetType UGS_RTSSkillBase::GetTargetType() const
{
	return SkillData ? SkillData->TargetType : ERTSSkillTargetType::None;
}

float UGS_RTSSkillBase::GetSkillRange() const
{
	return SkillData ? SkillData->SkillRange : 0.f;
}

float UGS_RTSSkillBase::GetEffectRadius() const
{
	return SkillData ? SkillData->EffectRadius : 0.f;
}

float UGS_RTSSkillBase::GetSkillPower() const
{
	return SkillData ? SkillData->SkillPower : 0.f;
}

float UGS_RTSSkillBase::GetEffectDuration() const
{
	return SkillData ? SkillData->EffectDuration : 0.f;
}

UNiagaraSystem* UGS_RTSSkillBase::GetActivationVFX() const
{
	// Soft Reference 로드
	if (SkillData && !SkillData->ActivationVFX.IsNull())
	{
		return SkillData->ActivationVFX.LoadSynchronous();
	}
	return nullptr;
}

UAkAudioEvent* UGS_RTSSkillBase::GetCastSound() const
{
	if (!SkillData)
	{
		return nullptr;
	}

	// Soft Reference 로드
	UAkAudioEvent* TPSSound = SkillData->CastSound_TPS.IsNull() ? nullptr : SkillData->CastSound_TPS.LoadSynchronous();
	UAkAudioEvent* RTSSound = SkillData->CastSound_RTS.IsNull() ? nullptr : SkillData->CastSound_RTS.LoadSynchronous();

	return SelectSoundEvent(TPSSound, RTSSound);
}

void UGS_RTSSkillBase::SetSkillData(UGS_RTSSkillData* InSkillData)
{
	SkillData = InSkillData;
}

void UGS_RTSSkillBase::Initialize(UGS_RTSSkillComponent* OwnerComponent)
{
	OwnerSkillComponent = OwnerComponent;
}

bool UGS_RTSSkillBase::CanActivate(UGS_RTSSkillComponent* SkillComponent) const
{
	if (!SkillComponent || !SkillData)
	{
		return false;
	}

	// 기본적으로 에테르 체크
	return SkillComponent->GetCurrentAether() >= GetAetherCost();
}

FVector UGS_RTSSkillBase::ActivateSkill(UGS_RTSSkillComponent* SkillComponent, const FVector& TargetLocation)
{
	// 블루프린트 이벤트 호출
	BP_OnSkillActivated(TargetLocation);

	// 기본적으로는 타겟 위치 그대로 반환
	return TargetLocation;
}

void UGS_RTSSkillBase::PlayCastEffects(const FVector& TargetLocation)
{
	if (UNiagaraSystem* VFX = GetActivationVFX())
	{
		PlaySkillVFX(VFX, TargetLocation);
	}

	if (UAkAudioEvent* Sound = GetCastSound())
	{
		PlaySkillSound(Sound, TargetLocation);
	}
}

AGS_RTSController* UGS_RTSSkillBase::GetRTSController() const
{
	if (OwnerSkillComponent.IsValid())
	{
		return Cast<AGS_RTSController>(OwnerSkillComponent->GetOwner());
	}
	return nullptr;
}

UWorld* UGS_RTSSkillBase::GetSkillWorld() const
{
	if (OwnerSkillComponent.IsValid())
	{
		return OwnerSkillComponent->GetWorld();
	}
	return nullptr;
}

void UGS_RTSSkillBase::PlaySkillVFX(UNiagaraSystem* NiagaraSystem, const FVector& Location, const FRotator& Rotation)
{
	if (!NiagaraSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaySkillVFX: NiagaraSystem is null"));
		return;
	}

	UWorld* World = GetSkillWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaySkillVFX: World is null"));
		return;
	}

	// 기본 파라미터로 스폰 시도
	UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		NiagaraSystem,
		Location,
		Rotation
	);
}

bool UGS_RTSSkillBase::IsRTSMode() const
{
	UWorld* World = GetSkillWorld();
	if (!World)
	{
		return false;
	}

	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(World, 0);
	if (!LocalPC)
	{
		return false;
	}

	// LocalPC가 RTS 컨트롤러인지 확인
	return Cast<AGS_RTSController>(LocalPC) != nullptr;
}

UAkAudioEvent* UGS_RTSSkillBase::SelectSoundEvent(UAkAudioEvent* TPSSound, UAkAudioEvent* RTSSound) const
{
	if (IsRTSMode())
	{
		// RTS 모드이고 RTS 사운드가 있으면 사용, 없으면 TPS 사운드 폴백
		return RTSSound ? RTSSound : TPSSound;
	}
	else
	{
		// TPS 모드이면 TPS 사운드 사용
		return TPSSound;
	}
}

void UGS_RTSSkillBase::PlaySkillSound(UAkAudioEvent* Sound, const FVector& Location)
{
	if (!Sound)
	{
		return;
	}

	UWorld* World = GetSkillWorld();
	if (!World)
	{
		return;
	}

	UAkGameplayStatics::PostEventAtLocation(
		Sound,
		Location,
		FRotator::ZeroRotator,
		World
	);
}
