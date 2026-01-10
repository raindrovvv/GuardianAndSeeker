// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Debuff/GS_DebuffStun.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/GS_Character.h"
#include "Character/GS_TpsController.h"
#include "AI/GS_AIController.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Skill/GS_SkillComp.h"

void UGS_DebuffStun::OnApply()
{
	Super::OnApply();
	if (!TargetCharacter->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("Is Not Server OnApply for %s"), *TargetCharacter->GetName());
	}

	if (TargetCharacter)
	{
		// 움직임도 멈춤(가디언)
		if (AGS_TpsController* Controller = Cast<AGS_TpsController>(TargetCharacter->GetController()))
		{
			Controller->SetMoveControlValue(false, false);
		}
		else if (AGS_AIController* AI = Cast<AGS_AIController>(TargetCharacter->GetController()))
		{
			MaxSpeed = TargetCharacter->GetCharacterMovement()->MaxWalkSpeed;
			TargetCharacter->GetCharacterMovement()->MaxWalkSpeed = 0.0f;
			AGS_Monster* Monster = Cast<AGS_Monster>(TargetCharacter);
			if (Monster)
			{
				Monster->ApplyStiffness();
			}
		}

		// Interrupt current skills and reset mask to prevent lock-outs
		if (UGS_SkillComp* SkillComp = TargetCharacter->FindComponentByClass<UGS_SkillComp>())
		{
			SkillComp->SkillsInterrupt();
			SkillComp->ResetAllowedSkillsMask();
		}

		// 스킬 못쓰게 설정
		TargetCharacter->SetCanUseSkill(false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Apply Stun Debuff Error - TargetCharacter is null"));
	}
}

void UGS_DebuffStun::OnExpire()
{
	if (!TargetCharacter->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("Is Not Server OnExpiree for %s"), *TargetCharacter->GetName());
	}

	if (TargetCharacter)
	{
		// 움직임(가디언)
		if (AGS_TpsController* Controller = Cast<AGS_TpsController>(TargetCharacter->GetController()))
		{
			Controller->SetMoveControlValue(true, true);
		}
		else
		{
			TargetCharacter->GetCharacterMovement()->MaxWalkSpeed = MaxSpeed;
		}

		AGS_Monster* Monster = Cast<AGS_Monster>(TargetCharacter);
		if (Monster)
		{
			Monster->EndStiffness();
		}

		// 스킬 사용 가능
		TargetCharacter->SetCanUseSkill(true);

		// 🔴 CRITICAL: Reset skill mask to restore all skills after stun expires
		if (UGS_SkillComp* SkillComp = TargetCharacter->FindComponentByClass<UGS_SkillComp>())
		{
			SkillComp->ResetAllowedSkillsMask();
			UE_LOG(LogTemp, Log, TEXT("[Stun] Reset skill mask for %s (Mask: %d)"),
				*TargetCharacter->GetName(), SkillComp->GetCurAllowedSkillsMask());
		}

		UE_LOG(LogTemp, Warning, TEXT("Stun expired for %s"), *TargetCharacter->GetName());
	}

	Super::OnExpire();
}
