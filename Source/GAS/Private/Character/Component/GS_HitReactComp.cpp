// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Component/GS_HitReactComp.h"

#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Character/Player/GS_Player.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Skill/GS_SkillBase.h"
#include "Weapon/Equipable/GS_WeaponAxe.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Character/Skill/Seeker/GS_HealSkill.h"


// Sets default values for this component's properties
UGS_HitReactComp::UGS_HitReactComp()
{
	PrimaryComponentTick.bCanEverTick = false;

	AM_HitReacts.Init(nullptr, static_cast<int>(EHitReactType::TypeNum));
}

void UGS_HitReactComp::PlayHitReact(EHitReactType ReactType, FVector HitDirection)
{
	FName Section = CalculateHitDirection(HitDirection);
	AGS_Player* OwnerCharacter = Cast<AGS_Player>(GetOwner());
	AGS_Seeker* OwnerSeeker = Cast<AGS_Seeker>(OwnerCharacter);
	if (OwnerCharacter)
	{
		// ============================================
		// Hit React Cooldown System
		// ============================================
		float CurrentTime = GetWorld()->GetTimeSeconds();

		// Interrupt 타입이고 쿨다운 시간 내라면 DamageOnly로 변경
		if (ReactType == EHitReactType::Interrupt &&
			(CurrentTime - LastHitReactTime) < HitReactCooldown)
		{
			ReactType = EHitReactType::DamageOnly;
		}

		// 메르시 궁극기 상태인지 확인
		bool bIsMerciUltimate = false;
		if (OwnerSeeker && OwnerSeeker->IsMerci())
		{
			if (OwnerSeeker->GetSkillComp()->IsSkillActive(ESkillSlot::Ultimate))
			{
				bIsMerciUltimate = true;
			}
		}

		if (ReactType == EHitReactType::Interrupt)
		{			
			if (OwnerSeeker)
			{
				// 메르시 궁극기 중에는 인터럽트 무시 (슈퍼아머 효과)
				if (!bIsMerciUltimate)
				{
					OwnerSeeker->GetSkillComp()->SkillsInterrupt();

					OwnerSeeker->Multicast_SetMontageSlot(ESeekerMontageSlot::FullBody);

					UAnimMontage* AM_HitReact = AM_HitReacts[static_cast<int>(ReactType)];
					if (AM_HitReact)
					{
						OwnerCharacter->Multicast_PlaySkillMontage(AM_HitReact, Section);

						UGS_SeekerAnimInstance* SeekerAnimInstance = Cast<UGS_SeekerAnimInstance>(OwnerSeeker->GetMesh()->GetAnimInstance());
						if (SeekerAnimInstance)
						{
							HitReactEndDelegate.BindUObject(this, &UGS_HitReactComp::OnEndDelegate);
							SeekerAnimInstance->Montage_SetEndDelegate(HitReactEndDelegate, AM_HitReact);
						}
					}

					// 피격모션 재생 시간 기록 (쿨다운용)
					LastHitReactTime = CurrentTime;
				}
			}
			OwnerCharacter->DisableHitReact(3.0f);
		}
		else if (ReactType == EHitReactType::Additive)
		{
			if (OwnerSeeker && !bIsMerciUltimate)
			{
				OwnerSeeker->StateReset();
				OwnerSeeker->SetSeekerGait(EGait::Run);
			}
		}
		else if (ReactType == EHitReactType::DamageOnly)
		{
			if (OwnerSeeker)
			{
				// 단순 데미지만 입을 때는 상태를 초기화하지 않음
			}
		}


		// 메르시 궁극기 중이 아니며, 단순 데미지 피격이 아닐 때만 활 조준 상태를 해제함
		if (OwnerSeeker && !bIsMerciUltimate && ReactType != EHitReactType::DamageOnly)
		{
			OwnerSeeker->SetAimState(false);
			OwnerSeeker->SetDrawState(false);
		}
	}
}


void UGS_HitReactComp::StopHitReact(UAnimMontage* TargetMontage)
{
	if (AGS_Player* OwnerCharacter = Cast<AGS_Player>(GetOwner()))
	{
		OwnerCharacter->Multicast_StopSkillMontage(TargetMontage);
	}
}

FName UGS_HitReactComp::CalculateHitDirection(FVector HitDirection)
{
	FName Section = NAME_None;
	
	if (AGS_Player* OwnerCharacter = Cast<AGS_Player>(GetOwner()))
	{
		FVector Front = OwnerCharacter->GetActorRotation().Vector();
		FVector Right = OwnerCharacter->GetActorRightVector();

		float FrontDot = FVector::DotProduct(Front, HitDirection);
		float RightDot = FVector::DotProduct(Right, HitDirection);

		if (FrontDot > 0.7f)
		{
			Section = FName("Front");
		}
		else if (FrontDot < -0.7f)
		{
			Section = FName("Back");
		}
		else if (RightDot > 0.0f)
		{
			Section = FName("Right");
		}
		else
		{
			Section = FName("Left");
		}
	}
	
	return Section;
}

void UGS_HitReactComp::OnEndDelegate(UAnimMontage* Montage, bool bInterrupted)
{
	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(GetOwner()))
	{
		// HitReact 애니메이션 종료 후 상태 복구
		Seeker->StateReset();
		Seeker->SetSeekerGait(EGait::Run);

		UGS_HealSkill* HealSkill = Cast<UGS_HealSkill>(Seeker->GetSkillComp()->GetSkillFromSkillMap(ESkillSlot::HealPotion));
		if (HealSkill)
		{
			UAnimMontage* AM_Wielding = HealSkill->SkillAnimMontages[2];

			if (AM_Wielding)
			{
				Seeker->TransWeaponHandingState(
				EWeaponHandlingState::Sheathing,
				EWeaponHandlingState::Wielding,
				AM_Wielding,
				ESeekerMontageSlot::UpperBody);
			}
		}
	}
}

// Called when the game starts
void UGS_HitReactComp::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

