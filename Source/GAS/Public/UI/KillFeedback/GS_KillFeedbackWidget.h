// Copyright

#pragma once

#include "Blueprint/UserWidget.h"
#include "Character/Component/GS_KillFeedbackComponent.h"
#include "CoreMinimal.h"
#include "GS_KillFeedbackWidget.generated.h"


#define LOCTEXT_NAMESPACE "GS_KillFeedback"

class UTextBlock;
class UVerticalBox;
class UCanvasPanel;
class UImage;
class UTexture2D;

/**
 * 킬 피드 항목 구조체 (UI 표시용)
 */
USTRUCT(BlueprintType)
struct FKillFeedEntry
{
	GENERATED_BODY()

	/** 킬 피드백 타입 */
	UPROPERTY(BlueprintReadOnly)
	EKillFeedbackType FeedbackType = EKillFeedbackType::None;

	/** 표시할 텍스트 */
	UPROPERTY(BlueprintReadOnly)
	FString DisplayText;

	/** 표시 시작 시간 */
	UPROPERTY(BlueprintReadOnly)
	float DisplayStartTime = 0.0f;

	/** 대상 이름 (엘리트/보스용) */
	UPROPERTY(BlueprintReadOnly)
	FString TargetName;

	/** 행동의 주체 이름 (킬러, 구조자 등) */
	UPROPERTY(BlueprintReadOnly)
	FString InstigatorName;

	/** 팀 ID (색상 결정용) */
	UPROPERTY(BlueprintReadOnly)
	uint8 TeamID = 0;
};

/**
 * 킬/어시스트 피드백 UI 위젯
 *
 * 화면에 킬 피드백을 표시합니다:
 * - 중앙 배너: 중요 이벤트 (엘리트/보스 킬, 가디언 격퇴, 팀원 쓰러짐)
 * - 좌측 하단 킬피드: 일반 킬/어시스트 로그
 * - 업적 배너: 특수 조건 달성 시 칭호 표시
 *
 * 블루프린트에서 상속하여 비주얼을 커스터마이즈할 수 있습니다.
 */
UCLASS(Blueprintable, BlueprintType)
class GAS_API UGS_KillFeedbackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 초기화 (컴포넌트에서 호출)
	 * @param InOwnerComponent 소유 컴포넌트
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	void InitializeWidget(UGS_KillFeedbackComponent* InOwnerComponent);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ===== 블루프린트 구현 가능 함수 =====

	/**
	 * 킬 피드백 수신 시 호출 (블루프린트에서 오버라이드)
	 * @param FeedbackInfo 킬 피드백 정보
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "KillFeedback")
	void OnKillFeedbackReceived(const FKillFeedbackInfo& FeedbackInfo);

	/**
	 * 중앙 배너 표시 (블루프린트에서 오버라이드)
	 * @param FeedbackType 피드백 타입
	 * @param DisplayText 표시할 텍스트
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "KillFeedback")
	void ShowCenterBanner(EKillFeedbackType FeedbackType, const FString& DisplayText);

	/**
	 * 킬 피드에 항목 추가 (블루프린트에서 오버라이드)
	 * @param Entry 킬 피드 항목
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "KillFeedback")
	void AddKillFeedEntry(const FKillFeedEntry& Entry);

	/**
	 * 피드백 타입에 따른 표시 텍스트 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	FText GetDisplayTextForType(EKillFeedbackType FeedbackType,
								const FString& TargetName,
								const FString& InstigatorName) const;

	/**
	 * 피드백 타입이 중앙 배너를 사용해야 하는지 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	bool ShouldUseCenterBanner(EKillFeedbackType FeedbackType) const;

	/**
	 * 업적 배너 표시 (블루프린트에서 오버라이드)
	 * @param AchievementInfo 업적 정보
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "KillFeedback")
	void ShowAchievementBanner(const FKillAchievementInfo& AchievementInfo);

	/**
	 * 업적 타입에 따른 표시 텍스트 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	FText GetAchievementDisplayText(EKillAchievementType AchievementType) const;

	/**
	 * 업적 등급에 따른 색상 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	FLinearColor GetAchievementTierColor(EAchievementTier Tier) const;

	// ===== 설정값 =====

	/** 킬 피드 최대 항목 수 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Settings")
	int32 MaxKillFeedEntries = 5;

	/** 킬 피드 항목 표시 시간 (초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Settings")
	float KillFeedDisplayDuration = 3.0f;

	/** 중앙 배너 표시 시간 (초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Settings")
	float CenterBannerDisplayDuration = 2.0f;

	// ===== 텍스트 설정 (로컬라이징용) =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText MonsterKillText = FText::FromString(TEXT("몬스터 처치!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText EliteKillText = FText::FromString(TEXT("ELITE SLAIN!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText BossKillText = FText::FromString(TEXT("BOSS CONQUERED!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText SeekerKillText = FText::FromString(TEXT("EXECUTION COMPLETED"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText GuardianRepelledText = FText::FromString(TEXT("GUARDIAN REPELLED!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText TeammateDyingText = FText::FromString(TEXT("동료가 위험합니다! 구조하세요!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText TeammateDeathText = FText::FromString(TEXT("동료를 잃었습니다..."));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText TeammateRevivedText = FText::FromString(TEXT("동료 구조 완료!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText RescueContributionText = FText::FromString(TEXT("동료를 구출했습니다!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText AssistText = FText::FromString(TEXT("어시스트!"));

	// ===== 업적 텍스트 설정 =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	FText MonsterSlayerText = FText::FromString(TEXT("몬스터 슬레이어!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	FText RampageText = FText::FromString(TEXT("광란!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	FText UnstoppableText = FText::FromString(TEXT("멈출 수 없다!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	FText VeteranHunterText = FText::FromString(TEXT("베테랑 헌터!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	FText EliteSlayerText = FText::FromString(TEXT("엘리트 슬레이어!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	FText GuardianSlayerText = FText::FromString(TEXT("가디언 슬레이어!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	FText FirstBloodText = FText::FromString(TEXT("퍼스트 블러드!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	FText HeadhunterText = FText::FromString(TEXT("헤드헌터!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	FText LifesaverText = FText::FromString(TEXT("생명의 은인!"));

	// ===== 업적 등급 색상 =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	FLinearColor BronzeColor = FLinearColor(0.8f, 0.5f, 0.2f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	FLinearColor SilverColor = FLinearColor(0.75f, 0.75f, 0.8f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	FLinearColor GoldColor = FLinearColor(1.0f, 0.84f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	FLinearColor PlatinumColor = FLinearColor(0.9f, 0.95f, 1.0f);

	// ===== 업적별 아이콘 설정 =====

	/** 업적 타입별 아이콘 텍스처 (에디터에서 설정) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement|Icon")
	TMap<EKillAchievementType, TSoftObjectPtr<UTexture2D>> AchievementIcons;

	/** 기본 업적 아이콘 (설정되지 않은 업적에 사용) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement|Icon")
	TSoftObjectPtr<UTexture2D> DefaultAchievementIcon;

	/**
	 * 업적 타입에 따른 아이콘 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	UTexture2D* GetAchievementIcon(EKillAchievementType AchievementType) const;

	/** 업적 배너 표시 시간 (초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Achievement")
	float AchievementBannerDisplayDuration = 2.5f;

	// ===== UI 바인딩 (선택적) =====

	/** 킬 피드 컨테이너 (어떤 패널이든 가능) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> KillFeedContainer;

	/** 중앙 배너 텍스트 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CenterBannerText;

	/** 중앙 배너 컨테이너 (어떤 위젯이든 가능) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CenterBannerContainer;

	// ===== 업적 배너 UI 바인딩 =====

	/** 업적 배너 컨테이너 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> AchievementBannerContainer;

	/** 업적 아이콘 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> AchievementIcon;

	/** 업적 타이틀 텍스트 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AchievementTitleText;

	/** 업적 부제 (킬 수 등) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AchievementSubtitleText;

private:
	/** 소유 컴포넌트 참조 */
	UPROPERTY()
	TWeakObjectPtr<UGS_KillFeedbackComponent> OwnerComponent;

	/** 현재 킬 피드 항목들 */
	UPROPERTY()
	TArray<FKillFeedEntry> KillFeedEntries;

	/** 중앙 배너 타이머 핸들 */
	FTimerHandle CenterBannerTimerHandle;

	/** 피드백 수신 핸들러 */
	UFUNCTION()
	void HandleKillFeedbackReceived(const FKillFeedbackInfo& FeedbackInfo);

	/** 만료된 킬 피드 항목 정리 */
	void CleanupExpiredEntries();

	/** 중앙 배너 숨기기 */
	void HideCenterBanner();

	/** 업적 수신 핸들러 */
	UFUNCTION()
	void HandleAchievementUnlocked(const FKillAchievementInfo& AchievementInfo);

	/** 업적 UI 데이터를 실제로 설정하는 함수 (BP Override에 안전) */
	void UpdateAchievementUI(const FKillAchievementInfo& AchievementInfo);

	/** 업적 배너 숨기기 */
	void HideAchievementBanner();

	/** 업적 배너 타이머 */
	FTimerHandle AchievementBannerTimerHandle;

	/** 업적 대기열 */
	TArray<FKillAchievementInfo> AchievementQueue;

	/** 현재 배너가 표시 중인지 여부 */
	bool bIsAchievementBannerShowing = false;

	/** 대기열의 다음 업적 처리 */
	void ProcessNextAchievementInQueue();

	/** 업적 아이콘 비동기 프리로드 */
	void PreloadAchievementIcons();

	/** 로드된 아이콘 캐시 (GC 방지) */
	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> CachedAchievementIcons;
};

#undef LOCTEXT_NAMESPACE
