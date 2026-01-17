#include "UI/KillFeedback/GS_KillFeedbackWidget.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanel.h"
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
}

void UGS_KillFeedbackWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 초기 상태: 중앙 배너 숨김
	if (CenterBannerContainer)
	{
		CenterBannerContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UGS_KillFeedbackWidget::NativeDestruct()
{
	// 델리게이트 해제
	if (OwnerComponent.IsValid())
	{
		OwnerComponent->OnKillFeedbackReceived.RemoveDynamic(this, &UGS_KillFeedbackWidget::HandleKillFeedbackReceived);
	}

	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CenterBannerTimerHandle);
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
	FText DisplayText = GetDisplayTextForType(FeedbackInfo.FeedbackType, FeedbackInfo.TargetName, FeedbackInfo.InstigatorName);

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
		World->GetTimerManager().SetTimer(
		    CenterBannerTimerHandle,
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
	case 0: // 시커 팀
		TextColor = FSlateColor(FLinearColor(0.2f, 0.6f, 1.0f)); // 밝은 파랑
		break;
	case 1: // 가디언 팀
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

FText UGS_KillFeedbackWidget::GetDisplayTextForType(EKillFeedbackType FeedbackType, const FString& TargetName, const FString& InstigatorName) const
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
			return FText::Format(LOCTEXT("EliteKillWithDetails", "{0} - {1} ({2})"), InstigatorText, EliteKillText, TargetText);
		}
		return EliteKillText;

	case EKillFeedbackType::BossKill:
		if (!InstigatorName.IsEmpty() && !TargetName.IsEmpty())
		{
			return FText::Format(LOCTEXT("BossKillWithDetails", "{0} - {1}\n{2}"), InstigatorText, BossKillText, TargetText);
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
			return FText::Format(LOCTEXT("GuardianRepelledWithKiller", "{0} - {1}"), InstigatorText, GuardianRepelledText);
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
			return FText::Format(LOCTEXT("RescueContributionLog", "{0} contributed to rescuing {1}!"), InstigatorText, TargetText);
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

#undef LOCTEXT_NAMESPACE
