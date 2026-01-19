// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Component/Seeker/GS_ChanSkillInputHandlerComp.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

void UGS_ChanSkillInputHandlerComp::OnRightClick(const FInputActionInstance& Instance)
{
	AGS_Chan* ChanCharacter = Cast<AGS_Chan>(OwnerCharacter);

	if (OwnerCharacter->IsDead())
	{
		return;
	}

	Super::OnRightClick(Instance);

	if (!bCtrlHeld)
	{
		if (ChanCharacter->GetSkillComp()->IsSkillActive(ESkillSlot::Ready))
		{
			ChanCharacter->GetSkillComp()->Server_TryDeactiveSkill(ESkillSlot::Ready);
		}
		else
		{
			ChanCharacter->GetSkillComp()->Server_TryActivateSkill(ESkillSlot::Ready);
		}
	}
	else
	{
		ChanCharacter->GetSkillComp()->Server_TryActivateSkill(ESkillSlot::Ultimate);
	}
}

void UGS_ChanSkillInputHandlerComp::OnLeftClick(const FInputActionInstance& Instance)
{
	AGS_Chan* ChanCharacter = Cast<AGS_Chan>(OwnerCharacter);
	if (OwnerCharacter->IsDead())
	{
		return;
	}

	Super::OnLeftClick(Instance);

	if (!bCtrlHeld)
	{
		if (ChanCharacter)
		{
			if (ChanCharacter->GetSkillComp()->IsSkillActive(ESkillSlot::Ready))
			{
				ChanCharacter->GetSkillComp()->Server_TryActivateSkill(ESkillSlot::Aiming);
			}
			else
			{
				if (ChanCharacter->CanAcceptComboInput)
				{
					// 조작감 개선: 클라이언트에서 즉시 회전 보정
					ChanCharacter->PreAttackSnap();
					ChanCharacter->Server_ExecuteComboAttack();
				}
			}
		}
	}
	else
	{
		if (ChanCharacter)
		{
			ChanCharacter->GetSkillComp()->Server_TryActivateSkill(ESkillSlot::Moving);
		}
	}
}

void UGS_ChanSkillInputHandlerComp::OnRoll(const struct FInputActionInstance& Instance)
{
	AGS_Chan* ChanCharacter = Cast<AGS_Chan>(OwnerCharacter);

	Super::OnRoll(Instance);

	if (ChanCharacter)
	{
		ChanCharacter->GetSkillComp()->Server_TryActivateSkill(ESkillSlot::Rolling);
	}

	return;
}

void UGS_ChanSkillInputHandlerComp::OnKeyReset(const struct FInputActionInstance& Instance)
{
	Super::OnKeyReset(Instance);
}
