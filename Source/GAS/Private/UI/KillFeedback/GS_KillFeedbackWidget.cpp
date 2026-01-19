#include "UI/KillFeedback/GS_KillFeedbackWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "GS_KillFeedback"

void UGS_KillFeedbackWidget::InitializeWidget(UGS_KillFeedbackComponent* InOwnerComponent)
{
	if (!InOwnerComponent)
	{
		return;
	}

	OwnerComponent = InOwnerComponent;

	// 델리게이트 바인딩
	OwnerComponent->OnKillFeedbackReceived.AddDynamic(this, &UGS_KillFeedbackWidget::HandleKillFeedbackReceived);
	OwnerComponent->OnAchievementUnlocked.AddDynamic(this, &UGS_KillFeedbackWidget::HandleAchievementUnlocked);
}

void UGS_KillFeedbackWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 초기 상태: 중앙 배너 숨김
	if (CenterBannerContainer)
	{
		CenterBannerContainer->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 초기 상태: 업적 배너 숨김
	if (AchievementBannerContainer)
	{
		AchievementBannerContainer->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 업적 아이콘 비동기 프리로드 (첫 표시 시 끊김 방지)
	PreloadAchievementIcons();
}

void UGS_KillFeedbackWidget::NativeDestruct()
{
	// 델리게이트 해제
	if (OwnerComponent.IsValid())
	{
		OwnerComponent->OnKillFeedbackReceived.RemoveDynamic(this, &UGS_KillFeedbackWidget::HandleKillFeedbackReceived);
		OwnerComponent->OnAchievementUnlocked.RemoveDynamic(this, &UGS_KillFeedbackWidget::HandleAchievementUnlocked);
	}

	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CenterBannerTimerHandle);
		World->GetTimerManager().ClearTimer(AchievementBannerTimerHandle);
	}

	Super::NativeDestruct();
}

void UGS_KillFeedbackWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 만료된 킬 피드 항목 정리
	CleanupExpiredEntries();
}

void UGS_KillFeedbackWidget::HandleKillFeedbackReceived(const FKillFeedbackInfo& FeedbackInfo)
{
	// 블루프린트로 이벤트 전달 (비주얼 연출 등)
	OnKillFeedbackReceived(FeedbackInfo);
}

void UGS_KillFeedbackWidget::OnKillFeedbackReceived_Implementation(const FKillFeedbackInfo& FeedbackInfo)
{
	FText DisplayText =
		GetDisplayTextForType(FeedbackInfo.FeedbackType, FeedbackInfo.TargetName, FeedbackInfo.InstigatorName);

	if (ShouldUseCenterBanner(FeedbackInfo.FeedbackType))
	{
		ShowCenterBanner(FeedbackInfo.FeedbackType, DisplayText.ToString());
	}
	else
	{
		FKillFeedEntry Entry;
		Entry.FeedbackType = FeedbackInfo.FeedbackType;
		Entry.DisplayText = DisplayText.ToString();
		Entry.TargetName = FeedbackInfo.TargetName;
		Entry.InstigatorName = FeedbackInfo.InstigatorName;
		Entry.TeamID = FeedbackInfo.TeamID;

		if (UWorld* World = GetWorld())
		{
			Entry.DisplayStartTime = World->GetTimeSeconds();
		}

		AddKillFeedEntry(Entry);
	}
}

void UGS_KillFeedbackWidget::ShowCenterBanner_Implementation(EKillFeedbackType FeedbackType, const FString& DisplayText)
{
	// 기본 구현: 중앙 배너 텍스트 설정 및 표시
	if (CenterBannerText)
	{
		CenterBannerText->SetText(FText::FromString(DisplayText));
	}

	if (CenterBannerContainer)
	{
		CenterBannerContainer->SetVisibility(ESlateVisibility::Visible);
	}

	// 타이머로 자동 숨김
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CenterBannerTimerHandle);
		World->GetTimerManager().SetTimer(CenterBannerTimerHandle,
										  this,
										  &UGS_KillFeedbackWidget::HideCenterBanner,
										  CenterBannerDisplayDuration,
										  false);
	}
}

void UGS_KillFeedbackWidget::AddKillFeedEntry_Implementation(const FKillFeedEntry& Entry)
{
	// KillFeedContainer가 없으면 무시
	if (!KillFeedContainer)
	{
		return;
	}

	// UI 자식 개수가 최대치 이상이면 가장 오래된 항목들 제거
	while (KillFeedContainer->GetChildrenCount() >= MaxKillFeedEntries)
	{
		UWidget* OldestChild = KillFeedContainer->GetChildAt(0);
		if (OldestChild)
		{
			OldestChild->RemoveFromParent();
		}
		else
		{
			break; // 안전장치
		}
	}

	// 데이터 배열도 동기화
	while (KillFeedEntries.Num() >= MaxKillFeedEntries)
	{
		KillFeedEntries.RemoveAt(0);
	}

	KillFeedEntries.Add(Entry);

	// 새 TextBlock 생성
	UTextBlock* NewTextBlock = NewObject<UTextBlock>(this);
	if (!NewTextBlock)
	{
		return;
	}

	// 텍스트 설정
	NewTextBlock->SetText(FText::FromString(Entry.DisplayText));

	// 팀 ID에 따른 색상 설정 (0: 시커팀-파랑, 1: 가디언팀-빨강, 기타: 흰색)
	FSlateColor TextColor;
	switch (Entry.TeamID)
	{
		case 0:														 // 시커 팀
			TextColor = FSlateColor(FLinearColor(0.2f, 0.6f, 1.0f)); // 밝은 파랑
			break;
		case 1:														 // 가디언 팀
			TextColor = FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f)); // 밝은 빨강
			break;
		default:
			TextColor = FSlateColor(FLinearColor::White);
			break;
	}
	NewTextBlock->SetColorAndOpacity(TextColor);

	// 폰트 크기 설정
	FSlateFontInfo FontInfo = NewTextBlock->GetFont();
	FontInfo.Size = 16;
	NewTextBlock->SetFont(FontInfo);

	// 그림자 효과 추가 (가독성 향상)
	NewTextBlock->SetShadowOffset(FVector2D(1.0f, 1.0f));
	NewTextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));

	// KillFeedContainer에 추가
	KillFeedContainer->AddChild(NewTextBlock);
}

FText UGS_KillFeedbackWidget::GetDisplayTextForType(EKillFeedbackType FeedbackType,
													const FString& TargetName,
													const FString& InstigatorName) const
{
	FText InstigatorText = FText::FromString(InstigatorName);
	FText TargetText = FText::FromString(TargetName);

	switch (FeedbackType)
	{
		case EKillFeedbackType::MonsterKill:
			if (!InstigatorName.IsEmpty())
			{
				return FText::Format(LOCTEXT("MonsterKillWithKiller", "{0} - {1}"), InstigatorText, MonsterKillText);
			}
			return MonsterKillText;

		case EKillFeedbackType::EliteKill:
			if (!InstigatorName.IsEmpty() && !TargetName.IsEmpty())
			{
				return FText::Format(
					LOCTEXT("EliteKillWithDetails", "{0} - {1} ({2})"), InstigatorText, EliteKillText, TargetText);
			}
			return EliteKillText;

		case EKillFeedbackType::BossKill:
			if (!InstigatorName.IsEmpty() && !TargetName.IsEmpty())
			{
				return FText::Format(
					LOCTEXT("BossKillWithDetails", "{0} - {1}\n{2}"), InstigatorText, BossKillText, TargetText);
			}
			return BossKillText;

		case EKillFeedbackType::SeekerKill:
			if (!InstigatorName.IsEmpty() && !TargetName.IsEmpty())
			{
				return FText::Format(LOCTEXT("SeekerExecuted", "{0} executed {1}"), InstigatorText, TargetText);
			}
			return SeekerKillText;

		case EKillFeedbackType::GuardianRepelled:
			if (!InstigatorName.IsEmpty())
			{
				return FText::Format(
					LOCTEXT("GuardianRepelledWithKiller", "{0} - {1}"), InstigatorText, GuardianRepelledText);
			}
			return GuardianRepelledText;

		case EKillFeedbackType::TeammateDying:
			if (!TargetName.IsEmpty())
			{
				return FText::Format(LOCTEXT("TeammateInDanger", "{0} is in danger! Rescue them!"), TargetText);
			}
			return TeammateDyingText;

		case EKillFeedbackType::TeammateDeath:
			if (!TargetName.IsEmpty())
			{
				return FText::Format(LOCTEXT("TeammateDied", "{0} has died..."), TargetText);
			}
			return TeammateDeathText;

		case EKillFeedbackType::TeammateRevived:
			if (!TargetName.IsEmpty() && !InstigatorName.IsEmpty())
			{
				return FText::Format(LOCTEXT("TeammateRevivedBy", "{0} rescued {1}!"), InstigatorText, TargetText);
			}
			else if (!TargetName.IsEmpty())
			{
				return FText::Format(LOCTEXT("TeammateRevivedSimple", "{0} rescued!"), TargetText);
			}
			return TeammateRevivedText;

		case EKillFeedbackType::RescueContribution:
			if (!TargetName.IsEmpty() && !InstigatorName.IsEmpty())
			{
				return FText::Format(
					LOCTEXT("RescueContributionLog", "{0} contributed to rescuing {1}!"), InstigatorText, TargetText);
			}
			return RescueContributionText;

		case EKillFeedbackType::Assist:
			if (!InstigatorName.IsEmpty())
			{
				return FText::Format(LOCTEXT("AssistLog", "{0} - {1}"), InstigatorText, AssistText);
			}
			return AssistText;

		default:
			return FText::GetEmpty();
	}
}

bool UGS_KillFeedbackWidget::ShouldUseCenterBanner(EKillFeedbackType FeedbackType) const
{
	switch (FeedbackType)
	{
		case EKillFeedbackType::EliteKill:
		case EKillFeedbackType::BossKill:
		case EKillFeedbackType::SeekerKill:
		case EKillFeedbackType::GuardianRepelled:
		case EKillFeedbackType::TeammateDying:
		case EKillFeedbackType::TeammateDeath:
		case EKillFeedbackType::TeammateRevived:
			return true;

		case EKillFeedbackType::MonsterKill:
		case EKillFeedbackType::Assist:
		case EKillFeedbackType::RescueContribution: // 구조 기여는 킬 피드로 표시
		default:
			return false;
	}
}

void UGS_KillFeedbackWidget::CleanupExpiredEntries()
{
	if (!GetWorld())
	{
		return;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();

	for (int32 i = KillFeedEntries.Num() - 1; i >= 0; --i)
	{
		if (CurrentTime - KillFeedEntries[i].DisplayStartTime > KillFeedDisplayDuration)
		{
			// UI 위젯도 함께 제거 (인덱스 동기화)
			if (KillFeedContainer && i < KillFeedContainer->GetChildrenCount())
			{
				if (UWidget* ChildToRemove = KillFeedContainer->GetChildAt(i))
				{
					ChildToRemove->RemoveFromParent();
				}
			}
			KillFeedEntries.RemoveAt(i);
		}
	}
}

void UGS_KillFeedbackWidget::HideCenterBanner()
{
	if (CenterBannerContainer)
	{
		CenterBannerContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ===== 업적 배너 구현 =====

void UGS_KillFeedbackWidget::HandleAchievementUnlocked(const FKillAchievementInfo& AchievementInfo)
{
	// 1. 대기열에 추가
	AchievementQueue.Add(AchievementInfo);

	// 2. 현재 표시 중인 배너가 없다면 즉시 처리 시작
	if (!bIsAchievementBannerShowing)
	{
		ProcessNextAchievementInQueue();
	}
}

void UGS_KillFeedbackWidget::ProcessNextAchievementInQueue()
{
	if (AchievementQueue.Num() <= 0)
	{
		bIsAchievementBannerShowing = false;
		HideAchievementBanner(); // 더 이상 보여줄 것이 없음
		return;
	}

	bIsAchievementBannerShowing = true;

	// 대기열의 첫 번째 항목 꺼내기
	FKillAchievementInfo NextAchievement = AchievementQueue[0];
	AchievementQueue.RemoveAt(0);

	// C++ UI 업데이트 강제 호출
	UpdateAchievementUI(NextAchievement);

	// 블루프린트로 이벤트 전달 (애니메이션 시작용)
	ShowAchievementBanner(NextAchievement);
}

void UGS_KillFeedbackWidget::UpdateAchievementUI(const FKillAchievementInfo& AchievementInfo)
{
	// 등급별 색상 한 번만 계산
	const FLinearColor TierColor = GetAchievementTierColor(AchievementInfo.Tier);

	// 업적 아이콘 설정
	if (AchievementIcon)
	{
		if (UTexture2D* IconTexture = GetAchievementIcon(AchievementInfo.AchievementType))
		{
			AchievementIcon->SetBrushFromTexture(IconTexture);
			AchievementIcon->SetVisibility(ESlateVisibility::Visible);

			// 등급별 아이콘 색상 틴트 적용 (선택적)
			AchievementIcon->SetColorAndOpacity(TierColor);
		}
		else
		{
			AchievementIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 업적 타이틀 텍스트 설정
	if (AchievementTitleText)
	{
		FText TitleText = GetAchievementDisplayText(AchievementInfo.AchievementType);

		// 달성자 이름이 있는 경우 (전역 알림 등) 이름 추가
		if (!AchievementInfo.AchieverName.IsEmpty())
		{
			TitleText = FText::Format(LOCTEXT("AchievementWithAchiever", "{0}: {1}"),
									  FText::FromString(AchievementInfo.AchieverName),
									  TitleText);
		}

		AchievementTitleText->SetText(TitleText);

		// 등급별 색상 적용
		AchievementTitleText->SetColorAndOpacity(FSlateColor(TierColor));
	}

	// 업적 부제 (킬 수) 설정
	if (AchievementSubtitleText)
	{
		if (AchievementInfo.KillCount > 0)
		{
			FText SubtitleText = FText::Format(LOCTEXT("AchievementKillCount", "{0}킬 달성!"),
											   FText::AsNumber(AchievementInfo.KillCount));
			AchievementSubtitleText->SetText(SubtitleText);
			AchievementSubtitleText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			AchievementSubtitleText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 업적 배너 표시 (강제 Visible)
	if (AchievementBannerContainer)
	{
		AchievementBannerContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
	}

	// 타이머로 자동 숨김
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AchievementBannerTimerHandle);
		World->GetTimerManager().SetTimer(AchievementBannerTimerHandle,
										  this,
										  &UGS_KillFeedbackWidget::ProcessNextAchievementInQueue, // 다음 항목으로
										  AchievementBannerDisplayDuration,
										  false);
	}
}

void UGS_KillFeedbackWidget::ShowAchievementBanner_Implementation(const FKillAchievementInfo& AchievementInfo)
{
	// 블루프린트 애니메이션 등에서 가시성을 조절할 수 있으므로,
	// C++ 수준에서 최소한 컨테이너는 다시 한번 보이도록 설정
	if (AchievementBannerContainer)
	{
		AchievementBannerContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	// 블루프린트에서 '부모 호출'을 했을 때만 이 로그가 찍힙니다.
}

FText UGS_KillFeedbackWidget::GetAchievementDisplayText(EKillAchievementType AchievementType) const
{
	switch (AchievementType)
	{
		case EKillAchievementType::MonsterSlayer:
			return MonsterSlayerText;
		case EKillAchievementType::Rampage:
			return RampageText;
		case EKillAchievementType::Unstoppable:
			return UnstoppableText;
		case EKillAchievementType::VeteranHunter:
			return VeteranHunterText;
		case EKillAchievementType::EliteSlayer:
			return EliteSlayerText;
		case EKillAchievementType::GuardianSlayer:
			return GuardianSlayerText;
		case EKillAchievementType::FirstBlood:
			return FirstBloodText;
		case EKillAchievementType::Headhunter:
			return HeadhunterText;
		case EKillAchievementType::Lifesaver:
			return LifesaverText;
		default:
			return FText::GetEmpty();
	}
}

FLinearColor UGS_KillFeedbackWidget::GetAchievementTierColor(EAchievementTier Tier) const
{
	FLinearColor OutColor = FLinearColor::White;
	switch (Tier)
	{
		case EAchievementTier::Bronze:
			OutColor = BronzeColor;
			break;
		case EAchievementTier::Silver:
			OutColor = SilverColor;
			break;
		case EAchievementTier::Gold:
			OutColor = GoldColor;
			break;
		case EAchievementTier::Platinum:
			OutColor = PlatinumColor;
			break;
		default:
			OutColor = FLinearColor::White;
			break;
	}

	// 색상이 너무 투명하면(A < 0.1) 최소한 보이도록 백색 폴백
	if (OutColor.A < 0.1f)
	{
		return FLinearColor::White;
	}
	return OutColor;
}

void UGS_KillFeedbackWidget::HideAchievementBanner()
{
	if (AchievementBannerContainer)
	{
		AchievementBannerContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UTexture2D* UGS_KillFeedbackWidget::GetAchievementIcon(EKillAchievementType AchievementType) const
{
	// 업적 타입에 해당하는 아이콘 검색
	if (const TSoftObjectPtr<UTexture2D>* FoundIcon = AchievementIcons.Find(AchievementType))
	{
		if (!FoundIcon->IsNull())
		{
			// 동기 로드 (이미 로드되어 있으면 즉시 반환)
			return FoundIcon->LoadSynchronous();
		}
	}

	// 기본 아이콘 반환
	if (!DefaultAchievementIcon.IsNull())
	{
		return DefaultAchievementIcon.LoadSynchronous();
	}

	return nullptr;
}

void UGS_KillFeedbackWidget::PreloadAchievementIcons()
{
	TArray<FSoftObjectPath> AssetsToLoad;

	// 모든 업적 아이콘 수집
	for (const auto& Pair : AchievementIcons)
	{
		if (!Pair.Value.IsNull())
		{
			AssetsToLoad.Add(Pair.Value.ToSoftObjectPath());
		}
	}

	// 기본 아이콘도 추가
	if (!DefaultAchievementIcon.IsNull())
	{
		AssetsToLoad.Add(DefaultAchievementIcon.ToSoftObjectPath());
	}

	if (AssetsToLoad.Num() == 0)
	{
		return;
	}

	// 안전한 약한 참조 사용 (위젯 파괴 시 크래시 방지)
	TWeakObjectPtr<UGS_KillFeedbackWidget> WeakThis(this);

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
						if (UTexture2D* Texture = Cast<UTexture2D>(Asset))
						{
							WeakThis->CachedAchievementIcons.Add(Texture);
						}
					}
				}
			}));
}

#undef LOCTEXT_NAMESPACE
