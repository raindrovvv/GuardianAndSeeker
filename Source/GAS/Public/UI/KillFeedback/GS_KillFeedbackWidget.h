// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Character/Component/GS_KillFeedbackComponent.h"
#include "GS_KillFeedbackWidget.generated.h"

#define LOCTEXT_NAMESPACE "GS_KillFeedback"

class UTextBlock;
class UVerticalBox;
class UCanvasPanel;

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
	 * @param FeedbackType 피드백 타입
	 * @param TargetName 대상 이름
	 * @param InstigatorName 주체 이름
	 * @return 표시할 텍스트
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	FText GetDisplayTextForType(EKillFeedbackType FeedbackType, const FString& TargetName, const FString& InstigatorName) const;

	/**
	 * 피드백 타입이 중앙 배너를 사용해야 하는지 반환
	 * @param FeedbackType 피드백 타입
	 * @return 중앙 배너 사용 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	bool ShouldUseCenterBanner(EKillFeedbackType FeedbackType) const;

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

	/** 팀원 빈사 상태 메시지 (구조 필요) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText TeammateDyingText = FText::FromString(TEXT("동료가 위험합니다! 구조하세요!"));

	/** 팀원 사망 메시지 (구조 실패) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText TeammateDeathText = FText::FromString(TEXT("동료를 잃었습니다..."));

	/** 팀원 구조 완료 메시지 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText TeammateRevivedText = FText::FromString(TEXT("동료 구조 완료!"));

	/** 구조 기여 메시지 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText RescueContributionText = FText::FromString(TEXT("동료를 구출했습니다!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|Text")
	FText AssistText = FText::FromString(TEXT("어시스트!"));

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
};

#undef LOCTEXT_NAMESPACE
