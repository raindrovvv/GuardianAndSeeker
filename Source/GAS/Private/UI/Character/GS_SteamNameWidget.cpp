#include "UI/Character/GS_SteamNameWidget.h"

#include "Character/Player/GS_Player.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Character/Player/Seeker/GS_Seeker.h"

UGS_SteamNameWidget::UGS_SteamNameWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
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
			}
			else
			{
				// AI 시커인 경우 캐릭터 타입에 따른 성함 및 번호 부여
				FString TypeName = TEXT("AI");
				switch (OwningPlayer->CharacterType)
				{
				case ECharacterType::Ares:
					TypeName = TEXT("Ares");
					break;
				case ECharacterType::Chan:
					TypeName = TEXT("Chan");
					break;
				case ECharacterType::Merci:
					TypeName = TEXT("Merci");
					break;
				case ECharacterType::Reina:
					TypeName = TEXT("Reina");
					break;
				}

				TArray<AActor*> FoundActors;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGS_Seeker::StaticClass(), FoundActors);

				TArray<AGS_Seeker*> SameTypeAI;
				for (AActor* Actor : FoundActors)
				{
					AGS_Seeker* Seeker = Cast<AGS_Seeker>(Actor);
					if (IsValid(Seeker) && Seeker->CharacterType == OwningPlayer->CharacterType && !Seeker->GetPlayerState())
					{
						SameTypeAI.Add(Seeker);
					}
				}

				// 인스턴스 이름 기준으로 정렬하여 순서 보장
				SameTypeAI.Sort([](const AGS_Seeker& A, const AGS_Seeker& B)
				                { return A.GetName() < B.GetName(); });

				int32 Index = SameTypeAI.Find(Cast<AGS_Seeker>(OwningPlayer));
				FString FinalName = FString::Printf(TEXT("%s %d"), *TypeName, Index + 1);
				SteamNameText->SetText(FText::FromString(FinalName));
			}

			// 로컬 플레이어 본인이면 네임태그 위젯 숨김
			if (OwningPlayer->IsLocallyControlled())
			{
				SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}
