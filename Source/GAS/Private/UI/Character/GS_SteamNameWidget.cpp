#include "UI/Character/GS_SteamNameWidget.h"

#include "Character/Player/GS_Player.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"

UGS_SteamNameWidget::UGS_SteamNameWidget(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
}

void UGS_SteamNameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	FTimerHandle WidgetTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(WidgetTimerHandle, this, &UGS_SteamNameWidget::InitializeSteamNameWidget, 3.f, false);
}

void UGS_SteamNameWidget::InitializeSteamNameWidget()
{
	if (OwningActor)
	{
		AGS_Player* OwningPlayer = Cast<AGS_Player>(OwningActor);
		
		if (IsValid(OwningPlayer))
		{
			if (OwningPlayer->GetPlayerState())
			{
				SteamNameText->SetText(FText::FromString(OwningPlayer->GetPlayerState()->GetPlayerName()));
				
				// 로컬 플레이어 본인이면 네임태그 위젯 숨김
				if (OwningPlayer->IsLocallyControlled())
				{
					SetVisibility(ESlateVisibility::Collapsed);
				}
			}
		}
	}
}
