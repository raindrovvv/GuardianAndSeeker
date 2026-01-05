// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Skill/Seeker/Chan/GS_ChanMovingSkill.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Player/Guardian/GS_Guardian.h"
#include "Character/Component/GS_DebuffComp.h"
#include "Character/Debuff/EDebuffType.h"
#include "Character/Player/GS_Player.h"
#include "AkAudioEvent.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Component/GS_StatRow.h"
#include "Sound/GS_SeekerAudioComponent.h"
#include "NiagaraSystem.h"
#include "System/Utility/GS_AssetLoader.h"

UGS_ChanMovingSkill::UGS_ChanMovingSkill()
{
	CurrentSkillType = ESkillSlot::Moving;
}

void UGS_ChanMovingSkill::ActiveSkill()
{
	Super::ActiveSkill();

	// 스킬 쿨타임 측정 시작
	StartCoolDown();

	CachedChanOwner = Cast<AGS_Chan>(OwnerCharacter);

	if (CachedChanOwner.IsValid())
	{
		// 애니메이션 설정
		CachedChanOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::FullBody);

		// 입력 제한 설정
		CachedChanOwner->SetMoveControlValue(false, false);

		if (UAnimMontage* LoadedMontage = GetCachedMontage(0))
		{
			CachedChanOwner->Multicast_PlaySkillMontage(LoadedMontage);
		}

		// =======================
		// VFX 재생 - 컴포넌트 RPC 사용
		// =======================
		if (OwningComp)
		{
			FVector SkillLocation = OwnerCharacter->GetActorLocation();
			FRotator SkillRotation = OwnerCharacter->GetActorRotation();

			// 스킬 시전 VFX 재생
			OwningComp->Multicast_PlayCastVFX(CurrentSkillType, SkillLocation, SkillRotation);

			// 스킬 범위 표시 VFX 재생
			OwningComp->Multicast_PlayRangeVFX(CurrentSkillType, SkillLocation, 800.0f);
		}

		// 스킬 시작 사운드 재생 (멀티캐스트)
		if (OwnerCharacter->HasAuthority())
		{
			if (UGS_SeekerAudioComponent* AudioComp = CachedChanOwner->SeekerAudioComponent)
			{
				AudioComp->RequestSkillAudio(CurrentSkillType, 0);
			}
		}

		// 방어력 강화
		StrengthenDefense();

		// 어그로
		AggroToOwner();
	}
}

void UGS_ChanMovingSkill::OnSkillCanceledByDebuff()
{
	Super::OnSkillCanceledByDebuff();
}

void UGS_ChanMovingSkill::OnSkillAnimationEnd()
{
	Super::OnSkillAnimationEnd();

	if (CachedChanOwner.IsValid())
	{
		CachedChanOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::None);
		CachedChanOwner->SetMoveControlValue(true, true);
		CachedChanOwner->CanChangeSeekerGait = true;
	}

	SetIsActive(false);

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
}

void UGS_ChanMovingSkill::InterruptSkill()
{
	Super::InterruptSkill();

	if (CachedChanOwner.IsValid())
	{
		CachedChanOwner->GetWorldTimerManager().ClearTimer(DEFBuffHandle);
	}

	SetIsActive(false);
}

void UGS_ChanMovingSkill::ApplyEffectToDungeonMonster(AGS_Monster* Target)
{
	// 어그로 디버프
	if (UGS_DebuffComp* DebuffComp = Target->FindComponentByClass<UGS_DebuffComp>())
	{
		Target->GetDebuffComp()->ApplyDebuff(EDebuffType::Aggro, OwnerCharacter);
	}
}

void UGS_ChanMovingSkill::ApplyEffectToGuardian(AGS_Guardian* Target)
{
	// 뮤트 디버프
	if (UGS_DebuffComp* DebuffComp = Target->FindComponentByClass<UGS_DebuffComp>())
	{
		Target->GetDebuffComp()->ApplyDebuff(EDebuffType::Mute, OwnerCharacter);
	}
}

void UGS_ChanMovingSkill::DeactiveSkill()
{
	// 방어력 증가 디버프 해제
	DeactiveDEFBuff();

	// 스킬 종료 사운드 재생 (멀티캐스트)
	if (OwnerCharacter->HasAuthority())
	{
		if (AGS_Seeker* OwnerSeeker = Cast<AGS_Seeker>(OwnerCharacter))
		{
			if (UGS_SeekerAudioComponent* AudioComp = OwnerSeeker->SeekerAudioComponent)
			{
				AudioComp->RequestSkillAudio(CurrentSkillType, 1);
			}
		}
	}

	// 스킬 상태 업데이트
	Super::DeactiveSkill();
}

void UGS_ChanMovingSkill::AggroToOwner()
{
	// 범위 내 몬스터 인식 및 효과 활성화
	TArray<FHitResult> HitResults;

	const FVector Center = OwnerCharacter->GetActorLocation(); // 중심은 캐릭터
	const float Radius = 800.0f;

	FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerCharacter);

	AGS_Chan* OwnerPlayer = CachedChanOwner.Get();
	if (OwnerPlayer)
	{
		OwnerPlayer->Multicast_DrawSkillRange(Center, Radius, FColor::Red, 2.0f);
	}
	// 캐릭터를 중심으로 한 지점에 고정된 SphereOverlap
	if (GetWorld()->SweepMultiByChannel(HitResults, Center, Center, FQuat::Identity, ECC_Pawn, Shape, Params))
	{
		TSet<AActor*> HitActors;

		for (const FHitResult& Hit : HitResults)
		{
			AActor* HitActor = Hit.GetActor();
			UPrimitiveComponent* HitComponent = Hit.GetComponent();

			if (!HitActor || HitActors.Contains(HitActor))
			{
				continue;
			}

			// SoundTrigger 콜리전 프로파일을 가진 컴포넌트 제외
			if (HitComponent && HitComponent->GetCollisionProfileName() == FName("SoundTrigger"))
			{
				continue;
			}

			HitActors.Add(HitActor);

			// === 효과 즉시 적용 (서버) ===
			// 디버프 등 게임플레이에 영향을 주는 수치는 에셋 로드와 상관없이 즉시 적용.
			if (AGS_Monster* TargetMonster = Cast<AGS_Monster>(HitActor))
			{
				ApplyEffectToDungeonMonster(TargetMonster);
			}
			else if (AGS_Guardian* TargetGuardian = Cast<AGS_Guardian>(HitActor))
			{
				ApplyEffectToGuardian(TargetGuardian);
			}

			// === VFX 비동기 로드 및 재생 ===
			TWeakObjectPtr<AActor> WeakHitActor(HitActor);
			TWeakObjectPtr<UGS_ChanMovingSkill> WeakThis(this);

			// 이미 로드된 에셋이 있다면 즉시 재생 (성능 최적화)
			if (UNiagaraSystem* CachedVFX = SkillImpactVFX.Get())
			{
				if (AGS_Character* TargetChar = Cast<AGS_Character>(HitActor))
				{
					TargetChar->PlayImpactVFX(CachedVFX, SkillVFXScale);
				}
				continue;
			}

			UGS_AssetLoader::AsyncLoadAsset<UNiagaraSystem>(
			    SkillImpactVFX,
			    [WeakThis, WeakHitActor](UNiagaraSystem* LoadedImpactVFX)
			    {
				    if (!WeakThis.IsValid() || !WeakHitActor.IsValid() || !LoadedImpactVFX)
				    {
					    return;
				    }

				    if (AGS_Character* TargetChar = Cast<AGS_Character>(WeakHitActor.Get()))
				    {
					    // Impact VFX 재생 (이미 효과는 위에서 적용됨)
					    TargetChar->PlayImpactVFX(LoadedImpactVFX, WeakThis->SkillVFXScale);
				    }
			    });
		}
	}
}

void UGS_ChanMovingSkill::StrengthenDefense()
{
	// 방어력 증가
	if (UGS_StatComp* StatComp = OwnerCharacter->GetStatComp())
	{
		FGS_StatRow BuffStat;
		BuffStat.DEF = ExtraDefense; // 방어력 *2(+200.0f)

		BuffAmount = BuffStat; // 나중에 되돌릴 때 사용할 변수
		StatComp->ChangeStat(BuffStat);
	}

	OwnerCharacter->GetWorld()->GetTimerManager().SetTimer(DEFBuffHandle, this, &UGS_ChanMovingSkill::DeactiveSkill, StrengthenDefenseDuration, false);
}

void UGS_ChanMovingSkill::DeactiveDEFBuff()
{
	// 방어력 강화 버프 리셋
	if (UGS_StatComp* StatComp = OwnerCharacter->GetStatComp())
	{
		StatComp->ResetStat(BuffAmount);
	}
}
