// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "Character/Skill/ESkill.h"
#include "GameFramework/Actor.h"
#include "AI/GS_AIConstants.h"

// 전방 선언 (일부 UHT 버전에서는 .generated.h 앞에 위치해야 함)
class UBehaviorTree;
class UBlackboardData;
class UAISenseConfig_Sight;
class AGS_AISeeker;
class AGS_Monster;
class AGS_TrapBase;
class AGS_AIGoalTrigger;
class AGS_Seeker;
class UGS_StatComp;
class UGS_SkillComp;

// generated 헤더는 반드시 마지막 include여야 함
#include "GS_SeekerAIController.generated.h"

/**
 * 유틸리티 AI의 고수준 행동 타입
 * 
 * AI는 각 행동의 유틸리티 점수를 계산하고, 
 * 가장 점수가 높은 행동을 선택하여 수행합니다.
 */
UENUM(BlueprintType)
enum class ESeekerBehavior : uint8
{
	/** 대기 상태 - 아무 행동 없음 */
	Idle UMETA(DisplayName = "Idle"),

	/** 탐험 - 맵을 돌아다니며 목표를 찾음 */
	Explore UMETA(DisplayName = "Explore"),

	/** 전투 - 적과 교전 */
	Combat UMETA(DisplayName = "Combat"),

	/** 힐 - 체력 회복 */
	Heal UMETA(DisplayName = "Heal"),

	/** 회피 - 트랩이나 위험 요소 피하기 */
	Evade UMETA(DisplayName = "Evade"),

	/** 상호작용 - 아이템 등과 상호작용 */
	Interact UMETA(DisplayName = "Interact"),

	/** 부활 - 쓰러진 아군 부활 */
	Revive UMETA(DisplayName = "Revive"),

	/** 전술 - EQS를 통한 전술적 위치 이동 */
	Tactical UMETA(DisplayName = "Tactical")
};

/**
 * 감지된 위협 데이터 구조체
 * 
 * 각 적에 대한 위협 수준을 계산하여 저장합니다.
 * 타겟 선택 우선순위 결정에 사용됩니다.
 */
USTRUCT(BlueprintType)
struct FGS_ThreatData
{
	GENERATED_BODY()

	/** 위협이 되는 액터 (몬스터) */
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> ThreatActor;

	/** 위협 수준 (높을수록 우선 공격 대상) */
	UPROPERTY(BlueprintReadOnly)
	float ThreatLevel = 0.0f;

	/** AI와의 거리 */
	UPROPERTY(BlueprintReadOnly)
	float Distance = 0.0f;

	/** 현재 AI를 공격 중인지 여부 */
	UPROPERTY(BlueprintReadOnly)
	bool bIsAggro = false;

	FGS_ThreatData() {}
	FGS_ThreatData(AActor* InActor, float InLevel, float InDist, bool bInAggro)
	    : ThreatActor(InActor), ThreatLevel(InLevel), Distance(InDist), bIsAggro(bInAggro) {}
};

/**
 * 유틸리티 점수 구조체
 * 
 * 각 행동과 그 점수를 저장합니다.
 * UpdateUtilityScores()에서 계산되어 저장됩니다.
 */
USTRUCT(BlueprintType)
struct FGS_UtilityScore
{
	GENERATED_BODY()

	/** 행동 타입 */
	UPROPERTY(BlueprintReadOnly)
	ESeekerBehavior Behavior = ESeekerBehavior::Idle;

	/** 유틸리티 점수 (높을수록 우선 선택됨) */
	UPROPERTY(BlueprintReadOnly)
	float Score = 0.0f;

	FGS_UtilityScore() {}
	FGS_UtilityScore(ESeekerBehavior InBehavior, float InScore)
	    : Behavior(InBehavior), Score(InScore) {}
};

/**
 * 시커 AI 컨트롤러
 * 
 * 자율 AI 시커를 제어하는 컨트롤러입니다.
 * 유틸리티 기반 AI 시스템을 사용하여 상황에 맞는 최적의 행동을 선택합니다.
 * 
 * 주요 기능:
 * - 퍼셉션: 시야 기반 적/트랩 감지
 * - 유틸리티 AI: 탐험/전투/힐/회피 등 행동 선택
 * - EQS: 전술적 위치 탐색 (엄폐, 우회)
 * - 팀 시너지: 아군과의 협동 전투
 */
UCLASS()
class GAS_API AGS_SeekerAIController : public AAIController
{
	GENERATED_BODY()

public:
	AGS_SeekerAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ========================================
	// 블랙보드 키 이름 상수
	// 비헤이비어 트리와 공유되는 데이터 키
	// ========================================
	static const FName TargetEnemyKey; // 현재 공격 대상
	static const FName TargetLocationKey; // 이동 목표 위치
	static const FName GoalActorKey; // 목표 액터 (AI 골 트리거)
	static const FName CurrentHealthPercentKey; // 현재 체력 퍼센트 (0.0~1.0)
	static const FName IsInCombatKey; // 전투 중 여부
	static const FName ShouldHealKey; // 힐 필요 여부
	static const FName ShouldEvadeKey; // 회피 필요 여부
	static const FName NearbyTrapKey; // 근처 트랩
	static const FName ExplorationTargetKey; // 탐험 목표 위치
	static const FName HasReachedGoalKey; // 목표 도달 여부
	static const FName LastKnownEnemyLocationKey; // 마지막으로 알려진 적 위치
	static const FName DownedAllyKey; // 쓰러진 아군
	static const FName InteractiveItemKey; // 상호작용 가능 아이템

	// EQS 전술 위치 키 (엄폐, 우회, 후퇴)
	static const FName CoverLocationKey; // 엄폐 위치
	static const FName FlankingLocationKey; // 우회(측면) 위치
	static const FName SafeRetreatLocationKey; // 안전한 후퇴 위치

	// ========================================
	// 퍼셉션(인지) 설정
	// 적과 트랩을 감지하는 시야 설정
	// ========================================

	/** 시야 반경 (이 범위 내의 적을 감지) */
	UPROPERTY(EditAnywhere, Category = "AI|퍼셉션")
	float SightRadius = GS_AI::DEFAULT_SIGHT_RADIUS;

	/** 시야 상실 반경 (이 범위를 벗어나면 적을 잃음) */
	UPROPERTY(EditAnywhere, Category = "AI|퍼셉션")
	float LoseSightRadius = GS_AI::DEFAULT_LOSE_SIGHT_RADIUS;

	/** 주변 시야 각도 (도 단위) */
	UPROPERTY(EditAnywhere, Category = "AI|퍼셉션")
	float PeripheralVisionAngleDegrees = 90.0f;

	/** 마지막으로 본 위치에서 자동 감지 성공 범위 */
	UPROPERTY(EditAnywhere, Category = "AI|퍼셉션")
	float AutoSuccessRangeFromLastSeenLocation = GS_AI::AUTO_SUCCESS_RANGE_FROM_LAST_SEEN;

	// ========================================
	// 전투 설정
	// ========================================

	/** 기본 공격 사거리 */
	UPROPERTY(EditAnywhere, Category = "AI|전투")
	float AttackRange = GS_AI::DEFAULT_MELEE_RANGE;

	/** 힐 임계값 (이 퍼센트 이하면 힐 시도) */
	UPROPERTY(EditAnywhere, Category = "AI|전투")
	float HealThreshold = GS_AI::HEAL_THRESHOLD_DEFAULT;

	/** 위험 힐 임계값 (이 퍼센트 이하면 긴급 힐) */
	UPROPERTY(EditAnywhere, Category = "AI|전투")
	float CriticalHealThreshold = GS_AI::HEAL_THRESHOLD_CRITICAL;

	// ========================================
	// 트랩 회피 설정
	// ========================================

	/** 트랩 감지 반경 */
	UPROPERTY(EditAnywhere, Category = "AI|트랩회피")
	float TrapDetectionRadius = 500.0f;

	/** 트랩 회피 거리 (최소 이 거리만큼 떨어지려 함) */
	UPROPERTY(EditAnywhere, Category = "AI|트랩회피")
	float TrapAvoidanceDistance = 300.0f;

	// ========================================
	// 탐험 설정
	// ========================================

	/** 탐험 반경 (이 범위 내에서 탐험 포인트 탐색) */
	UPROPERTY(EditAnywhere, Category = "AI|탐험")
	float ExplorationRadius = GS_AI::DEFAULT_EXPLORATION_RADIUS;

	/** 최소 탐험 거리 (너무 가까운 위치는 제외) */
	UPROPERTY(EditAnywhere, Category = "AI|탐험")
	float MinExplorationDistance = GS_AI::MIN_EXPLORATION_DISTANCE;

	// ========================================
	// AI 상태 관리 함수
	// ========================================

	/** 공격 대상 설정 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetTargetEnemy(AActor* NewTarget);

	/** 공격 대상 해제 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void ClearTargetEnemy();

	/** 목표 액터 설정 (AI 골 트리거) */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetGoalActor(AGS_AIGoalTrigger* GoalTrigger);

	/** 목표 도달 시 호출 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void OnGoalReached();

	/** 체력 상태 업데이트 (블랙보드에 반영) */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void UpdateHealthStatus();

	/** 근처 트랩 설정 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetNearbyTrap(AGS_TrapBase* Trap);

	/** 근처 트랩 해제 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void ClearNearbyTrap();

	/** 전투 중인지 확인 */
	UFUNCTION(BlueprintPure, Category = "AI")
	bool IsInCombat() const;

	/** 제어 중인 AISeeker 래퍼 반환 */
	UFUNCTION(BlueprintPure, Category = "AI")
	AGS_AISeeker* GetControlledSeeker() const;

	/** 이동 재개 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void ResumeMovement();

	/** 힐 필요 여부 확인 */
	UFUNCTION(BlueprintPure, Category = "AI")
	bool ShouldHeal() const;

	/** 회피 필요 여부 확인 */
	UFUNCTION(BlueprintPure, Category = "AI")
	bool ShouldEvade() const;

	// ========================================
	// 유틸리티 AI 시스템
	// 각 행동의 점수를 계산하고 최적 행동 선택
	// ========================================

	/** 위협 평가 업데이트 (주변 적의 위협 수준 계산) */
	UFUNCTION(BlueprintCallable, Category = "AI|유틸리티")
	void UpdateThreatAssessment();

	/** 모든 행동의 유틸리티 점수 계산 */
	UFUNCTION(BlueprintCallable, Category = "AI|유틸리티")
	void UpdateUtilityScores();

	/** 가장 점수가 높은 행동 선택 */
	UFUNCTION(BlueprintCallable, Category = "AI|유틸리티")
	ESeekerBehavior SelectBestBehavior();

	/** 가장 우선순위 높은 위협 반환 */
	UFUNCTION(BlueprintPure, Category = "AI|유틸리티")
	AActor* GetHighestPriorityThreat() const;

	/** 정차/고착 시간 반환 (정체 감지용) */
	UFUNCTION(BlueprintPure, Category = "AI")
	float GetStationaryTime() const { return StationaryTime; }

	// 개별 유틸리티 계산 함수들
	float CalculateExploreUtility(); // 탐험 유틸리티
	float CalculateCombatUtility(); // 전투 유틸리티
	float CalculateHealUtility(); // 힐 유틸리티
	float CalculateEvadeUtility(); // 회피 유틸리티 (트랩 + 원거리 공격)
	float CalculateReviveUtility(); // 부활 유틸리티
	float CalculateTacticalUtility(); // 전술 유틸리티

	/** 현재 행동 반환 */
	UFUNCTION(BlueprintPure, Category = "AI")
	ESeekerBehavior GetCurrentBehavior() const { return CurrentBehavior; }

	/** 특정 행동의 유틸리티 점수 조회 (Hysteresis 계산용) */
	float GetUtilityScoreForBehavior(ESeekerBehavior Behavior) const;

	/** 팀 전술 정보 수집 (아군 위기 상황, 공격자 등) */
	void GetTeamTacticalIntel(float Radius, bool& bAllyInTrouble, AActor*& Attacker, bool& bAllyNearby);

	// ========================================
	// 탐험 관련 함수
	// ========================================

	/** 새로운 탐험 목표 찾기 */
	UFUNCTION(BlueprintCallable, Category = "AI|탐험")
	bool FindNewExplorationTarget();

	/** 네비게이션 가능한 랜덤 포인트 반환 */
	UFUNCTION(BlueprintCallable, Category = "AI|탐험")
	FVector GetRandomPointInNavigableRadius(float Radius) const;

	// ========================================
	// 전투 관련 함수
	// ========================================

	/** 최적의 공격 대상 찾기 */
	UFUNCTION(BlueprintCallable, Category = "AI|전투")
	AActor* FindBestTarget() const;

	/** 타겟과의 거리 계산 */
	UFUNCTION(BlueprintCallable, Category = "AI|전투")
	float GetDistanceToTarget(AActor* Target) const;

	/** 타겟이 공격 사거리 내에 있는지 확인 */
	UFUNCTION(BlueprintCallable, Category = "AI|전투")
	bool IsTargetInAttackRange(AActor* Target) const;

	// ========================================
	// 트랩 회피 관련 함수
	// ========================================

	/** 근처 트랩 감지 */
	UFUNCTION(BlueprintCallable, Category = "AI|트랩회피")
	AGS_TrapBase* DetectNearbyTraps() const;

	/** 트랩 회피 방향 계산 */
	UFUNCTION(BlueprintCallable, Category = "AI|트랩회피")
	FVector CalculateTrapAvoidanceDirection(AGS_TrapBase* Trap) const;

	/** 경로가 트랩에 막혀있는지 확인 */
	UFUNCTION(BlueprintCallable, Category = "AI|트랩회피")
	bool IsPathBlockedByTrap(const FVector& Destination) const;

	/** 타겟이 트랩 근처에 있는지 확인 */
	UFUNCTION(BlueprintPure, Category = "AI|트랩")
	bool IsTargetNearTrap(AActor* Target, float Radius = 300.0f) const;

	// 팀 시스템 오버라이드
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	// ========================================
	// 부활 관련 함수
	// ========================================

	/** 부활 키 홀드 중인지 확인 */
	UFUNCTION(BlueprintCallable, Category = "AI|부활")
	bool IsHoldingReviveKey() const { return bIsHoldingReviveKey; }

	/** 가장 가까운 건강한 아군 찾기 */
	UFUNCTION(BlueprintCallable, Category = "AI|부활")
	AActor* FindNearestHealthyAlly() const;

	/** 부활 키 홀드 상태 설정 */
	UFUNCTION(BlueprintCallable, Category = "AI|부활")
	void SetHoldingReviveKey(bool bHolding) { bIsHoldingReviveKey = bHolding; }

	/** AI 업데이트 (Tick에서 호출) */
	void UpdateAI(float DeltaTime);

	// ========================================
	// 분산 업데이트 헬퍼 (성능 최적화)
	// ========================================
	void HandleStuckDetection(float DeltaTime, APawn* ControlledPawn, class AGS_Seeker* Seeker, UWorld* World);
	void UpdateOrientation(float DeltaTime, APawn* ControlledPawn);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaTime) override;

	// ========================================
	// 퍼셉션 설정
	// ========================================

	/** 시야 감지 설정 */
	UPROPERTY(VisibleAnywhere, Category = "AI|퍼셉션")
	UAISenseConfig_Sight* SightConfig;

	/** 타겟 인지 업데이트 콜백 */
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	// ========================================
	// 비헤이비어 트리 에셋
	// ========================================

	/** 사용할 비헤이비어 트리 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|비헤이비어트리")
	TSoftObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	/** 사용할 블랙보드 데이터 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|비헤이비어트리")
	TSoftObjectPtr<UBlackboardData> BlackboardAsset;

	// ========================================
	// EQS 전술 템플릿
	// ========================================

	/** 엄폐 위치 쿼리 템플릿 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|전술")
	TSoftObjectPtr<class UEnvQuery> CoverQueryTemplate;

	/** 우회 위치 쿼리 템플릿 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|전술")
	TSoftObjectPtr<class UEnvQuery> FlankQueryTemplate;

	/** 전술 쿼리 요청 ID */
	int32 TacticalQueryRequestID = -1;

	/** 전술 쿼리 실행 */
	void RunTacticalQuery();

	/** 전술 쿼리 완료 콜백 */
	void OnTacticalQueryFinished(TSharedPtr<struct FEnvQueryResult> Result);

	/** 비동기 에셋 로드 완료 후 비헤이비어 트리 초기화 */
	void InitializeBehaviorTree();

private:
	// ========================================
	// 내부 상태 데이터
	// ========================================

	/** 제어 중인 AISeeker 래퍼 */
	UPROPERTY()
	TWeakObjectPtr<AGS_AISeeker> ControlledSeeker;

	/** 현재 타겟 적 */
	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTargetEnemy;

	/** 현재 목표 (AI 골 트리거) */
	UPROPERTY()
	TWeakObjectPtr<AGS_AIGoalTrigger> CurrentGoal;

	/** 감지된 트랩 목록 */
	UPROPERTY()
	TArray<TWeakObjectPtr<AGS_TrapBase>> DetectedTraps;

	/** 방문한 위치 목록 (탐험 시 중복 방지) */
	UPROPERTY()
	TArray<FVector> VisitedLocations;

	// ========================================
	// 캐시된 컴포넌트 (성능 최적화)
	// ========================================

	UPROPERTY()
	UGS_StatComp* CachedStatComp;

	UPROPERTY()
	UGS_SkillComp* CachedSkillComp;

	// ========================================
	// 유틸리티 AI 데이터
	// ========================================

	/** 현재 위협 목록 */
	UPROPERTY(VisibleInstanceOnly, Category = "AI|유틸리티")
	TArray<FGS_ThreatData> CurrentThreats;

	/** 현재 유틸리티 점수 목록 */
	UPROPERTY(VisibleInstanceOnly, Category = "AI|유틸리티")
	TArray<FGS_UtilityScore> UtilityScores;

	/** 현재 선택된 행동 */
	UPROPERTY(VisibleInstanceOnly, Category = "AI|유틸리티")
	ESeekerBehavior CurrentBehavior = ESeekerBehavior::Idle;

	/** 최대 위협 수준 */
	UPROPERTY(VisibleInstanceOnly, Category = "AI|유틸리티")
	float MaxThreat = 0.0f;

	// ========================================
	// 탐험 관련 내부 변수
	// ========================================

	/** 방문 판정 반경 */
	float VisitedLocationRadius = GS_AI::VISITED_LOCATION_RADIUS;

	/** 위치 방문 여부 확인 */
	bool HasVisitedLocation(const FVector& Location) const;

	/** 위치를 방문 완료로 마킹 */
	void MarkLocationVisited(const FVector& Location);

	// ========================================
	// 정체 감지 및 회피 관련
	// ========================================

	/** 마지막 위치 (정체 감지용) */
	FVector LastPosition = FVector::ZeroVector;

	/** 정지 시간 (초) */
	float StationaryTime = 0.0f;

	/** 회피 억제 여부 */
	bool bIsEvasionSuppressed = false;

	/** 회피 억제 타이머 */
	float EvasionSuppressionTimer = 0.0f;

	// ========================================
	// Tick 최적화 관련
	// ========================================

	/** 프레임 카운터 (LOD 기반 업데이트 스킵용) */
	int32 TickFrameCounter = 0;

	/** 업데이트 페이즈 (0~2, 업데이트 분산용) */
	int32 TickUpdatePhase = 0;

	/** 트랩 체크 타이머 (0.5초마다 체크) */
	float TrapCheckTimer = 0.0f;

	/** 몬스터 오버랩 체크 타이머 (0.2초마다 체크) */
	float MonsterOverlapTimer = 0.0f;

	// ========================================
	// 탐험 마커 관련
	// ========================================

	/** 마지막 마커 배치 시간 */
	float LastMarkerPlaceTime = 0.0f;

	/** 마커 배치 쿨다운 */
	const float MarkerCooldown = GS_AI::MARKER_PLACEMENT_COOLDOWN;

	/** 부활 키 홀드 상태 */
	bool bIsHoldingReviveKey = false;
};
