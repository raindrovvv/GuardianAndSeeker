// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Skill/Seeker/Chan/GS_ChanAimingSkill.h"
#include "Character/GS_Character.h"
#include "Character/Component/GS_DebuffComp.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Player/Guardian/GS_Guardian.h"
#include "Character/Debuff/EDebuffType.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Kismet/GameplayStatics.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Character/Player/GS_Player.h"
#include "AkAudioEvent.h"
#include "Sound/GS_SeekerAudioComponent.h"


UGS_ChanAimingSkill::UGS_ChanAimingSkill()
{
	CurrentSkillType = ESkillSlot::Aiming;
}

void UGS_ChanAimingSkill::ActiveSkill()
{
	// 스킬 상태 업데이트
	Super::ActiveSkill();
	
	// 쿨타임 측정 시작
	StartCoolDown();

	CachedChanOwner = Cast<AGS_Chan>(OwnerCharacter);

	if (CachedChanOwner.IsValid())
	{
		if (!CachedChanOwner->GetSkillComp()->IsSkillAllowed(ESkillSlot::Aiming))
		{
			UE_LOG(LogTemp, Warning, TEXT("Aiming Skill, OnSkillCommand, IsSkillAllowd is false"));
			return;
		}

		// 애니메이션 설정
		CachedChanOwner->Multicast_SetMustTurnInPlace(false);
		CachedChanOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::FullBody);

		// 입력 제한 설정
		CachedChanOwner->SetLookControlValue(false, false);
		CachedChanOwner->SetMoveControlValue(false, false);

		// bitmask flag
		CachedChanOwner->GetSkillComp()->SetCurAllowedSkillsMask(0);

		// Play Montage
		CachedChanOwner->Multicast_PlaySkillMontage(SkillAnimMontages[1]);

		// Set HitReact
		CachedChanOwner->SetCanHitReact(false);

		// Forward Jump
		const FVector Forward = CachedChanOwner->GetActorForwardVector();
		const FVector JumpVelocity = Forward * 600.0f + FVector(0.f, 0.f, 420.0f);
		CachedChanOwner->LaunchCharacter(JumpVelocity, true, true);

		// 스킬 시작 사운드 재생 (멀티캐스트)
		if (CachedChanOwner->HasAuthority())
		{
			if (UGS_SeekerAudioComponent* AudioComp = CachedChanOwner->SeekerAudioComponent)
			{
				AudioComp->RequestSkillAudio(CurrentSkillType, 0);
			}
		}

		// =======================
		// 스킬 범위 VFX 재생
		// =======================
		OwnerCharacter->GetWorld()->GetTimerManager().SetTimer(
			RangeVFXSpawnHandle,
			FTimerDelegate::CreateUObject(this, &UGS_ChanAimingSkill::SpawnAimingSkillVFX),
			0.93f,
			false);

		// 내려치기
		OwnerCharacter->GetWorld()->GetTimerManager().SetTimer(KnockbackHandle, this, &UGS_ChanAimingSkill::OnShieldSlam, 0.8f, false);
	}

}

void UGS_ChanAimingSkill::OnSkillCanceledByDebuff()
{
	Super::OnSkillCanceledByDebuff();

	// 방어 상태 비활성화
	if (CachedChanOwner.IsValid())
	{
		CachedChanOwner->SetDefending(false);
	}
}

void UGS_ChanAimingSkill::OnSkillAnimationEnd()
{
	Super::OnSkillAnimationEnd();

	if (CachedChanOwner.IsValid())
	{
		// Change Slot
		CachedChanOwner->Multicast_SetMustTurnInPlace(false);
		CachedChanOwner->SetSeekerGait(CachedChanOwner->GetLastSeekerGait());
		// Change Slot
		CachedChanOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::None);

		CachedChanOwner->CanChangeSeekerGait = true;
		
		CachedChanOwner->SetMoveControlValue(true, true);
		CachedChanOwner->SetLookControlValue(true, true);

		// 피격 애니메이션 재생 가능 설정
		//CachedChanOwner->SetCanHitReact(true);

		// SetIsActive(false); // 방어 상태를 유지하기 위해 제거

		// =======================
		// 스킬 종료 VFX 재생
		// =======================
		
		if (OwningComp)
		{
			FVector SkillLocation = OwnerCharacter->GetActorLocation();
			FRotator SkillRotation = OwnerCharacter->GetActorRotation();

			// 스킬 종료 VFX 재생
			OwningComp->Multicast_PlayEndVFX(CurrentSkillType, SkillLocation, SkillRotation);
		}

		CachedChanOwner->SetCanHitReact(true);
	}
}

void UGS_ChanAimingSkill::SpawnAimingSkillVFX()
{
	if (OwningComp&& OwnerCharacter)
	{
		const FVector Start = OwnerCharacter->GetActorLocation();
		const FVector Forward = OwnerCharacter->GetActorForwardVector();
		FVector SkillLocation = Start + Forward * 150.0f;
		const float Radius = 200.f;

		// 스킬 범위 표시 VFX 재생
		OwningComp->Multicast_PlayRangeVFX(CurrentSkillType, SkillLocation, Radius);
	}
}

void UGS_ChanAimingSkill::InterruptSkill()
{
	Super::InterruptSkill();

	if (CachedChanOwner.IsValid())
	{
		CachedChanOwner->SetLookControlValue(true, true);
		// 방어 상태 비활성화 (스킬이 중단될 때)
		CachedChanOwner->SetDefending(false);
	}
	
	SetIsActive(false);

	//OwnerPlayer->SetCurrentStamina(0.f);
	//OwnerPlayer->Client_ChanAimingSkillBar(false);
}


void UGS_ChanAimingSkill::OnShieldSlam()
{
	// 방패 충돌
	TArray<FHitResult> HitResults;

	const FVector Start = OwnerCharacter->GetActorLocation();
	const FVector Forward = OwnerCharacter->GetActorForwardVector();
	FVector SkillLocation = Start + Forward * 150.0f;
	const float Radius = 200.f;

	FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerCharacter);

	// 스킬 범위 표시(테스트)
	if (CachedChanOwner.IsValid())
	{
		CachedChanOwner->Multicast_DrawSkillRange(SkillLocation, Radius, FColor::Red, 1.0f);

		// 방패 슬램 충돌 사운드 재생
		if (UGS_SeekerAudioComponent* AudioComp = CachedChanOwner->SeekerAudioComponent)
		{
			AudioComp->PlayShieldSlamImpactSound();
		}
	}

	if (OwnerCharacter->GetWorld()->SweepMultiByChannel(HitResults, SkillLocation, SkillLocation, FQuat::Identity, ECC_Pawn, Shape, Params))
	{
		TSet<AActor*> HitActors;

		for (const FHitResult& Hit : HitResults)
		{
			// 충돌체
			AActor* HitActor = Hit.GetActor();
			UPrimitiveComponent* HitComponent = Hit.GetComponent();

			// 중복된 충돌 액터 걸러내기
			if (!HitActor || HitActors.Contains(HitActor))
			{
				continue;
			}

			// 사운드 트리거 무시
			if (HitComponent && HitComponent->GetCollisionProfileName() == FName("SoundTrigger"))
			{
				continue;
			}

			// 충돌 액터 저장
			HitActors.Add(HitActor);

			// 충돌 효과 활성화
			if (AGS_Monster* TargetMonster = Cast<AGS_Monster>(HitActor)) // 몬스터일 경우
			{
				ApplyEffectToDungeonMonster(TargetMonster);
				// Impact VFX 재생
				TargetMonster->PlayImpactVFX(SkillImpactVFX, SkillVFXScale);
			}
			else if (AGS_Guardian* TargetGuardian = Cast<AGS_Guardian>(HitActor)) // 가디언일 경우
			{
				ApplyEffectToGuardian(TargetGuardian);
				// Impact VFX 재생
				TargetGuardian->PlayImpactVFX(SkillImpactVFX, SkillVFXScale);
			}
			else if (AGS_Character* Target = Cast<AGS_Character>(HitActor)) // 시커일 경우
			{
				// 넉백
				const FVector LaunchDirection = (Target->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal();
				Target->LaunchCharacter(LaunchDirection * 1000.0f + FVector(0, 0, 500.0f), true, true);
			}
		}
	}

	// Set HitReact
	//OwnerPlayer->SetCanHitReact(true);

	// 방어 상태 해제 (방패 슬램 실행 시)
	if (CachedChanOwner.IsValid())
	{
		CachedChanOwner->SetDefending(false);
	}

	// 스킬 종료
	DeactiveSkill();
}

void UGS_ChanAimingSkill::StartHoldUp()
{
	if (CachedChanOwner.IsValid())
	{
		// 스테미나 초기화
		//CachedChanOwner->ResetCurrentStamina();
		CachedChanOwner->SetDefending(true);

		// UI 표시
		//CachedChanOwner->Client_ChanAimingSkillBar(true);
	}
}

void UGS_ChanAimingSkill::ApplyEffectToDungeonMonster(AGS_Monster* Target)
{
	if (!Target) 
	{
		return;
	}

	// 경직 디버프
	if (UGS_DebuffComp* DebuffComp = Target->FindComponentByClass<UGS_DebuffComp>())
	{
		Target->GetDebuffComp()->ApplyDebuff(EDebuffType::Stun, OwnerCharacter);
	}

	// 넉백
	const FVector LaunchDirection = (Target->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal();
	Target->LaunchCharacter(LaunchDirection * 1000.0f + FVector(0, 0, 500.0f), true, true);

	// 데미지
	UGameplayStatics::ApplyDamage(Target, Damage, OwnerCharacter->GetController(), OwnerCharacter, UDamageType::StaticClass());
}

void UGS_ChanAimingSkill::ApplyEffectToGuardian(AGS_Guardian* Target)
{
	// 데미지
	UGameplayStatics::ApplyDamage(Target, Damage, OwnerCharacter->GetController(), OwnerCharacter, UDamageType::StaticClass());

	// 경직 디버프
	if (UGS_DebuffComp* DebuffComp = Target->FindComponentByClass<UGS_DebuffComp>())
	{
		Target->GetDebuffComp()->ApplyDebuff(EDebuffType::Stun, OwnerCharacter);
	}
}

void UGS_ChanAimingSkill::DeactiveSkill()
{
	if (CachedChanOwner.IsValid())
	{
		// Set HitReact
		CachedChanOwner->CanChangeSeekerGait = true;
		CachedChanOwner->SetSeekerGait(EGait::Run);
		//CachedChanOwner->SetCanHitReact(true);

		// 방어 상태 비활성화 (스킬 완전 종료 시)
		CachedChanOwner->SetDefending(false);
	}

	// 스킬 상태 업데이트
	Super::DeactiveSkill();
}
