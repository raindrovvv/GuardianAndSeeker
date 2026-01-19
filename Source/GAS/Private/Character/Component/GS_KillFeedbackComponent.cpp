// Copyright

#include "Character/Component/GS_KillFeedbackComponent.h"

#include "Blueprint/UserWidget.h"
#include "Character/GS_Character.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "System/GameState/GS_InGameGS.h"
#include "TimerManager.h"
#include "UI/KillFeedback/GS_KillFeedbackWidget.h"


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
			AddIfValid(AchievementSound); // 업적 사운드도 프리로드

			if (AssetsToLoad.Num() > 0)
			{
				// 안전한 약한 참조 사용 (컴포넌트 파괴 시 크래시 방지)
				TWeakObjectPtr<UGS_KillFeedbackComponent> WeakThis(this);

				UAssetManager::GetStreamableManager().RequestAsyncLoad(
					AssetsToLoad,
					FStreamableDelegate::CreateLambda(
						[WeakThis, AssetsToLoad]()
						{
							if (!WeakThis.IsValid())
							{
								return;
							}

							// 로드 완료 후 캐싱 (GC 방지)
							for (const FSoftObjectPath& Path : AssetsToLoad)
							{
								if (UObject* Asset = Path.ResolveObject())
								{
									if (USoundBase* Sound = Cast<USoundBase>(Asset))
									{
										WeakThis->CachedLoadedSounds.Add(Sound);
									}
								}
							}
						}));
			}
		}
	}
}

void UGS_KillFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 위젯 정리
	if (IsValid(KillFeedbackWidget))
	{
		KillFeedbackWidget->RemoveFromParent();
		KillFeedbackWidget = nullptr;
	}

	// 사운드 캐시 정리
	CachedLoadedSounds.Empty();

	// 타이머 정리 (람다 크래시 방지)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MonsterKillThrottlingTimer);
		World->GetTimerManager().ClearTimer(KillStreakTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UGS_KillFeedbackComponent::CreateKillFeedbackWidget()
{
	if (!KillFeedbackWidgetClass)
	{
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

void UGS_KillFeedbackComponent::NotifyKill(EKillFeedbackType FeedbackType,
										   const FString& TargetName,
										   const FString& InstigatorName,
										   uint8 InstigatorTeamID,
										   bool bIsInstigatorPlayer,
										   bool bWasCritical)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// 몬스터 킬인 경우 Throttling 적용 (UI 표시는 지연하지만 업적 체크는 즉시)
	if (FeedbackType == EKillFeedbackType::MonsterKill)
	{
		MonsterKillCount++;

		// 업적 체크는 항상 수행 (전역 업적은 AI도 처리해야 함)
		CheckAndTriggerAchievements(FeedbackType, InstigatorName, bIsInstigatorPlayer, bWasCritical);

		// UI 표시는 Throttling 적용
		if (!GetWorld()->GetTimerManager().IsTimerActive(MonsterKillThrottlingTimer))
		{
			GetWorld()->GetTimerManager().SetTimer(
				MonsterKillThrottlingTimer, this, &UGS_KillFeedbackComponent::SendThrottledMonsterKill, 0.5f, false);
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

	// 업적 체크는 항상 수행 (전역 업적은 AI도 처리해야 함)
	CheckAndTriggerAchievements(FeedbackType, FinalInstigatorName, bIsInstigatorPlayer, bWasCritical);

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
void UGS_KillFeedbackComponent::NotifyAssist(const FString& AssisterName, bool bInIsAssisterPlayer)
{
	if (!GetOwner())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		// 1. 개인 카운트 증가 (플레이어인 경우)
		if (bInIsAssisterPlayer)
		{
			AssistCount++;
			CheckAssistAchievements(AssisterName, bInIsAssisterPlayer);
		}

		// 어시스트는 보통 당사자에게만 띄워주거나, 전체 로그에 "A가
		// 어시스트함"이라고 띄울 수도 있음. 기획 의도에 따라 다르지만, 여기서는
		// 당사자 강조 + 전체 로그 로직을 위해 브로드캐스트 사용 타입은 Assist,
		// Target은 없음(보통 킬 로그에 붙기 때문), Instigator=Assister
		BroadcastToAllPlayers(EKillFeedbackType::Assist, TEXT(""), TEXT(""), 0, AssisterName);
	}
}

void UGS_KillFeedbackComponent::CheckAssistAchievements(const FString& InAssisterName, bool bInIsPlayer)
{
	if (AssistCount == TacticianThreshold)
	{
		TriggerAchievement(EKillAchievementType::Tactician, InAssisterName, bInIsPlayer, AssistCount);
	}
	else if (AssistCount == BattleMasterThreshold)
	{
		TriggerAchievement(EKillAchievementType::BattleMaster, InAssisterName, bInIsPlayer, AssistCount);
	}
	else if (AssistCount == LegendarySupportThreshold)
	{
		TriggerAchievement(EKillAchievementType::LegendarySupport, InAssisterName, bInIsPlayer, AssistCount);
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
		// 업적 체크 (Lifesaver) - 구조한 사람을 달성자로 설정
		CheckAndTriggerAchievements(EKillFeedbackType::TeammateRevived, ReviverName, true);

		// 구조 완료 사실은 모두에게 중요. Target=구조된사람, Instigator=구조자
		BroadcastToAllPlayers(EKillFeedbackType::TeammateRevived, RevivedPlayerName, ReviverName, 0);
	}
}

void UGS_KillFeedbackComponent::NotifyRescueContribution(const FString& RevivedPlayerName,
														 const FString& InstigatorName)
{
	if (!GetOwner())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		// 업적 체크 (Lifesaver) - 구조 기여자에게도 업적 부여
		CheckAndTriggerAchievements(EKillFeedbackType::RescueContribution, InstigatorName, true);

		// 구조 기여는 "당사자"에게 칭찬하는 용도 (점수 획득 등)
		// 하지만 "A가 B를 구조함" 로그를 띄우고 싶다면 Broadcast도 가능.
		// 여기서는 기획 의도상 "구조한 사람에게만" 뜨는 별도 피드백일 수 있으나
		// AI도 포함해야 하다면 Broadcast 후 Instigator 체크하는게 나음.
		BroadcastToAllPlayers(EKillFeedbackType::RescueContribution, RevivedPlayerName, InstigatorName, 0);
	}
}

void UGS_KillFeedbackComponent::NotifyTeamContribution(const TArray<FString>& ContributorNames,
													   const TArray<uint8>& ContributionFlags)
{
	// 팀 기여도 표시는 가디언 격퇴 시에만 사용
	// 위젯에서 직접 처리하도록 델리게이트 브로드캐스트
	FKillFeedbackInfo Info;
	Info.FeedbackType = EKillFeedbackType::GuardianRepelled;

	// 모든 기여자의 이름과 플래그를 취합하여 전달 (논리적 오류 수정: 데이터 손실
	// 방지)
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

void UGS_KillFeedbackComponent::BroadcastToAllPlayers(EKillFeedbackType FeedbackType,
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

void UGS_KillFeedbackComponent::Client_ShowKillFeedback_Implementation(EKillFeedbackType FeedbackType,
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

void UGS_KillFeedbackComponent::Client_ShowTeammateRevivedFeedback_Implementation(const FString& RevivedPlayerName,
																				  const FString& ReviverName)
{
	ShowKillFeedbackInternal(EKillFeedbackType::TeammateRevived, RevivedPlayerName, ReviverName, 0);
}

void UGS_KillFeedbackComponent::Client_ShowRescueContributionFeedback_Implementation(const FString& RevivedPlayerName,
																					 const FString& InstigatorName)
{
	ShowKillFeedbackInternal(EKillFeedbackType::RescueContribution, RevivedPlayerName, InstigatorName, 0);
}

void UGS_KillFeedbackComponent::ShowKillFeedbackInternal(EKillFeedbackType FeedbackType,
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
	// 위젯에서 bIsSelf 여부에 따라 UI 배너를 띄울지 말지 결정하도록 로직을
	// 위젯으로 넘김 현재 Info 구조체엔 bIsSelf가 없으므로 InstigatorName을 통해
	// 위젯이 판단해야 함 또는 Info 구조체에 Flags를 활용할 수도 있음.
	OnKillFeedbackReceived.Broadcast(Info);

	// 사운드 재생
	float VolumeMultiplier = DefaultFeedbackVolume;
	bool bShouldPlaySound = bIsSelf;

	// 타입별 볼륨 및 재생 여부 결정 (본인용 몬스터 킬은 볼륨 낮게, 팀 위기는 본인이 아니어도 크게)
	if (FeedbackType == EKillFeedbackType::MonsterKill)
	{
		VolumeMultiplier = MonsterKillVolumeMultiplier;
	}
	else if (FeedbackType == EKillFeedbackType::TeammateDying || FeedbackType == EKillFeedbackType::TeammateDeath ||
			 FeedbackType == EKillFeedbackType::TeammateRevived || FeedbackType == EKillFeedbackType::GuardianRepelled)
	{
		VolumeMultiplier = ImportantEventVolumeMultiplier;
		bShouldPlaySound = true; // 본인이 아니어도 팀 중요 이벤트는 재생
	}

	if (bShouldPlaySound)
	{
		PlayFeedbackSound(FeedbackType, VolumeMultiplier);
	}
}

void UGS_KillFeedbackComponent::PlayFeedbackSound(EKillFeedbackType FeedbackType, float VolumeMultiplier)
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

		// 2. 로드되지 않았다면 비동기 로딩 대기 (동기 로드 제거하여 히치 방지)
		if (!SoundToPlay)
		{
			// BeginPlay에서 프리로드하므로 대부분은 1번에서 해결됨
			// 여기서는 로그만 남기고 무시 (플레이 도중 로딩을 기다리는 것보다 소리가 안 나는 게 나음)
		}

		if (SoundToPlay)
		{
			// UI 사운드이므로 PlaySound2D 사용
			UGameplayStatics::PlaySound2D(this, SoundToPlay, VolumeMultiplier);
		}
	}
}

// ===== 업적 시스템 구현 =====

void UGS_KillFeedbackComponent::ResetKillStreak()
{
	KillStreakCount = 0;
	CriticalKillStreakCount = 0;
	HighestAchievedStreak = EKillAchievementType::None;
}

void UGS_KillFeedbackComponent::ProcessKillStreak(const FString& InKillerName, bool bInIsKillerPlayer)
{
	if (KillStreakCount >= UnstoppableThreshold && HighestAchievedStreak < EKillAchievementType::Unstoppable)
	{
		HighestAchievedStreak = EKillAchievementType::Unstoppable;
		TriggerAchievement(EKillAchievementType::Unstoppable, InKillerName, bInIsKillerPlayer, KillStreakCount);
	}
	else if (KillStreakCount >= RampageThreshold && HighestAchievedStreak < EKillAchievementType::Rampage)
	{
		HighestAchievedStreak = EKillAchievementType::Rampage;
		TriggerAchievement(EKillAchievementType::Rampage, InKillerName, bInIsKillerPlayer, KillStreakCount);
	}
	else if (KillStreakCount >= MonsterSlayerThreshold && HighestAchievedStreak == EKillAchievementType::None)
	{
		HighestAchievedStreak = EKillAchievementType::MonsterSlayer;
		TriggerAchievement(EKillAchievementType::MonsterSlayer, InKillerName, bInIsKillerPlayer, KillStreakCount);
	}
}

void UGS_KillFeedbackComponent::CheckAndTriggerAchievements(EKillFeedbackType KillType,
															const FString& InKillerName,
															bool bInIsKillerPlayer,
															bool bWasCritical)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	AGS_InGameGS* GS = World->GetGameState<AGS_InGameGS>();

	// 1. 전역 업적: 퍼스트 블러드 (어떤 킬이든 첫 번째면 발생)
	if (KillType == EKillFeedbackType::MonsterKill || KillType == EKillFeedbackType::EliteKill ||
		KillType == EKillFeedbackType::BossKill || KillType == EKillFeedbackType::SeekerKill)
	{
		if (GS)
		{
			if (GS->TryTriggerFirstBlood())
			{
				TriggerAchievement(EKillAchievementType::FirstBlood, InKillerName, bInIsKillerPlayer);
			}
		}
		else
		{
			static bool bLocalFirstBloodTriggered = false;
			static TWeakObjectPtr<UWorld> LastFBWorld;
			if (LastFBWorld != World)
			{
				bLocalFirstBloodTriggered = false;
				LastFBWorld = World;
			}

			if (!bLocalFirstBloodTriggered)
			{
				bLocalFirstBloodTriggered = true;
				TriggerAchievement(EKillAchievementType::FirstBlood, InKillerName, bInIsKillerPlayer);
			}
		}
	}

	// 타입별 업적 체크
	switch (KillType)
	{
		case EKillFeedbackType::SeekerKill:
		case EKillFeedbackType::MonsterKill:
		case EKillFeedbackType::EliteKill:
		{
			// 1. 첫 킬 체크 (GameState 사용) - 퍼스트 블러드는 상단에서 처리됨.
			// 2. 시간 윈도우 체크 및 연속 킬 카운트 증가
			if (CurrentTime - LastKillTime > KillStreakTimeWindow)
			{
				ResetKillStreak();
			}

			KillStreakCount++;
			LastKillTime = CurrentTime;

			// 3. 연속 킬 타이머 리셋
			World->GetTimerManager().ClearTimer(KillStreakTimerHandle);
			World->GetTimerManager().SetTimer(
				KillStreakTimerHandle, this, &UGS_KillFeedbackComponent::ResetKillStreak, KillStreakTimeWindow, false);

			// 4. 엘리트 전용 처리
			if (KillType == EKillFeedbackType::EliteKill)
			{
				TriggerAchievement(EKillAchievementType::VeteranHunter, InKillerName, bInIsKillerPlayer);

				EliteKillStreakCount++;
				if (EliteKillStreakCount >= EliteSlayerThreshold)
				{
					TriggerAchievement(
						EKillAchievementType::EliteSlayer, InKillerName, bInIsKillerPlayer, EliteKillStreakCount);
					EliteKillStreakCount = 0;
				}
			}
			else
			{
				EliteKillStreakCount = 0;
			}

			// 5. 공통 스트릭 업적 체크 (MonsterSlayer, Rampage, Unstoppable)
			ProcessKillStreak(InKillerName, bInIsKillerPlayer);

			// 6. 크리티컬 킬 체크
			if (bWasCritical)
			{
				CriticalKillStreakCount++;
				if (CriticalKillStreakCount >= HeadhunterThreshold)
				{
					TriggerAchievement(
						EKillAchievementType::Headhunter, InKillerName, bInIsKillerPlayer, CriticalKillStreakCount);
					CriticalKillStreakCount = 0;
				}
			}
			else
			{
				CriticalKillStreakCount = 0;
			}
		}
		break;

		case EKillFeedbackType::GuardianRepelled:
		case EKillFeedbackType::BossKill:
		{
			if (GS)
			{
				if (GS->TryTriggerGuardianSlayer())
				{
					TriggerAchievement(EKillAchievementType::GuardianSlayer, InKillerName, bInIsKillerPlayer);
				}
			}
			else
			{
				// GameState가 없는 경우 (에디터 테스트 등) 로컬 폴백 사용
				static bool bLocalGuardianSlayerTriggered = false;
				static TWeakObjectPtr<UWorld> LastGSWorld;

				if (LastGSWorld != World)
				{
					bLocalGuardianSlayerTriggered = false;
					LastGSWorld = World;
				}

				if (!bLocalGuardianSlayerTriggered)
				{
					bLocalGuardianSlayerTriggered = true;
					TriggerAchievement(EKillAchievementType::GuardianSlayer, InKillerName, bInIsKillerPlayer);
				}
			}
		}
		break;

		case EKillFeedbackType::TeammateRevived:
		case EKillFeedbackType::RescueContribution:
		{
			TriggerAchievement(EKillAchievementType::Lifesaver, InKillerName, bInIsKillerPlayer);
		}
		break;

		default:
			break;
	}
}

EAchievementTier UGS_KillFeedbackComponent::GetTierForAchievement(EKillAchievementType AchievementType) const
{
	switch (AchievementType)
	{
		case EKillAchievementType::MonsterSlayer:
		case EKillAchievementType::Tactician:
			return EAchievementTier::Bronze;

		case EKillAchievementType::Rampage:
		case EKillAchievementType::VeteranHunter:
		case EKillAchievementType::Headhunter:
		case EKillAchievementType::BattleMaster:
			return EAchievementTier::Silver;

		case EKillAchievementType::Unstoppable:
		case EKillAchievementType::EliteSlayer:
		case EKillAchievementType::Lifesaver:
		case EKillAchievementType::LegendarySupport:
			return EAchievementTier::Gold;

		case EKillAchievementType::GuardianSlayer:
		case EKillAchievementType::FirstBlood:
			return EAchievementTier::Platinum;

		default:
			return EAchievementTier::Bronze;
	}
}

void UGS_KillFeedbackComponent::TriggerAchievement(EKillAchievementType AchievementType,
												   const FString& InAchieverName,
												   bool bInIsAchieverPlayer,
												   int32 KillCount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// 전역 업적 체크
	bool bIsGlobalAchievement = (AchievementType == EKillAchievementType::FirstBlood ||
								 AchievementType == EKillAchievementType::GuardianSlayer);

	// 쿨다운 체크 (반복 가능 업적만)
	if (!bIsGlobalAchievement)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		float* LastTriggeredTime = AchievementLastTriggeredTime.Find(AchievementType);

		if (LastTriggeredTime && (CurrentTime - *LastTriggeredTime) < RepeatableAchievementCooldown)
		{
			return;
		}
		AchievementLastTriggeredTime.Add(AchievementType, CurrentTime);
	}

	EAchievementTier Tier = GetTierForAchievement(AchievementType);

	// 달성자 이름 결정
	FString AchieverName = InAchieverName;
	if (AchieverName.IsEmpty())
	{
		AchieverName = TEXT("Unknown");
	}

	// AI 접두사 처리
	if (!bInIsAchieverPlayer && !AchieverName.StartsWith(TEXT("AI ")))
	{
		AchieverName = FString::Printf(TEXT("AI %s"), *AchieverName);
	}

	// 전역 업적 (모든 플레이어 브로드캐스트)
	if (bIsGlobalAchievement)
	{
		BroadcastAchievementToAllPlayers(AchievementType, Tier, KillCount, AchieverName);
		return;
	}

	// 개인 업적 (AI 제외)
	if (!bInIsAchieverPlayer)
	{
		return;
	}

	// 본인에게 RPC 발송
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
			PC = Cast<APlayerController>(OwnerPawn->GetController());
	}

	if (PC)
	{
		if (PC->IsLocalController())
		{
			ShowAchievementInternal(AchievementType, Tier, KillCount, AchieverName);
		}
		else
		{
			Client_ShowAchievement(AchievementType, Tier, KillCount, AchieverName);
		}
	}
}


void UGS_KillFeedbackComponent::BroadcastAchievementToAllPlayers(EKillAchievementType AchievementType,
																 EAchievementTier Tier,
																 int32 KillCount,
																 const FString& AchieverName)
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
			Comp->Client_ShowAchievement(AchievementType, Tier, KillCount, AchieverName);
		}
	}
}


void UGS_KillFeedbackComponent::Client_ShowAchievement_Implementation(EKillAchievementType AchievementType,
																	  EAchievementTier Tier,
																	  int32 KillCount,
																	  const FString& AchieverName)
{
	ShowAchievementInternal(AchievementType, Tier, KillCount, AchieverName);
}

void UGS_KillFeedbackComponent::ShowAchievementInternal(EKillAchievementType AchievementType,
														EAchievementTier Tier,
														int32 KillCount,
														const FString& AchieverName)
{
	FKillAchievementInfo AchievementInfo;
	AchievementInfo.AchievementType = AchievementType;
	AchievementInfo.Tier = Tier;
	AchievementInfo.KillCount = KillCount;
	AchievementInfo.AchieverName = AchieverName;

	// 델리게이트 브로드캐스트 (위젯에서 수신)
	OnAchievementUnlocked.Broadcast(AchievementInfo);

	// 업적 사운드 재생
	if (!AchievementSound.IsNull())
	{
		if (USoundBase* Sound = AchievementSound.Get()) // 이미 프리로드됨
		{
			UGameplayStatics::PlaySound2D(this, Sound, AchievementVolumeMultiplier);
		}
		else
		{
			// 혹시 안되었다면 동기 로드
			UGameplayStatics::PlaySound2D(this, AchievementSound.LoadSynchronous(), AchievementVolumeMultiplier);
		}
	}
}

// ===== 디버그 콘솔 명령어 =====
// 팁: ActorComponent의 Exec 함수는 오너가 PlayerController이거나 직접 연동되어야 작동합니다.
// 작동하지 않는다면 서버 트리거 테스트를 위해 TriggerAchievement를 서버에서 직접 호출하도록 보강합니다.

void UGS_KillFeedbackComponent::Debug_Achievement_MonsterSlayer()
{
	if (GetOwner() && GetOwner()->HasAuthority())
		TriggerAchievement(EKillAchievementType::MonsterSlayer, TEXT("DebugPlayer"), true, MonsterSlayerThreshold);
}

void UGS_KillFeedbackComponent::Debug_Achievement_Rampage()
{
	if (GetOwner() && GetOwner()->HasAuthority())
		TriggerAchievement(EKillAchievementType::Rampage, TEXT("DebugPlayer"), true, RampageThreshold);
}

void UGS_KillFeedbackComponent::Debug_Achievement_Unstoppable()
{
	if (GetOwner() && GetOwner()->HasAuthority())
		TriggerAchievement(EKillAchievementType::Unstoppable, TEXT("DebugPlayer"), true, UnstoppableThreshold);
}

void UGS_KillFeedbackComponent::Debug_Achievement_VeteranHunter()
{
	if (GetOwner() && GetOwner()->HasAuthority())
		TriggerAchievement(EKillAchievementType::VeteranHunter, TEXT("DebugPlayer"), true, 0);
}

void UGS_KillFeedbackComponent::Debug_Achievement_EliteSlayer()
{
	if (GetOwner() && GetOwner()->HasAuthority())
		TriggerAchievement(EKillAchievementType::EliteSlayer, TEXT("DebugPlayer"), true, EliteSlayerThreshold);
}

void UGS_KillFeedbackComponent::Debug_Achievement_GuardianSlayer()
{
	if (GetOwner() && GetOwner()->HasAuthority())
		TriggerAchievement(EKillAchievementType::GuardianSlayer, TEXT("DebugPlayer"), true, 0);
}

void UGS_KillFeedbackComponent::Debug_Achievement_FirstBlood()
{
	if (GetOwner() && GetOwner()->HasAuthority())
		TriggerAchievement(EKillAchievementType::FirstBlood, TEXT("DebugPlayer"), true, 0);
}

void UGS_KillFeedbackComponent::Debug_Achievement_Headhunter()
{
	if (GetOwner() && GetOwner()->HasAuthority())
		TriggerAchievement(EKillAchievementType::Headhunter, TEXT("DebugPlayer"), true, HeadhunterThreshold);
}

void UGS_KillFeedbackComponent::Debug_Achievement_Lifesaver()
{
	if (GetOwner() && GetOwner()->HasAuthority())
		TriggerAchievement(EKillAchievementType::Lifesaver, TEXT("DebugPlayer"), true, 0);
}


void UGS_KillFeedbackComponent::Debug_Achievement_All()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 안전한 약한 참조로 람다 캡처 (컴포넌트 파괴 시 크래시 방지)
	TWeakObjectPtr<UGS_KillFeedbackComponent> WeakThis(this);

	// 첫 번째 업적 즉시 표시
	Debug_Achievement_FirstBlood();

	// 고유한 타이머 핸들 사용 (각 타이머가 독립적으로 동작하도록)
	FTimerHandle Timer1, Timer2, Timer3, Timer4, Timer5, Timer6, Timer7, Timer8;

	World->GetTimerManager().SetTimer(
		Timer1,
		[WeakThis]()
		{
			if (WeakThis.IsValid())
				WeakThis->Debug_Achievement_MonsterSlayer();
		},
		2.0f,
		false);

	World->GetTimerManager().SetTimer(
		Timer2,
		[WeakThis]()
		{
			if (WeakThis.IsValid())
				WeakThis->Debug_Achievement_Rampage();
		},
		4.0f,
		false);

	World->GetTimerManager().SetTimer(
		Timer3,
		[WeakThis]()
		{
			if (WeakThis.IsValid())
				WeakThis->Debug_Achievement_Unstoppable();
		},
		6.0f,
		false);

	World->GetTimerManager().SetTimer(
		Timer4,
		[WeakThis]()
		{
			if (WeakThis.IsValid())
				WeakThis->Debug_Achievement_VeteranHunter();
		},
		8.0f,
		false);

	World->GetTimerManager().SetTimer(
		Timer5,
		[WeakThis]()
		{
			if (WeakThis.IsValid())
				WeakThis->Debug_Achievement_EliteSlayer();
		},
		10.0f,
		false);

	World->GetTimerManager().SetTimer(
		Timer6,
		[WeakThis]()
		{
			if (WeakThis.IsValid())
				WeakThis->Debug_Achievement_GuardianSlayer();
		},
		12.0f,
		false);

	World->GetTimerManager().SetTimer(
		Timer7,
		[WeakThis]()
		{
			if (WeakThis.IsValid())
				WeakThis->Debug_Achievement_Headhunter();
		},
		14.0f,
		false);

	World->GetTimerManager().SetTimer(
		Timer8,
		[WeakThis]()
		{
			if (WeakThis.IsValid())
				WeakThis->Debug_Achievement_Lifesaver();
		},
		16.0f,
		false);
}
