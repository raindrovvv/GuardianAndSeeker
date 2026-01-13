#include "UI/Character/GS_SkillWidget.h"
#include "Character/Player/GS_Player.h"
#include "Character/Skill/GS_SkillBase.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Character/Skill/Seeker/GS_HealSkill.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UGS_SkillWidget::UGS_SkillWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UGS_SkillWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			OwningCharacter = Cast<AGS_Player>(Pawn);
			SetOwningActor(OwningCharacter);

			if (IsValid(OwningCharacter) && OwningCharacter->GetSkillComp())
			{
				OwningCharacter->GetSkillComp()->InitializeSkillWidget(this);
			}
		}
	}
}

void UGS_SkillWidget::NativeDestruct()
{
	if (IsValid(OwningCharacter) && OwningCharacter->GetSkillComp())
	{
		UGS_SkillComp* SkillComp = OwningCharacter->GetSkillComp();
		SkillComp->OnSkillCooldownChanged.RemoveAll(this);
		SkillComp->OnHealCountChanged.RemoveAll(this);
		SkillComp->OnSkillActivated.RemoveDynamic(this, &UGS_SkillWidget::OnSkillActivated);
		SkillComp->OnSkillCooldownBlocked.RemoveDynamic(this, &UGS_SkillWidget::OnSkillCooldownBlocked);
		SkillComp->OnSkillCooldownReady.RemoveDynamic(this, &UGS_SkillWidget::OnSkillCooldownReady);
	}

	Super::NativeDestruct();
}

void UGS_SkillWidget::InitSkill(UGS_SkillBase* Skill)
{
	if (Skill)
	{
		OnSkillCoolTimeChanged(SkillSlot, 0.f);
		CurrentCoolTimeText->SetVisibility(ESlateVisibility::Hidden);
		SkillImage->SetBrushFromTexture(Skill->GetSkillImage());

		if (SkillSlot == ESkillSlot::HealPotion)
		{
			CoolTimeBar->SetVisibility(ESlateVisibility::Hidden);

			// 힐 스킬일 때만 카운트 텍스트 표시
			if (UGS_HealSkill* HealSkill = Cast<UGS_HealSkill>(Skill))
			{
				HealCountText->SetVisibility(ESlateVisibility::Visible);
				int32 CurrentCount = HealSkill->GetCurrentHealCount();
				int32 MaxCount = HealSkill->GetMaxHealCount();
				HealCountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), CurrentCount)));
			}
			else
			{
				HealCountText->SetVisibility(ESlateVisibility::Hidden);
			}
		}
		else
		{
			HealCountText->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UGS_SkillWidget::OnSkillActivated(ESkillSlot InSkillSlot)
{
	if (InSkillSlot == SkillSlot)
	{
		PlayHeartbeatAnimation();
		PlayGlowEffect();

		if (IsUltimateSkillSlot())
		{
			PlayUltimateSkillActivationSound();
		}
		else
		{
			PlaySkillActivationSound();
		}
	}
}

void UGS_SkillWidget::OnSkillCoolTimeChanged(ESkillSlot InSkillSlot, float InCurrentCoolTime) const
{
	if (InSkillSlot != SkillSlot)
	{
		return;
	}

	float CoolTime = OwningCharacter->GetSkillComp()->GetSkillFromSkillMap(SkillSlot)->GetCoolTime();

	//finish skill
	if (InCurrentCoolTime < KINDA_SMALL_NUMBER)
	{
		CurrentCoolTimeText->SetVisibility(ESlateVisibility::Hidden);
		CoolTimeBar->SetVisibility(ESlateVisibility::Hidden);
	}
	//start skill
	else
	{
		CurrentCoolTimeText->SetVisibility(ESlateVisibility::Visible);
		CoolTimeBar->SetVisibility(ESlateVisibility::Visible);
		CurrentCoolTimeText->SetText(FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(InCurrentCoolTime))));
		CoolTimeBar->SetPercent(InCurrentCoolTime / CoolTime);
	}
}

void UGS_SkillWidget::OnHealCountChanged(ESkillSlot InSkillSlot, int32 CurrentCount, int32 MaxCount)
{
	if (SkillSlot != InSkillSlot)
		return;

	HealCountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), CurrentCount)));

	if (CurrentCount <= 0)
	{
		PlayCooldownBlockedAnimation();
		PlayRedFlashEffect();
		PlaySkillCooldownSound();
	}
}

void UGS_SkillWidget::PlaySkillActivationSound()
{
	if (bEnableAudio && SkillActivationSound)
	{
		UGameplayStatics::PlaySound2D(this, SkillActivationSound, AudioVolume);
	}
}

void UGS_SkillWidget::OnSkillCooldownBlocked(ESkillSlot InSkillSlot)
{
	if (InSkillSlot == SkillSlot)
	{
		PlayCooldownBlockedAnimation();
		PlayRedFlashEffect();

		if (IsUltimateSkillSlot())
		{
			PlayUltimateSkillCooldownSound();
		}
		else
		{
			PlaySkillCooldownSound();
		}
	}
}

void UGS_SkillWidget::PlaySkillCooldownSound()
{
	if (bEnableAudio && SkillCooldownSound)
	{
		UGameplayStatics::PlaySound2D(this, SkillCooldownSound, AudioVolume);
	}
}

bool UGS_SkillWidget::IsUltimateSkillSlot() const
{
	return SkillSlot == ESkillSlot::Ultimate;
}

void UGS_SkillWidget::PlayUltimateSkillActivationSound()
{
	if (bEnableAudio && UltimateSkillActivationSound)
	{
		UGameplayStatics::PlaySound2D(this, UltimateSkillActivationSound, AudioVolume);
	}
}

void UGS_SkillWidget::PlayUltimateSkillCooldownSound()
{
	if (bEnableAudio && UltimateSkillCooldownSound)
	{
		UGameplayStatics::PlaySound2D(this, UltimateSkillCooldownSound, AudioVolume);
	}
}

void UGS_SkillWidget::OnSkillCooldownReady(ESkillSlot InSkillSlot)
{
	if (InSkillSlot == SkillSlot)
	{
		// 스킬 아이콘 주변 반짝임 효과
		PlaySkillReadyAnimation();
		PlaySkillReadyGlow();

		// 사운드 재생
		if (IsUltimateSkillSlot())
		{
			PlayUltimateSkillReadySound();
			// 궁극기는 화면 테두리 황금빛 플래시 추가
			PlayUltimateReadyScreenFlash();
		}
		else
		{
			PlaySkillReadySound();
		}
	}
}

void UGS_SkillWidget::PlaySkillReadySound()
{
	if (bEnableAudio && SkillReadySound)
	{
		UGameplayStatics::PlaySound2D(this, SkillReadySound, AudioVolume);
	}
}

void UGS_SkillWidget::PlayUltimateSkillReadySound()
{
	if (bEnableAudio && UltimateSkillReadySound)
	{
		UGameplayStatics::PlaySound2D(this, UltimateSkillReadySound, AudioVolume);
	}
}