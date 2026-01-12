#include "UI/Character/GS_HPBoardWidget.h"

#include "Character/GS_Character.h"
#include "Character/Player/GS_Player.h"
#include "Components/VerticalBox.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "System/GS_PlayerState.h"
#include "UI/Character/GS_HPWidget.h"
#include "UI/Character/GS_PlayerInfoWidget.h"
#include "Character/Player/Seeker/GS_Seeker.h"

void UGS_HPBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(GetOwningPlayer()->GetPawn()))
	{
		OwningCharacter = Cast<AGS_Character>(GetOwningPlayer()->GetPawn());
	}

	FTimerHandle WidgetTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(WidgetTimerHandle, this, &UGS_HPBoardWidget::InitBoardWidget, 3.f);
}

void UGS_HPBoardWidget::InitBoardWidget()
{
	if (!IsValid(PlayerInfoWidgetClass) || !IsValid(PlayerInfoContainer))
	{
		return;
	}

	// 최신 OwningCharacter 업데이트 (NativeConstruct 이후 폰이 변경되었을 수 있음)
	if (APlayerController* PC = GetOwningPlayer())
	{
		OwningCharacter = Cast<AGS_Character>(PC->GetPawn());
	}

	PlayerInfoContainer->ClearChildren();

	// 1. 실제 플레이어(Human) 추가
	AGameStateBase* GS = UGameplayStatics::GetGameState(this);
	TSet<AActor*> AddedSeekers; // 중복 추가 방지용

	if (IsValid(GS))
	{
		TArray<APlayerState*> PSA = GS->PlayerArray;
		for (APlayerState* PS : PSA)
		{
			AGS_PlayerState* GSPS = Cast<AGS_PlayerState>(PS);
			if (IsValid(GSPS))
			{
				if (GSPS->CurrentPlayerRole == EPlayerRole::PR_Guardian)
				{
					continue;
				}

				AGS_Player* Player = Cast<AGS_Player>(GSPS->GetPawn());
				// 자신을 제외하고 리스트에 추가
				if (IsValid(Player) && !Player->IsLocallyControlled())
				{
					UGS_PlayerInfoWidget* PlayerInfoWidget = CreateWidget<UGS_PlayerInfoWidget>(this, PlayerInfoWidgetClass);
					if (IsValid(PlayerInfoWidget))
					{
						PlayerInfoWidget->SetOwningActor(Player);
						PlayerInfoWidget->InitializePlayerInfoWidget(Player);
						PlayerInfoContainer->AddChildToVerticalBox(PlayerInfoWidget);
						AddedSeekers.Add(Player);
					}
				}
			}
		}
	}

	// 2. AI 시커 추가 (PlayerState가 없는 AI 봇들)
	TArray<AActor*> FoundSeekers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGS_Seeker::StaticClass(), FoundSeekers);

	for (AActor* Actor : FoundSeekers)
	{
		AGS_Seeker* Seeker = Cast<AGS_Seeker>(Actor);
		// 자신을 제외하고, 위에서 이미 추가된 플레이어가 아니며, AI 컨트롤러가 제어 중인 경우만 추가
		if (IsValid(Seeker) && !Seeker->IsLocallyControlled() && !AddedSeekers.Contains(Seeker))
		{
			// AI인지 확인 (PlayerState가 없거나 PlayerController가 아닌 경우)
			if (!Seeker->IsPlayerControlled() || !Seeker->GetPlayerState())
			{
				UGS_PlayerInfoWidget* PlayerInfoWidget = CreateWidget<UGS_PlayerInfoWidget>(this, PlayerInfoWidgetClass);
				if (IsValid(PlayerInfoWidget))
				{
					PlayerInfoWidget->SetOwningActor(Seeker);
					PlayerInfoWidget->InitializePlayerInfoWidget(Seeker);
					PlayerInfoContainer->AddChildToVerticalBox(PlayerInfoWidget);
					AddedSeekers.Add(Seeker);
				}
			}
		}
	}
}
