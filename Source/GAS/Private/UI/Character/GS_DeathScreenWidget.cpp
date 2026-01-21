// Copyright

#include "UI/Character/GS_DeathScreenWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Animation/WidgetAnimation.h"

void UGS_DeathScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 초기에는 숨김 상태 (ShowDeathScreen 호출 시 표시)
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGS_DeathScreenWidget::ShowDeathScreen(const FLastKillerInfo& KillerInfo)
{
	// 타이틀 텍스트 설정
	if (DeathTitleText)
	{
		DeathTitleText->SetText(DeathTitleDefault);
	}

	// 킬러 정보 텍스트 설정
	if (KillerInfoText)
	{
		FText KillerName =
			KillerInfo.KillerName.IsEmpty() ? UnknownKillerText : FText::FromString(KillerInfo.KillerName);

		FText FormattedText = FText::Format(KillerInfoFormat, KillerName);
		KillerInfoText->SetText(FormattedText);

		// 킬러 타입에 따른 색상 적용
		FLinearColor TextColor = GetColorForKillerType(KillerInfo.KillerType);
		KillerInfoText->SetColorAndOpacity(FSlateColor(TextColor));
	}

	// 위젯 표시
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// 페이드 인 애니메이션 재생
	if (FadeInAnimation)
	{
		PlayAnimation(FadeInAnimation);
	}
}

void UGS_DeathScreenWidget::HideDeathScreen()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

FLinearColor UGS_DeathScreenWidget::GetColorForKillerType(ECharacterType KillerType) const
{
	if (IsMonsterType(KillerType))
	{
		return MonsterColor;
	}
	else if (IsSeekerType(KillerType))
	{
		return SeekerColor;
	}
	else if (IsGuardianType(KillerType))
	{
		return GuardianColor;
	}

	return DefaultColor;
}

bool UGS_DeathScreenWidget::IsMonsterType(ECharacterType Type) const
{
	switch (Type)
	{
		case ECharacterType::SmallClaw:
		case ECharacterType::NeedleFang:
		case ECharacterType::IronFang:
		case ECharacterType::StoneClaw:
		case ECharacterType::ShadowFang:
			return true;
		default:
			return false;
	}
}

bool UGS_DeathScreenWidget::IsSeekerType(ECharacterType Type) const
{
	switch (Type)
	{
		case ECharacterType::Ares:
		case ECharacterType::Chan:
		case ECharacterType::Merci:
		case ECharacterType::Reina:
			return true;
		default:
			return false;
	}
}

bool UGS_DeathScreenWidget::IsGuardianType(ECharacterType Type) const
{
	return Type == ECharacterType::Drakhar;
}
