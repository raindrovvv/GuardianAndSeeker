// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Character/E_Character.h"
#include "Character/Skill/ESkill.h"
#include "GS_AISeeker.generated.h"

class AGS_Seeker;
class AGS_Ares;
class AGS_Chan;
class AGS_Merci;
class AGS_SeekerAIController;
class UGS_SkillComp;
class UGS_StatComp;

/**
 * AI 시커 타입 열거형
 * 
 * 각 시커 캐릭터의 전투 스타일과 특성을 정의합니다.
 * - Ares: 근접 검 전사, 대쉬로 거리를 좁히고 콤보 공격
 * - Chan: 근접 탱커, 방패로 아군 보호
 * - Merci: 원거리 궁수, 엄폐물 활용 및 화살 공격
 */
UENUM(BlueprintType)
enum class ESeekerAIType : uint8
{
	/** 아레스 - 검 (근접 딜러, 대쉬 스킬) */
	Ares UMETA(DisplayName = "Ares (Sword)"),

	/** 첸 - 도끼 & 방패 (근접 탱커, 방패 스킬) */
	Chan UMETA(DisplayName = "Chan (Axe & Shield)"),

	/** 메르시 - 활 (원거리 딜러, 안개 화살 스킬) */
	Merci UMETA(DisplayName = "Merci (Bow)")
};

/**
 * AI 시커 래퍼 클래스
 * 
 * AI가 제어하는 시커를 스폰하고 관리하는 래퍼 액터입니다.
 * 레벨에 직접 배치하여 사용하며, 에디터에서 시커 타입을 선택할 수 있습니다.
 * 
 * 사용 방법:
 * 1. 레벨에 BP_AISeeker를 배치
 * 2. SeekerType을 원하는 캐릭터로 설정
 * 3. 필요시 HealThreshold, AttackRange 등 조정
 * 
 * 게임 시작 시 자동으로:
 * - 선택된 타입의 시커 캐릭터 스폰
 * - AI 컨트롤러 생성 및 빙의
 * - 비헤이비어 트리 시작
 */
UCLASS(Blueprintable, BlueprintType)
class GAS_API AGS_AISeeker : public AActor
{
	GENERATED_BODY()

public:
	AGS_AISeeker();

	// ========================================
	// 시커 설정 (에디터에서 조정)
	// ========================================

	/** 스폰할 시커 타입 선택 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI 시커|설정", meta = (DisplayPriority = 1))
	ESeekerAIType SeekerType = ESeekerAIType::Ares;

	/** 아레스 블루프린트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI 시커|설정")
	TSoftClassPtr<AGS_Ares> AresClass;

	/** 첸 블루프린트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI 시커|설정")
	TSoftClassPtr<AGS_Chan> ChanClass;

	/** 메르시 블루프린트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI 시커|설정")
	TSoftClassPtr<AGS_Merci> MerciClass;

	/** AI 컨트롤러 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI 시커|설정")
	TSoftClassPtr<AGS_SeekerAIController> AIControllerClass;

	// ========================================
	// AI 행동 설정
	// ========================================

	/** 힐 시작 체력 임계값 (0.0~1.0, 이 이하면 힐 시도) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI 시커|행동")
	float HealThreshold = 0.3f;

	/** 공격 사거리 (메르시는 자동으로 1200으로 조정됨) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI 시커|행동")
	float AttackRange = 200.0f;

	/** 트랩 감지 반경 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI 시커|행동")
	float TrapDetectionRadius = 500.0f;

	/** 게임 시작 시 자동 탐험 시작 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI 시커|행동")
	bool bAutoStartExploration = true;

	// ========================================
	// 디버그 설정
	// ========================================

	/** 디버그 정보 표시 여부 (체력, 사거리 등) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI 시커|디버그")
	bool bShowDebugInfo = false;

	// ========================================
	// 게터 함수들
	// ========================================

	/** 제어 중인 시커 캐릭터 반환 */
	UFUNCTION(BlueprintPure, Category = "AI 시커")
	AGS_Seeker* GetControlledSeeker() const { return SpawnedSeeker; }

	/** AI 컨트롤러 반환 */
	UFUNCTION(BlueprintPure, Category = "AI 시커")
	AGS_SeekerAIController* GetSeekerAIController() const;

	/** 스킬 컴포넌트 반환 */
	UFUNCTION(BlueprintPure, Category = "AI 시커")
	UGS_SkillComp* GetSkillComp() const;

	/** 스탯 컴포넌트 반환 */
	UFUNCTION(BlueprintPure, Category = "AI 시커")
	UGS_StatComp* GetStatComp() const;

	/** 시커 타입 반환 */
	UFUNCTION(BlueprintPure, Category = "AI 시커")
	ESeekerAIType GetSeekerType() const { return SeekerType; }

	/** 근접 시커 여부 (Ares 또는 Chan) - 우회 전술에 사용 */
	UFUNCTION(BlueprintPure, Category = "AI 시커")
	bool IsMeleeSeeker() const { return SeekerType == ESeekerAIType::Ares || SeekerType == ESeekerAIType::Chan; }

	/** 원거리 시커 여부 (Merci) - 엄폐 전술에 사용 */
	UFUNCTION(BlueprintPure, Category = "AI 시커")
	bool IsRangedSeeker() const { return SeekerType == ESeekerAIType::Merci; }

	// ========================================
	// AI 전투 제어 함수
	// ========================================

	/** 
	 * 공격 수행
	 * 현재 상황에 맞는 최적의 콤보/스킬을 선택하여 실행합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI 시커|전투")
	void PerformAttack();

	/** 
	 * 특정 스킬 사용
	 * @param SkillIndex 스킬 슬롯 인덱스 (ESkillSlot)
	 */
	UFUNCTION(BlueprintCallable, Category = "AI 시커|전투")
	void PerformSkill(int32 SkillIndex);

	/** 힐 포션 사용 */
	UFUNCTION(BlueprintCallable, Category = "AI 시커|전투")
	void PerformHeal();

	/** 
	 * 구르기 수행
	 * @param Direction 구르기 방향 (자동으로 몬스터 회피 방향 계산)
	 */
	UFUNCTION(BlueprintCallable, Category = "AI 시커|전투")
	void PerformRoll(FVector Direction);

	/** 모든 액션 정지 (몽타주, 스킬, 이동 등) */
	UFUNCTION(BlueprintCallable, Category = "AI 시커|전투")
	void StopAllActions();

	/** 
	 * 현재 전투 상황에 맞는 최적 스킬 콤보 계산
	 * 
	 * 캐릭터별 전술:
	 * - Ares: 거리가 멀면 대쉬, 가까우면 궁극기+콤보
	 * - Chan: 아군 위기 시 방패, 그 외 궁극기+콤보  
	 * - Merci: 고립 시 안개 화살, 그 외 궁극기+콤보
	 * 
	 * @param DistanceToEnemy 적과의 거리
	 * @return 추천 스킬 슬롯 배열 (우선순위 순)
	 */
	UFUNCTION(BlueprintPure, Category = "AI 시커|전투")
	TArray<ESkillSlot> GetOptimalCombo(float DistanceToEnemy) const;

	// ========================================
	// 상태 확인 함수
	// ========================================

	/** 현재 체력 퍼센트 반환 (0.0~1.0) */
	UFUNCTION(BlueprintPure, Category = "AI 시커|상태")
	float GetHealthPercent() const;

	/** 생존 여부 확인 */
	UFUNCTION(BlueprintPure, Category = "AI 시커|상태")
	bool IsAlive() const;

	/** 특정 스킬 사용 가능 여부 확인 */
	UFUNCTION(BlueprintPure, Category = "AI 시커|상태")
	bool CanUseSkill(int32 SkillIndex) const;

	/** 힐 사용 가능 여부 확인 */
	UFUNCTION(BlueprintPure, Category = "AI 시커|상태")
	bool CanHeal() const;

	// ========================================
	// 이벤트 델리게이트
	// ========================================

	/** 시커 스폰 완료 시 발생 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAISeekerSpawned, AGS_Seeker*, Seeker);

	/** 시커 사망 시 발생 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAISeekerDeath);

	/** 목표 도달 시 발생 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAISeekerReachedGoal);

	UPROPERTY(BlueprintAssignable, Category = "AI 시커|이벤트")
	FOnAISeekerSpawned OnSeekerSpawned;

	UPROPERTY(BlueprintAssignable, Category = "AI 시커|이벤트")
	FOnAISeekerDeath OnSeekerDeath;

	UPROPERTY(BlueprintAssignable, Category = "AI 시커|이벤트")
	FOnAISeekerReachedGoal OnReachedGoal;

	// ========================================
	// 목표 처리
	// ========================================

	/** 목표 도달 알림 (외부에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "AI 시커")
	void NotifyGoalReached();

	// ========================================
	// 메르시 전용 - 화살 제어
	// ========================================

	/** 랜덤 특수 화살로 변경 (도끼/분열 화살) */
	UFUNCTION(BlueprintCallable, Category = "AI 시커|전투")
	void SwitchToRandomSpecialArrow();

	/** 일반 화살로 복귀 */
	UFUNCTION(BlueprintCallable, Category = "AI 시커|전투")
	void ResetToNormalArrow();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
	/** 에디터 프로퍼티 변경 시 호출 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	// ========================================
	// 내부 데이터
	// ========================================

	/** 스폰된 시커 캐릭터 */
	UPROPERTY()
	AGS_Seeker* SpawnedSeeker;

	/** AI 컨트롤러 */
	UPROPERTY()
	AGS_SeekerAIController* SeekerController;

	/** 캐시된 스킬 컴포넌트 */
	UPROPERTY()
	UGS_SkillComp* CachedSkillComp;

	/** 캐시된 스탯 컴포넌트 */
	UPROPERTY()
	UGS_StatComp* CachedStatComp;

	// ========================================
	// 내부 함수
	// ========================================

	/** 시커 캐릭터 스폰 */
	void SpawnSeeker();

	/** AI 컨트롤러 설정 및 빙의 */
	void SetupAIController();

	/** 타입에 맞는 시커 클래스 반환 */
	TSubclassOf<AGS_Seeker> GetSeekerClassByType() const;

	/** 시커 사망 핸들러 */
	UFUNCTION()
	void HandleSeekerDeath();

	// ========================================
	// 메르시 활 홀드 로직 (AI 안전 폴백)
	// ========================================

	/** 활 당기기 시작 시간 */
	float DrawStartTime = 0.0f;

	/** 메르시 최대 홀드 시간 */
	const float MerciMaxHoldDuration = 1.5f;

	// ========================================
	// 타이머 기반 스킬 로직
	// ========================================

	/** 아레스 대쉬 타이머 */
	FTimerHandle AresDashTimer;

	/** 첸 방패 타이머 */
	FTimerHandle ChanShieldTimer;

	/** 메르시 이동 스킬(안개) 타이머 */
	FTimerHandle MerciMovingSkillTimer;

	/** 아레스 대쉬 실행 (타이머 콜백) */
	void ExecuteAresDash();

	/** 첸 방패 해제 (타이머 콜백) */
	void StopChanShield();

	/** 메르시 이동 스킬 실행 (타이머 콜백) */
	void ExecuteMerciMovingSkill();

	// ========================================
	// 디버그
	// ========================================

	/** 디버그 정보 그리기 */
	void DrawDebugInfo() const;
};
