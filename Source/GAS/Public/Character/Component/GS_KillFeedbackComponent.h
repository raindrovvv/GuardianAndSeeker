// Copyright

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
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
	MonsterKill UMETA(DisplayName = "Monster Kill"),
	EliteKill UMETA(DisplayName = "Elite Kill"),
	BossKill UMETA(DisplayName = "Boss Kill"),
	SeekerKill UMETA(DisplayName = "Seeker Kill"),
	GuardianRepelled UMETA(DisplayName = "Guardian Repelled"),
	TeammateDying UMETA(DisplayName = "Teammate Dying"),
	TeammateDeath UMETA(DisplayName = "Teammate Death"),
	TeammateRevived UMETA(DisplayName = "Teammate Revived"),
	RescueContribution UMETA(DisplayName = "Rescue Contribution"),
	Assist UMETA(DisplayName = "Assist"),
};

/**
 * 팀 기여도 타입 (시커 팀용)
 */
UENUM(BlueprintType, Meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EContributionType : uint8
{
	None = 0,
	Damage = 1 << 0 UMETA(DisplayName = "Damage"),
	Tank = 1 << 1 UMETA(DisplayName = "Tank"),
	Support = 1 << 2 UMETA(DisplayName = "Support"),
	CC = 1 << 3 UMETA(DisplayName = "CC"),
};
ENUM_CLASS_FLAGS(EContributionType);

/**
 * 킬 업적(칭호) 타입 정의
 */
UENUM(BlueprintType)
enum class EKillAchievementType : uint8
{
	None,
	MonsterSlayer UMETA(DisplayName = "Monster Slayer"),	  // 15초 내 5킬
	Rampage UMETA(DisplayName = "Rampage"),					  // 15초 내 8킬
	Unstoppable UMETA(DisplayName = "Unstoppable"),			  // 15초 내 12킬
	VeteranHunter UMETA(DisplayName = "Veteran Hunter"),	  // 엘리트 처치
	EliteSlayer UMETA(DisplayName = "Elite Slayer"),		  // 연속 엘리트 2킬
	GuardianSlayer UMETA(DisplayName = "Guardian Slayer"),	  // 가디언(보스) 격퇴
	FirstBlood UMETA(DisplayName = "First Blood"),			  // 첫 킬
	Headhunter UMETA(DisplayName = "Headhunter"),			  // 연속 크리티컬 3회
	Lifesaver UMETA(DisplayName = "Lifesaver"),				  // 팀원 구조
	Tactician UMETA(DisplayName = "Tactician"),				  // 어시스트 5회
	BattleMaster UMETA(DisplayName = "Battle Master"),		  // 어시스트 10회
	LegendarySupport UMETA(DisplayName = "Legendary Support") // 어시스트 20회
};

/**
 * 업적 등급 (UI 스타일 결정용)
 */
UENUM(BlueprintType)
enum class EAchievementTier : uint8
{
	Bronze = 0 UMETA(DisplayName = "Bronze"),
	Silver = 1 UMETA(DisplayName = "Silver"),
	Gold = 2 UMETA(DisplayName = "Gold"),
	Platinum = 3 UMETA(DisplayName = "Platinum"),
};

/**
 * 킬 업적 정보 구조체
 */
USTRUCT(BlueprintType)
struct FKillAchievementInfo
{
	GENERATED_BODY()

	/** 업적 타입 */
	UPROPERTY(BlueprintReadOnly)
	EKillAchievementType AchievementType = EKillAchievementType::None;

	/** 업적 등급 */
	UPROPERTY(BlueprintReadOnly)
	EAchievementTier Tier = EAchievementTier::Bronze;

	/** 연속 킬 수 (해당 시) */
	UPROPERTY(BlueprintReadOnly)
	int32 KillCount = 0;

	/** 업적 달성자 이름 */
	UPROPERTY(BlueprintReadOnly)
	FString AchieverName;
};

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
 * 업적 달성 이벤트 델리게이트
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAchievementUnlocked, const FKillAchievementInfo&, AchievementInfo);

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

	/** 업적 달성 시 발생 (UI 위젯에서 바인딩) */
	UPROPERTY(BlueprintAssignable, Category = "KillFeedback")
	FOnAchievementUnlocked OnAchievementUnlocked;

	// ===== 공개 함수 =====

	/**
	 * 킬 피드백 표시 (모든 클라이언트에 브로드캐스팅)
	 * @param FeedbackType 킬 피드백 타입
	 * @param TargetName 대상 이름 (엘리트/보스용)
	 * @param InstigatorName 주체 이름 (비워두면 오너 이름 사용)
	 * @param InstigatorTeamID 주체의 팀 ID
	 * @param bIsInstigatorPlayer 주체가 실제 플레이어인지 여부 (AI 업적 필터링용)
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	void NotifyKill(EKillFeedbackType FeedbackType,
					const FString& TargetName = TEXT(""),
					const FString& InstigatorName = TEXT(""),
					uint8 InstigatorTeamID = 0,
					bool bIsInstigatorPlayer = true,
					bool bWasCritical = false);

	/**
	 * 어시스트 피드백 표시
	 * @param AssisterName 어시스트한 플레이어 이름 (주체)
	 */
	UFUNCTION(BlueprintCallable, Category = "KillFeedback")
	void NotifyAssist(const FString& AssisterName, bool bIsAssisterPlayer = true);

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
	void Client_ShowKillFeedback(EKillFeedbackType FeedbackType,
								 const FString& TargetName,
								 const FString& InstigatorName,
								 uint8 InstigatorTeamID);

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

	/** 업적 표시 RPC (본인용) */
	UFUNCTION(Client, Reliable)
	void Client_ShowAchievement(EKillAchievementType AchievementType,
								EAchievementTier Tier,
								int32 KillCount,
								const FString& AchieverName);

	/** 전역 업적 브로드캐스트 (서버 전용) */
	void BroadcastAchievementToAllPlayers(EKillAchievementType AchievementType,
										  EAchievementTier Tier,
										  int32 KillCount,
										  const FString& AchieverName);

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

	// ===== 업적 설정값 =====

	/** 연속 킬 유효 시간 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Achievement", meta = (ClampMin = "5.0", ClampMax = "30.0"))
	float KillStreakTimeWindow = 15.0f;

	/** 몬스터 슬레이어 조건 (연속 킬 수) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Achievement")
	int32 MonsterSlayerThreshold = 5;

	/** 광란 조건 (연속 킬 수) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Achievement")
	int32 RampageThreshold = 8;

	/** 멈출 수 없다 조건 (연속 킬 수) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Achievement")
	int32 UnstoppableThreshold = 12;

	/** 엘리트 슬레이어 조건 (연속 엘리트 킬 수) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Achievement")
	int32 EliteSlayerThreshold = 2;

	/** 헤드헌터 조건 (연속 크리티컬 킬 수) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Achievement")
	int32 HeadhunterThreshold = 3;

	/** 전술가 조건 (어시스트 수) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Achievement")
	int32 TacticianThreshold = 5;

	/** 배틀 마스터 조건 (어시스트 수) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Achievement")
	int32 BattleMasterThreshold = 10;

	/** 전설적 지원 조건 (어시스트 수) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Achievement")
	int32 LegendarySupportThreshold = 20;

	/** 업적 사운드 */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Achievement|Sound")
	TSoftObjectPtr<USoundBase> AchievementSound;

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

	/** 일반 킬 사운드 볼륨 배수 */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	float DefaultFeedbackVolume = 1.0f;

	/** 몬스터 킬 사운드 볼륨 배수 (잦은 발생으로 인한 피로도 감소) */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	float MonsterKillVolumeMultiplier = 0.6f;

	/** 중요 이벤트(팀원 위기 등) 사운드 볼륨 배수 */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	float ImportantEventVolumeMultiplier = 1.2f;

	/** 업적 달성 사운드 볼륨 배수 */
	UPROPERTY(EditDefaultsOnly, Category = "KillFeedback|Sound")
	float AchievementVolumeMultiplier = 1.3f;

private:
	/** 킬 피드백 위젯 생성 */
	void CreateKillFeedbackWidget();

	/** 실제 피드백 표시 처리 (클라이언트에서 실행) */
	void ShowKillFeedbackInternal(EKillFeedbackType FeedbackType,
								  const FString& TargetName,
								  const FString& InstigatorName,
								  uint8 InstigatorTeamID,
								  const FString& AssisterName = TEXT(""));

	/** 모든 플레이어에게 피드백 브로드캐스팅 (서버 전용) */
	void BroadcastToAllPlayers(EKillFeedbackType FeedbackType,
							   const FString& TargetName,
							   const FString& InstigatorName,
							   uint8 InstigatorTeamID,
							   const FString& AssisterName = TEXT(""));

	/** 피드백 타입에 따른 사운드 재생 */
	void PlayFeedbackSound(EKillFeedbackType FeedbackType, float VolumeMultiplier = 1.0f);

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

	// ===== 업적 트래킹 변수 =====

	/** 연속 킬 스트릭 카운트 (시간 윈도우 내) */
	int32 KillStreakCount = 0;

	/** 연속 엘리트 처치 카운트 */
	int32 EliteKillStreakCount = 0;

	/** 연속 크리티컬 킬 카운트 */
	int32 CriticalKillStreakCount = 0;

	/** 마지막 킬 시간 */
	float LastKillTime = 0.0f;

	/** 최고 달성 업적 (중복 방지 - 연속 킬 스트릭 내에서만) */
	EKillAchievementType HighestAchievedStreak = EKillAchievementType::None;

	/** 각 업적별 마지막 달성 시간 (쿨다운 체크용) */
	TMap<EKillAchievementType, float> AchievementLastTriggeredTime;

	/** 반복 가능 업적의 쿨다운 시간 (초) - 에디터에서 조절 가능 */
	UPROPERTY(EditDefaultsOnly, Category = "Kill Feedback|Achievement")
	float RepeatableAchievementCooldown = 60.0f;

	/** 연속 킬 타이머 */
	FTimerHandle KillStreakTimerHandle;

	/** 어시스트 카운트 */
	int32 AssistCount = 0;

	/** 어시스트 업적 체크 */
	void CheckAssistAchievements(const FString& InAssisterName, bool bInIsPlayer);

	/** 연속 킬 리셋 */
	void ResetKillStreak();

	/** 연속 킬 스트릭 로직 처리 (내부용) */
	void ProcessKillStreak(const FString& InKillerName, bool bInIsKillerPlayer);

	/** 업적 체크 및 트리거 */
	void CheckAndTriggerAchievements(EKillFeedbackType KillType,
									 const FString& InKillerName,
									 bool bInIsKillerPlayer,
									 bool bWasCritical = false);

	/** 업적 트리거 */
	void TriggerAchievement(EKillAchievementType AchievementType,
							const FString& InAchieverName,
							bool bInIsAchieverPlayer,
							int32 KillCount = 0);

	/** 업적 타입에 따른 등급 반환 */
	EAchievementTier GetTierForAchievement(EKillAchievementType AchievementType) const;

	/** 업적 표시 내부 처리 */
	void ShowAchievementInternal(EKillAchievementType AchievementType,
								 EAchievementTier Tier,
								 int32 KillCount,
								 const FString& AchieverName);

public:
	// ===== 디버그 콘솔 명령어 =====

	UFUNCTION(Exec)
	void Debug_Achievement_MonsterSlayer();

	UFUNCTION(Exec)
	void Debug_Achievement_Rampage();

	UFUNCTION(Exec)
	void Debug_Achievement_Unstoppable();

	UFUNCTION(Exec)
	void Debug_Achievement_VeteranHunter();

	UFUNCTION(Exec)
	void Debug_Achievement_EliteSlayer();

	UFUNCTION(Exec)
	void Debug_Achievement_GuardianSlayer();

	UFUNCTION(Exec)
	void Debug_Achievement_FirstBlood();

	UFUNCTION(Exec)
	void Debug_Achievement_Headhunter();

	UFUNCTION(Exec)
	void Debug_Achievement_Lifesaver();

	UFUNCTION(Exec)
	void Debug_Achievement_All();
};

#undef LOCTEXT_NAMESPACE
