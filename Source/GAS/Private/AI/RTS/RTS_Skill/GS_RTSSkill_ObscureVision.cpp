// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/RTS/RTS_Skill/GS_RTSSkill_ObscureVision.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillComponent.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillData.h"
#include "AI/RTS/GS_RTSController.h"
#include "Character/Debuff/GS_DebuffObscure.h"
#include "Character/Debuff/EDebuffType.h"
#include "Character/Component/GS_DebuffComp.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"

UGS_RTSSkill_ObscureVision::UGS_RTSSkill_ObscureVision()
{
}

bool UGS_RTSSkill_ObscureVision::CanActivate(UGS_RTSSkillComponent* SkillComponent) const
{
	if (!Super::CanActivate(SkillComponent))
	{
		return false;
	}

	// 살아있는 시커가 있는지 확인
	UWorld* World = SkillComponent ? SkillComponent->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
	{
		const TArray<TWeakObjectPtr<AGS_Seeker>>& Seekers = Registry->GetSeekers();
		for (const TWeakObjectPtr<AGS_Seeker>& SeekerPtr : Seekers)
		{
			AGS_Seeker* Seeker = SeekerPtr.Get();
			if (IsValid(Seeker) && !Seeker->IsDead())
			{
				return true;  // 살아있는 시커가 최소 1명 있음
			}
		}
	}

	return false;  // 살아있는 시커 없음
}

FVector UGS_RTSSkill_ObscureVision::ActivateSkill(UGS_RTSSkillComponent* SkillComponent, const FVector& TargetLocation)
{
	Super::ActivateSkill(SkillComponent, TargetLocation);

	// 서버에서만 실행
	if (!SkillComponent || !SkillComponent->GetOwner()->HasAuthority())
	{
		return TargetLocation;
	}

	ApplyObscureToAllSeekers();

	// 시전 VFX (RTS 화면에서 보여줄 이펙트)
	if (UNiagaraSystem* ActivationVFX = GetActivationVFX())
	{
		if (AGS_RTSController* RTSController = GetRTSController())
		{
			const FVector CameraLocation = RTSController->GetPawn() ? RTSController->GetPawn()->GetActorLocation() : FVector::ZeroVector;
			PlaySkillVFX(ActivationVFX, CameraLocation);
		}
	}

	// 시전 사운드 재생
	if (UAkAudioEvent* CastSound = GetCastSound())
	{
		if (UWorld* World = GetSkillWorld())
		{
			// RTS 컨트롤러의 Pawn 위치나 ZeroVector에서 재생
			FVector SoundLocation = FVector::ZeroVector;
			if (AGS_RTSController* RTSController = GetRTSController())
			{
				SoundLocation = RTSController->GetPawn() ? RTSController->GetPawn()->GetActorLocation() : FVector::ZeroVector;
			}
			UAkGameplayStatics::PostEventAtLocation(CastSound, SoundLocation, FRotator::ZeroRotator, World);
		}
	}

	return TargetLocation;
}

void UGS_RTSSkill_ObscureVision::ApplyObscureToAllSeekers()
{
	UWorld* World = GetSkillWorld();
	if (!World)
	{
		return;
	}

	const UGS_RTSSkillData_ObscureVision* ObscureData = GetObscureVisionData();
	UAkAudioEvent* ActivateSound = nullptr;
	if (ObscureData)
	{
		ActivateSound = SelectSoundEvent(ObscureData->ObscureActivateSound_TPS, ObscureData->ObscureActivateSound_RTS);
	}

	const float ObscureDuration = GetEffectDuration();

	int32 AffectedCount = 0;

	// 월드 내 모든 시커 순회
	if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
	{
		const TArray<TWeakObjectPtr<AGS_Seeker>>& Seekers = Registry->GetSeekers();
		for (const TWeakObjectPtr<AGS_Seeker>& SeekerPtr : Seekers)
		{
			AGS_Seeker* Seeker = SeekerPtr.Get();
			if (!IsValid(Seeker) || Seeker->IsDead())
			{
				continue;
			}

			// 디버프 컴포넌트 가져오기
			UGS_DebuffComp* DebuffComp = Seeker->GetDebuffComp();
			if (DebuffComp)
			{
				// 기존 DebuffComp를 통해 디버프 적용
				DebuffComp->ApplyDebuff(EDebuffType::Obscure, nullptr);
				++AffectedCount;
			}
			else
			{
				// 디버프 컴포넌트가 없는 경우 직접 호출
				Seeker->Client_StartVisionObscured();

				// 지속 시간 후 해제하는 타이머 설정
				FTimerHandle TimerHandle;
				World->GetTimerManager().SetTimer(
					TimerHandle,
					[WeakSeeker = TWeakObjectPtr<AGS_Seeker>(Seeker)]()
					{
						if (WeakSeeker.IsValid())
						{
							WeakSeeker->Client_StopVisionObscured();
						}
					},
					ObscureDuration,
					false
				);

				++AffectedCount;
			}

			// 개별 시커에게 활성화 사운드 재생
			if (ActivateSound)
			{
				PlaySkillSound(ActivateSound, Seeker->GetActorLocation());
			}
		}
	}
}

const UGS_RTSSkillData_ObscureVision* UGS_RTSSkill_ObscureVision::GetObscureVisionData() const
{
	return Cast<UGS_RTSSkillData_ObscureVision>(GetSkillData());
}
