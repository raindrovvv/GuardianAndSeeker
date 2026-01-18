// Copyright

#include "Character/Component/GS_KillFeedbackComponent.h"

#include "Character/GS_Character.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "UI/KillFeedback/GS_KillFeedbackWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Sound/SoundBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

UGS_KillFeedbackComponent::UGS_KillFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGS_KillFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	// 로컬 플레이어만 위젯 생성 및 사운드 프리로딩
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		if (PC->IsLocalController())
		{
			CreateKillFeedbackWidget();

			// 사운드 비동기 로드 시작 (최적화)
			TArray<FSoftObjectPath> AssetsToLoad;
			auto AddIfValid = [&](const TSoftObjectPtr<USoundBase>& Ptr)
			{
				if (!Ptr.IsNull())
					AssetsToLoad.Add(Ptr.ToSoftObjectPath());
			};

			AddIfValid(MonsterKillSound);
			AddIfValid(EliteKillSound);
			AddIfValid(BossKillSound);
			AddIfValid(SeekerKillSound);
			AddIfValid(GuardianRepelledSound);
			AddIfValid(AssistSound);
			AddIfValid(TeammateDyingSound);
			AddIfValid(TeammateDeathSound);
			AddIfValid(TeammateRevivedSound);
			AddIfValid(RescueContributionSound);

			if (AssetsToLoad.Num() > 0)
			{
				UAssetManager::GetStreamableManager().RequestAsyncLoad(AssetsToLoad, FStreamableDelegate::CreateLambda([this, AssetsToLoad]()
				                                                                                                       {
					// 로드 완료 후 캐싱 (GC 방지)
					for (const FSoftObjectPath& Path : AssetsToLoad)
					{
						if (UObject* Asset = Path.ResolveObject())
						{
							if (USoundBase* Sound = Cast<USoundBase>(Asset))
							{
								CachedLoadedSounds.Add(Sound);
							}
						}
					} }));
			}
		}
	}
}

void UGS_KillFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(KillFeedbackWidget))
	{
		KillFeedbackWidget->RemoveFromParent();
		KillFeedbackWidget = nullptr;
	}

	CachedLoadedSounds.Empty();

	Super::EndPlay(EndPlayReason);
}

void UGS_KillFeedbackComponent::CreateKillFeedbackWidget()
{
	if (!KillFeedbackWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[KillFeedback] Widget class not set"));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	KillFeedbackWidget = CreateWidget<UGS_KillFeedbackWidget>(PC, KillFeedbackWidgetClass);
	if (KillFeedbackWidget)
	{
		KillFeedbackWidget->AddToViewport(5); // 적당한 Z-Order
		KillFeedbackWidget->InitializeWidget(this);
	}
}

void UGS_KillFeedbackComponent::NotifyKill(EKillFeedbackType FeedbackType, const FString& TargetName, const FString& InstigatorName, uint8 InstigatorTeamID)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// 몬스터 킬인 경우 Throttling 적용
	if (FeedbackType == EKillFeedbackType::MonsterKill)
	{
		MonsterKillCount++;

		// 이미 타이머가 돌고 있다면 갱신하지 않고 대기
		if (!GetWorld()->GetTimerManager().IsTimerActive(MonsterKillThrottlingTimer))
		{
			GetWorld()->GetTimerManager().SetTimer(MonsterKillThrottlingTimer, this, &UGS_KillFeedbackComponent::SendThrottledMonsterKill, 0.5f, false);
		}
		return;
	}

	// InstigatorName이 비어있으면 오너 이름 사용
	FString FinalInstigatorName = InstigatorName;
	if (FinalInstigatorName.IsEmpty())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
		{
			if (PC->PlayerState)
			{
				FinalInstigatorName = PC->PlayerState->GetPlayerName();
			}
		}
	}

	// 모든 플레이어에게 브로드캐스트
	BroadcastToAllPlayers(FeedbackType, TargetName, FinalInstigatorName, InstigatorTeamID);
}

void UGS_KillFeedbackComponent::SendThrottledMonsterKill()
{
	if (MonsterKillCount <= 0)
		return;

	FString DisplayName = TEXT("Monster");
	if (MonsterKillCount > 1)
	{
		DisplayName = FString::Printf(TEXT("Monster x%d"), MonsterKillCount);
	}

	FString InstigatorName = TEXT("");
	uint8 TeamID = 0;

	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		if (PC->PlayerState)
		{
			InstigatorName = PC->PlayerState->GetPlayerName();
			if (IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(PC->PlayerState))
			{
				TeamID = TeamAgent->GetGenericTeamId().GetId();
			}
		}
	}

	BroadcastToAllPlayers(EKillFeedbackType::MonsterKill, DisplayName, InstigatorName, TeamID);

	MonsterKillCount = 0;
}

void UGS_KillFeedbackComponent::NotifyAssist(const FString& AssisterName)
{
	if (!GetOwner())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		// 어시스트는 보통 당사자에게만 띄워주거나, 전체 로그에 "A가 어시스트함"이라고 띄울 수도 있음.
		// 기획 의도에 따라 다르지만, 여기서는 당사자 강조 + 전체 로그 로직을 위해 브로드캐스트 사용
		// 타입은 Assist, Target은 없음(보통 킬 로그에 붙기 때문), Instigator=Assister
		BroadcastToAllPlayers(EKillFeedbackType::Assist, TEXT(""), TEXT(""), 0, AssisterName);
	}
}

void UGS_KillFeedbackComponent::NotifyTeammateDying(const FString& DyingPlayerName)
{
	if (!GetOwner())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		// 빈사 상태는 "누가" 그랬는지보다 "누가 빈사인지(Target)"가 중요
		// Instigator는 보통 몬스터이거나 불명. 비워둠.
		BroadcastToAllPlayers(EKillFeedbackType::TeammateDying, DyingPlayerName, TEXT(""), 0);
	}
}

void UGS_KillFeedbackComponent::NotifyTeammateDeath(const FString& DeadPlayerName)
{
	if (!GetOwner())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		BroadcastToAllPlayers(EKillFeedbackType::TeammateDeath, DeadPlayerName, TEXT(""), 0);
	}
}

void UGS_KillFeedbackComponent::NotifyTeammateRevived(const FString& RevivedPlayerName, const FString& ReviverName)
{
	if (!GetOwner())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		// 구조 완료 사실은 모두에게 중요. Target=구조된사람, Instigator=구조자
		BroadcastToAllPlayers(EKillFeedbackType::TeammateRevived, RevivedPlayerName, ReviverName, 0);
	}
}

void UGS_KillFeedbackComponent::NotifyRescueContribution(const FString& RevivedPlayerName, const FString& InstigatorName)
{
	if (!GetOwner())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		// 구조 기여는 "당사자"에게 칭찬하는 용도 (점수 획득 등)
		// 하지만 "A가 B를 구조함" 로그를 띄우고 싶다면 Broadcast도 가능.
		// 여기서는 기획 의도상 "구조한 사람에게만" 뜨는 별도 피드백일 수 있으나
		// AI도 포함해야 하다면 Broadcast 후 Instigator 체크하는게 나음.
		BroadcastToAllPlayers(EKillFeedbackType::RescueContribution, RevivedPlayerName, InstigatorName, 0);
	}
}

void UGS_KillFeedbackComponent::NotifyTeamContribution(
    const TArray<FString>& ContributorNames,
    const TArray<uint8>& ContributionFlags)
{
	// 팀 기여도 표시는 가디언 격퇴 시에만 사용
	// 위젯에서 직접 처리하도록 델리게이트 브로드캐스트
	FKillFeedbackInfo Info;
	Info.FeedbackType = EKillFeedbackType::GuardianRepelled;

	// 모든 기여자의 이름과 플래그를 취합하여 전달 (논리적 오류 수정: 데이터 손실 방지)
	if (ContributorNames.Num() > 0)
	{
		// 이름: "Player1, Player2..." 형태로 연결
		Info.AssisterName = FString::Join(ContributorNames, TEXT(", "));
	}

	// 플래그: 모든 기여자의 플래그를 OR 연산으로 합침
	uint8 CombinedFlags = 0;
	for (uint8 Flag : ContributionFlags)
	{
		CombinedFlags |= Flag;
	}
	Info.ContributionFlags = CombinedFlags;

	OnKillFeedbackReceived.Broadcast(Info);
}

void UGS_KillFeedbackComponent::BroadcastToAllPlayers(
    EKillFeedbackType FeedbackType,
    const FString& TargetName,
    const FString& InstigatorName,
    uint8 InstigatorTeamID,
    const FString& AssisterName)
{
	UWorld* World = GetWorld();
	if (!World)
		return;

	// 모든 플레이어 컨트롤러 순회하여 RPC 발송
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
			continue;

		UGS_KillFeedbackComponent* Comp = PC->FindComponentByClass<UGS_KillFeedbackComponent>();
		if (Comp)
		{
			// 각 클라이언트에게 RPC 발송
			switch (FeedbackType)
			{
			case EKillFeedbackType::Assist:
				Comp->Client_ShowAssistFeedback(AssisterName);
				break;
			case EKillFeedbackType::TeammateDying:
				Comp->Client_ShowTeammateDyingFeedback(TargetName);
				break;
			case EKillFeedbackType::TeammateDeath:
				Comp->Client_ShowTeammateDeathFeedback(TargetName);
				break;
			case EKillFeedbackType::TeammateRevived:
				Comp->Client_ShowTeammateRevivedFeedback(TargetName, InstigatorName);
				break;
			case EKillFeedbackType::RescueContribution:
				Comp->Client_ShowRescueContributionFeedback(TargetName, InstigatorName);
				break;
			default:
				Comp->Client_ShowKillFeedback(FeedbackType, TargetName, InstigatorName, InstigatorTeamID);
				break;
			}
		}
	}
}

void UGS_KillFeedbackComponent::Client_ShowKillFeedback_Implementation(
    EKillFeedbackType FeedbackType,
    const FString& TargetName,
    const FString& InstigatorName,
    uint8 InstigatorTeamID)
{
	ShowKillFeedbackInternal(FeedbackType, TargetName, InstigatorName, InstigatorTeamID);
}

void UGS_KillFeedbackComponent::Client_ShowAssistFeedback_Implementation(const FString& AssisterName)
{
	ShowKillFeedbackInternal(EKillFeedbackType::Assist, TEXT(""), TEXT(""), 0, AssisterName);
}

void UGS_KillFeedbackComponent::Client_ShowTeammateDyingFeedback_Implementation(const FString& DyingPlayerName)
{
	ShowKillFeedbackInternal(EKillFeedbackType::TeammateDying, DyingPlayerName, TEXT(""), 0);
}

void UGS_KillFeedbackComponent::Client_ShowTeammateDeathFeedback_Implementation(const FString& DeadPlayerName)
{
	ShowKillFeedbackInternal(EKillFeedbackType::TeammateDeath, DeadPlayerName, TEXT(""), 0);
}

void UGS_KillFeedbackComponent::Client_ShowTeammateRevivedFeedback_Implementation(
    const FString& RevivedPlayerName,
    const FString& ReviverName)
{
	ShowKillFeedbackInternal(EKillFeedbackType::TeammateRevived, RevivedPlayerName, ReviverName, 0);
}

void UGS_KillFeedbackComponent::Client_ShowRescueContributionFeedback_Implementation(
    const FString& RevivedPlayerName,
    const FString& InstigatorName)
{
	ShowKillFeedbackInternal(EKillFeedbackType::RescueContribution, RevivedPlayerName, InstigatorName, 0);
}

void UGS_KillFeedbackComponent::ShowKillFeedbackInternal(
    EKillFeedbackType FeedbackType,
    const FString& TargetName,
    const FString& InstigatorName,
    uint8 InstigatorTeamID,
    const FString& AssisterName)
{
	FKillFeedbackInfo Info;
	Info.FeedbackType = FeedbackType;
	Info.TargetName = TargetName;
	Info.InstigatorName = InstigatorName;
	Info.AssisterName = AssisterName;
	Info.TeamID = InstigatorTeamID;

	// 나 자신인지 확인 (UI 강조용)
	bool bIsSelf = false;
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		if (PC->PlayerState)
		{
			bIsSelf = (PC->PlayerState->GetPlayerName() == InstigatorName);
		}
	}

	// 델리게이트 브로드캐스트 (위젯에서 수신)
	// 위젯에서 bIsSelf 여부에 따라 UI 배너를 띄울지 말지 결정하도록 로직을 위젯으로 넘김
	// 현재 Info 구조체엔 bIsSelf가 없으므로 InstigatorName을 통해 위젯이 판단해야 함
	// 또는 Info 구조체에 Flags를 활용할 수도 있음.
	OnKillFeedbackReceived.Broadcast(Info);

	// 사운드 재생
	// 내가 당사자(Instigator)이거나, 내 팀원(Team)이 관련된 중요 사안일 때 재생
	// 여기서는 일단 '모든' 피드백에 대해 소리를 재생하되,
	// 추후 위젯이나 컴포넌트에서 '내 소리'와 '남 소리'를 구분할 수 있음.
	// 현재 로직: bIsSelf일 때만 '강조된' 소리가 나야 하지만, 일단 재생하고 사운드 큐에서 거리를 두거나(2D vs 3D),
	// 혹은 여기서 bIsSelf 체크 후 분기.

	// 개선: 내가 주인공인 경우에만 사운드 재생 (또는 중요한 팀 이벤트)
	if (bIsSelf ||
	    FeedbackType == EKillFeedbackType::TeammateDying ||
	    FeedbackType == EKillFeedbackType::TeammateDeath ||
	    FeedbackType == EKillFeedbackType::TeammateRevived ||
	    FeedbackType == EKillFeedbackType::GuardianRepelled)
	{
		PlayFeedbackSound(FeedbackType);
	}
}

void UGS_KillFeedbackComponent::PlayFeedbackSound(EKillFeedbackType FeedbackType)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TSoftObjectPtr<USoundBase>* SoundSlot = nullptr;

	switch (FeedbackType)
	{
	case EKillFeedbackType::MonsterKill:
		SoundSlot = &MonsterKillSound;
		break;
	case EKillFeedbackType::EliteKill:
		SoundSlot = &EliteKillSound;
		break;
	case EKillFeedbackType::BossKill:
		SoundSlot = &BossKillSound;
		break;
	case EKillFeedbackType::SeekerKill:
		SoundSlot = &SeekerKillSound;
		break;
	case EKillFeedbackType::GuardianRepelled:
		SoundSlot = &GuardianRepelledSound;
		break;
	case EKillFeedbackType::Assist:
		SoundSlot = &AssistSound;
		break;
	case EKillFeedbackType::TeammateDying:
		SoundSlot = &TeammateDyingSound;
		break;
	case EKillFeedbackType::TeammateDeath:
		SoundSlot = &TeammateDeathSound;
		break;
	case EKillFeedbackType::TeammateRevived:
		SoundSlot = &TeammateRevivedSound;
		break;
	case EKillFeedbackType::RescueContribution:
		SoundSlot = &RescueContributionSound;
		break;
	default:
		return;
	}

	if (SoundSlot && !SoundSlot->IsNull())
	{
		USoundBase* SoundToPlay = nullptr;

		// 1. 이미 로드된 에셋인지 확인
		if (UObject* ResolvedObj = SoundSlot->Get())
		{
			SoundToPlay = Cast<USoundBase>(ResolvedObj);
		}

		// 2. 로드되지 않았다면 동기 로드
		if (!SoundToPlay)
		{
			SoundToPlay = SoundSlot->LoadSynchronous();
		}

		if (SoundToPlay)
		{
			// UI 사운드이므로 PlaySound2D 사용
			UGameplayStatics::PlaySound2D(this, SoundToPlay);
		}
	}
}
