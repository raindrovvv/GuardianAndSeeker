// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Character/GS_PlayerInfoWidget.h"

#include "Character/Component/GS_StatComp.h"
#include "Character/Player/GS_Player.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "GameFramework/PlayerState.h"
#include "Character/Player/GS_PawnMappingDataAsset.h"
#include "Kismet/KismetSystemLibrary.h"
#include "System/GS_PlayerState.h"

void UGS_PlayerInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsValid(OwningCharacter))
	{
		OwningCharacter = Cast<AGS_Player>(GetOwningPlayer()->GetPawn());
	}

	if (IsValid(OwningCharacter))
	{
		OwningCharacter->SetPlayerInfoWidget(this);
	}
}

void UGS_PlayerInfoWidget::InitializePlayerInfoWidget(AGS_Player* InPlayer)
{
	if (InPlayer)
	{
		OnCurrentHPBarChanged(InPlayer->GetStatComp());

		// PlayerName 설정 (AI 시커는 PlayerState가 없을 수 있음)
		if (InPlayer->GetPlayerState())
		{
			PlayerName->SetText(FText::FromString(InPlayer->GetPlayerState()->GetPlayerName()));
		}
		else
		{
			// AI 시커인 경우 캐릭터 타입에 따른 이름 표시
			FString AIName = TEXT("AI Seeker");
			switch (InPlayer->CharacterType)
			{
			case ECharacterType::Chan:
				AIName = TEXT("AI Chan");
				break;
			case ECharacterType::Ares:
				AIName = TEXT("AI Ares");
				break;
			case ECharacterType::Merci:
				AIName = TEXT("AI Merci");
				break;
			case ECharacterType::Reina:
				AIName = TEXT("AI Reina");
				break;
			}
			PlayerName->SetText(FText::FromString(AIName));
		}

		// CharacterType에서 SeekerJob으로 직접 변환
		// (PlayerState의 CurrentSeekerJob은 타이밍 이슈로 신뢰할 수 없음)
		ESeekerJob SeekerJob = ESeekerJob::Chan; // 기본값

		if (InPlayer->CharacterType == ECharacterType::Chan)
		{
			SeekerJob = ESeekerJob::Chan;
		}
		else if (InPlayer->CharacterType == ECharacterType::Ares)
		{
			SeekerJob = ESeekerJob::Ares;
		}
		else if (InPlayer->CharacterType == ECharacterType::Merci)
		{
			SeekerJob = ESeekerJob::Merci;
		}
		else if (InPlayer->CharacterType == ECharacterType::Reina)
		{
			SeekerJob = ESeekerJob::Reina;
		}

		if (IsValid(PawnMappingData))
		{
			const FAssetToSpawn* SpawnInfo = PawnMappingData->SeekerPawnClasses.Find(SeekerJob);
			if (SpawnInfo)
			{
				PlayerClass->SetBrushFromTexture(SpawnInfo->ClassIconTexture);
			}
		}
	}
}

void UGS_PlayerInfoWidget::OnCurrentHPBarChanged(UGS_StatComp* InStatComp)
{
	HPBarWidget->SetPercent(InStatComp->GetCurrentHealth() / InStatComp->GetMaxHealth());
}
