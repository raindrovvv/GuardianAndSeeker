#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GS_KillFeedbackComponent.generated.h"

#define LOCTEXT_NAMESPACE "GS_KillFeedback"

class UUserWidget;
class UGS_KillFeedbackWidget;
class USoundBase;

/**
 * 킬 피드백 타입 정의
 */
UENUM(BlueprintType)
enum class EKillFeedbackType : uint8
{
	None,
	MonsterKill UMETA(DisplayName = "Monster Kill"), // 일반 몬스터 처치
	EliteKill UMETA(DisplayName = "Elite Kill"), // 엘리트 몬스터 처치
	BossKill UMETA(DisplayName = "Boss Kill"), // 보스 처치
	SeekerKill UMETA(DisplayName = "Seeker Kill"), // 시커 처치 (가디언 측)
	GuardianRepelled UMETA(DisplayName = "Guardian Repelled"), // 가디언 격퇴 (시커 측)
	TeammateDying UMETA(DisplayName = "Teammate Dying"), // 팀원 빈사 상태 (시커 측 - 구조 필요!)
	TeammateDeath UMETA(DisplayName = "Teammate Death"), // 팀원 완전 사망 (시커 측 - 구조 실패)
	TeammateRevived UMETA(DisplayName = "Teammate Revived"), // 팀원 구조 완료 (시커 측)
	RescueContribution UMETA(DisplayName = "Rescue Contribution"), // 구조 기여 표시 (구조한 플레이어에게)
	Assist UMETA(DisplayName = "Assist"), // 어시스트
};

/**
 * 팀 기여도 타입 (시커 팀용)
 */
UENUM(BlueprintType, Meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EContributionType : uint8
{
	None = 0,
	Damage = 1 << 0 UMETA(DisplayName = "Damage"), // 🎯 딜 기여
	Tank = 1 << 1 UMETA(DisplayName = "Tank"), // 🛡️ 탱킹 기여
	Support = 1 << 2 UMETA(DisplayName = "Support"), // ➕ 힐/서포트 기여
	CC = 1 << 3 UMETA(DisplayName = "CC"), // ⚡ CC 기여
};
ENUM_CLASS_FLAGS(EContributionType);

/**
 * 킬 피드백 정보 구조체
 */
USTRUCT(BlueprintType)
struct FKillFeedbackInfo
{
	GENERATED_BODY()

	/** 킬 피드백 타입 */
	UPROPERTY(BlueprintReadOnly)
	EKillFeedbackType FeedbackType = EKillFeedbackType::None;

	/** 처치한 대상의 이름 (엘리트/보스용) */
	UPROPERTY(BlueprintReadOnly)
	FString TargetName;

	/** 행동의 주체 이름 (킬러, 구조자 등) */
	UPROPERTY(BlueprintReadOnly)
	FString InstigatorName;

	/** 처치에 기여한 다른 플레이어 이름 (어시스트 등 - 필요 시 사용) */
	UPROPERTY(BlueprintReadOnly)
	FString AssisterName;

	/** 행동 주체의 팀 ID (우리 편인지 확인용) */
	UPROPERTY(BlueprintReadOnly)
	uint8 TeamID = 0;

	/** 팀 기여도 플래그 (시커 팀용) */
	UPROPERTY(BlueprintReadOnly, meta = (Bitmask, BitmaskEnum = EContributionType))
	uint8 ContributionFlags = 0;
};


/**
 * 킬 피드백 이벤트 델리게이트
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKillFeedbackReceived, const FKillFeedbackInfo&, FeedbackInfo);

/**
 * 킬/어시스트 피드백 시스템 컴포넌트
 * 
 * 적 처치 또는 어시스트 시 시각/청각 피드백을 제공합니다.
 * - 시커: 몬스터 처치, 가디언 격퇴, 팀원 쓰러짐 등
 * - 가디언: 시커 처치, 유닛 손실 등
 * 
 * 로컬 플레이어만 피드백을 표시합니다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GAS_API UGS_KillFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_KillFeedbackComponent();

	// ===== 이벤트 델리게이트 =====

	/** 킬 피드백 수신 시 발생 (UI 위젯에서 바인딩) */
	UPROPERTY(BlueprintAssignable, Category = "KillFeedback")
	FOnKillFeedbackReceived OnKillFeedbackReceived;

	// ===== 공개 함수 =====

	/**
	 * 킬 피드백 표시 (모든 클라이언트에 브로드캐스팅)
	 * @param FeedbackType 킬 피드백 타입
	 * @param TargetName 대상 이름 (엘리트/보스용)
	 * @param InstigatorName 주체 이름 (비워두면 오너 이름 사용)
	 * @param InstigatorTeamID 주체의 팀 ID
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	void NotifyKill(EKillFeedbackType FeedbackType, const FString& TargetName = TEXT(""), const FString& InstigatorName = TEXT(""), uint8 InstigatorTeamID = 0);

	/**
	 * 어시스트 피드백 표시
	 * @param AssisterName 어시스트한 플레이어 이름 (주체)
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	void NotifyAssist(const FString& AssisterName);

	/**
	 * 팀원 빈사 상태 알림 (시커 전용 - 구조 필요)
	 * @param DyingPlayerName 빈사 상태가 된 팀원 이름 (Target)
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	void NotifyTeammateDying(const FString& DyingPlayerName);

	/**
	 * 팀원 사망 알림 (시커 전용 - 구조 실패)
	 * @param DeadPlayerName 사망한 팀원 이름 (Target)
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	void NotifyTeammateDeath(const FString& DeadPlayerName);

	/**
	 * 팀원 구조 완료 알림 (시커 전용)
	 * @param RevivedPlayerName 구조된 팀원 이름 (Target)
	 * @param ReviverName 구조한 플레이어 이름 (Instigator)
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	void NotifyTeammateRevived(const FString& RevivedPlayerName, const FString& ReviverName);

	/**
	 * 구조 기여 알림 (구조한 플레이어에게 표시)
	 * @param RevivedPlayerName 구조된 팀원 이름
	 * @param InstigatorName 기여한 플레이어 이름
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	void NotifyRescueContribution(const FString& RevivedPlayerName, const FString& InstigatorName);

	/**
	 * 팀 기여도 표시 (가디언 격퇴 시 시커 팀용)
	 * @param ContributorNames 기여자 이름 배열
	 * @param ContributionFlags 각 기여자의 기여도 플래그 배열
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	void NotifyTeamContribution(const TArray<FString>& ContributorNames, const TArray<uint8>& ContributionFlags);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ===== RPC 함수 =====

	/** 클라이언트 RPC - 킬 피드백 표시 */
	UFUNCTION(Client, Reliable)
	void Client_ShowKillFeedback(EKillFeedbackType FeedbackType, const FString& TargetName, const FString& InstigatorName, uint8 InstigatorTeamID);

	/** 클라이언트 RPC - 어시스트 피드백 표시 */
	UFUNCTION(Client, Reliable)
	void Client_ShowAssistFeedback(const FString& AssisterName);

	/** 클라이언트 RPC - 팀원 빈사 알림 */
	UFUNCTION(Client, Reliable)
	void Client_ShowTeammateDyingFeedback(const FString& DyingPlayerName);

	/** 클라이언트 RPC - 팀원 사망 알림 */
	UFUNCTION(Client, Reliable)
	void Client_ShowTeammateDeathFeedback(const FString& DeadPlayerName);

	/** 클라이언트 RPC - 팀원 구조 완료 알림 */
	UFUNCTION(Client, Reliable)
	void Client_ShowTeammateRevivedFeedback(const FString& RevivedPlayerName, const FString& ReviverName);

	/** 클라이언트 RPC - 구조 기여 알림 */
	UFUNCTION(Client, Reliable)
	void Client_ShowRescueContributionFeedback(const FString& RevivedPlayerName, const FString& InstigatorName);

	// ===== 설정값 =====

	/** 킬 피드백 위젯 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "KillFeedback|UI")
	TSubclassOf<UGS_KillFeedbackWidget> KillFeedbackWidgetClass;

	/** 킬 피드 로그 최대 개수 */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Settings", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxKillFeedEntries = 5;

	/** 킬 피드 항목 표시 시간 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Settings", meta = (ClampMin = "1.0"))
	float KillFeedDisplayDuration = 3.0f;

	// ===== 사운드 설정 =====

	/** 일반 몬스터 킬 사운드 */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	TSoftObjectPtr<USoundBase> MonsterKillSound;

	/** 엘리트 킬 사운드 */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	TSoftObjectPtr<USoundBase> EliteKillSound;

	/** 보스 킬 사운드 */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	TSoftObjectPtr<USoundBase> BossKillSound;

	/** 가디언 격퇴 사운드 (시커용) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	TSoftObjectPtr<USoundBase> GuardianRepelledSound;

	/** 시커 처치 사운드 (가디언용) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	TSoftObjectPtr<USoundBase> SeekerKillSound;

	/** 어시스트 사운드 */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	TSoftObjectPtr<USoundBase> AssistSound;

	/** 팀원 빈사 경고 사운드 (구조 필요) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	TSoftObjectPtr<USoundBase> TeammateDyingSound;

	/** 팀원 사망 사운드 (구조 실패) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	TSoftObjectPtr<USoundBase> TeammateDeathSound;

	/** 팀원 구조 완료 사운드 */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	TSoftObjectPtr<USoundBase> TeammateRevivedSound;

	/** 구조 기여 사운드 */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	TSoftObjectPtr<USoundBase> RescueContributionSound;

private:
	/** 킬 피드백 위젯 생성 */
	void CreateKillFeedbackWidget();

	/** 실제 피드백 표시 처리 (클라이언트에서 실행) */
	void ShowKillFeedbackInternal(EKillFeedbackType FeedbackType, const FString& TargetName, const FString& InstigatorName, uint8 InstigatorTeamID, const FString& AssisterName = TEXT(""));

	/** 모든 플레이어에게 피드백 브로드캐스팅 (서버 전용) */
	void BroadcastToAllPlayers(EKillFeedbackType FeedbackType, const FString& TargetName, const FString& InstigatorName, uint8 InstigatorTeamID, const FString& AssisterName = TEXT(""));

	/** 피드백 타입에 따른 사운드 재생 */
	void PlayFeedbackSound(EKillFeedbackType FeedbackType);

	/** 현재 표시 중인 킬 피드백 위젯 */
	UPROPERTY()
	TObjectPtr<UGS_KillFeedbackWidget> KillFeedbackWidget;

	/** 연속 처치 몬스터 수 (Throttling용) */
	int32 MonsterKillCount = 0;
	FTimerHandle MonsterKillThrottlingTimer;

	/** 몬스터 처치 로그 전송 지연 처리 */
	void SendThrottledMonsterKill();

	/** 비동기 로드된 사운드 에셋 캐시 (GC 방지 및 즉시 재생용) */
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> CachedLoadedSounds;
};

#undef LOCTEXT_NAMESPACE
