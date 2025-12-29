// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AkGameplayStatics.h"
#include "BehaviorTree/BlackboardData.h"
#include "Character/GS_Character.h"
#include "Sound/GS_MonsterAudioComponent.h"
#include "GS_Monster.generated.h"


class UWidgetComponent;
class UGS_MonsterSkillComp;
class UGS_MonsterAnimInstance;
class UGS_VFXComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMonsterDead, AGS_Monster*,
											DeadUnit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMonsterAttacked, AGS_Monster*,
											 AttackedUnit, FVector,
											 AttackLocation);

UCLASS()
class GAS_API AGS_Monster : public AGS_Character
{
	GENERATED_BODY()

public:
	AGS_Monster();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "RTS")
	bool bCommandLocked;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "RTS")
	bool bSelectionLocked;

	UPROPERTY(EditAnywhere, Category = "AI")
	UBehaviorTree* BTAsset;

	UPROPERTY(EditAnywhere, Category = "AI")
	UBlackboardData* BBAsset;

	UPROPERTY(EditAnywhere, Category = "Attack")
	UAnimMontage* AttackMontage;

	UPROPERTY(BlueprintAssignable, Category = "Dead")
	FOnMonsterDead OnMonsterDead;

	UPROPERTY(BlueprintAssignable, Category = "RTS|Notification")
	FOnMonsterAttacked OnMonsterAttacked;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
	TObjectPtr<UWidgetComponent> SkillCooldownWidgetComp;

	// 전투 음악 관련 (BGM 이벤트만 유지, 트리거는 제거)
	UPROPERTY(EditAnywhere, Category = "Combat")
	UAkAudioEvent* CombatMusicEvent;

	UPROPERTY(EditAnywhere, Category = "Combat")
	UAkAudioEvent* CombatMusicStopEvent;

	// 몬스터 오디오 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	class UGS_MonsterAudioComponent* MonsterAudioComponent;

	// VFX 컴포넌트 (디버프 등 모든 VFX)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX")
	UGS_VFXComponent* VFXComponent;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDeath();

	FORCEINLINE bool IsCommandable() const { return !bCommandLocked; }
	FORCEINLINE bool IsSelectable() const { return !bSelectionLocked; }

	void SetSelected(bool bSelected, bool bPlaySound = true);

	virtual void SetCanUseSkill(bool bCanUse) override;

	UFUNCTION(BlueprintCallable, Category = "AI")
	virtual void Attack();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackMontage();

	FORCEINLINE UGS_MonsterSkillComp* GetMonsterSkillComp() const
	{
		return MonsterSkillComp;
	}

	UFUNCTION(BlueprintCallable, Category = "Skill")
	virtual void UseSkill();

	UFUNCTION()
	virtual void ApplyStiffness();

	UFUNCTION()
	virtual void EndStiffness();

	void ShowTargetUI(bool bIsActive);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
	TObjectPtr<UGS_MonsterSkillComp> MonsterSkillComp;

	UPROPERTY()
	TObjectPtr<UGS_MonsterAnimInstance> MonsterAnim;

	UPROPERTY(VisibleAnywhere)
	UAkComponent* AkComponent;

	// 몬스터 조준 3D UI
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	UWidgetComponent* TargetedUIComponent;

	void HandleDelayedDestroy();
	virtual void OnDeath() override;

	UFUNCTION()
	void HandleSkillCooldownChanged(float InCurrentCoolTime, float InMaxCoolTime);

	UFUNCTION()
	void HandleHPChanged(UGS_StatComp* InStatComp);

	virtual FLinearColor GetCurrentDecalColor() override;
	virtual void UpdateDecal() override;
	virtual bool ShowDecal() override;

	/** 몬스터 크기에 따른 최적 컬링 거리 반환 (자식 클래스에서 오버라이드) */
	virtual float GetOptimalCullDistance() const;

	/** 네트워크 업데이트 빈도 최적화 (거리 기반) */
	void UpdateNetworkOptimization();

	/** 그림자 컬링 최적화 (거리 기반) */
	void UpdateShadowCulling();

private:
	bool bIsSelected;

	/** Cache for TargetedUI visibility to avoid redundant updates */
	bool bIsTargetUIActive;

	/** Tracks previous HP for damage detection (not healing) */
	float LastKnownHP;

	/** 네트워크 최적화 업데이트 타이머 (1초마다 체크) */
	FTimerHandle NetworkOptimizationTimerHandle;

	/** 마지막으로 설정한 NetUpdateFrequency (변경 감지용) */
	float LastNetUpdateFrequency;
};
