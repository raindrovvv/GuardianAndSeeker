// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/RTS/RTS_Skill/GS_RTSSkillBase.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillComponent.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillData.h"
#include "AI/RTS/GS_RTSController.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"

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

UParticleSystem* UGS_RTSSkillBase::GetActivationVFX() const
{
	return SkillData ? SkillData->ActivationVFX.Get() : nullptr;
}

USoundBase* UGS_RTSSkillBase::GetCastSound() const
{
	return SkillData ? SkillData->CastSound.Get() : nullptr;
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

void UGS_RTSSkillBase::ActivateSkill(UGS_RTSSkillComponent* SkillComponent, const FVector& TargetLocation)
{
	// 블루프린트 이벤트 호출
	BP_OnSkillActivated(TargetLocation);
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

void UGS_RTSSkillBase::PlaySkillVFX(UParticleSystem* ParticleSystem, const FVector& Location, const FRotator& Rotation)
{
	if (!ParticleSystem)
	{
		return;
	}

	UWorld* World = GetSkillWorld();
	if (!World)
	{
		return;
	}

	UGameplayStatics::SpawnEmitterAtLocation(
		World,
		ParticleSystem,
		Location,
		Rotation,
		true
	);
}

void UGS_RTSSkillBase::PlaySkillSound(USoundBase* Sound, const FVector& Location)
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

	UGameplayStatics::PlaySoundAtLocation(
		World,
		Sound,
		Location
	);
}
