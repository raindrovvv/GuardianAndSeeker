// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GS_RTSSkillComponent.generated.h"

class UGS_RTSSkillBase;
class UGS_RTSSkillData;
class AGS_RTSController;

// 에테르 변경 델리게이트 (RTS 스킬 시스템용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRTSAetherChanged, float, CurrentAether, float, MaxAether);
// RTS 스킬 쿨다운 변경 델리게이트 (GS_SkillComp의 FOnSkillCooldownChanged와 이름 충돌 방지)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRTSSkillCooldownChanged, int32, SkillIndex, float, RemainingCooldown, float, MaxCooldown);
// 스킬 활성화 상태 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillActivationStateChanged, int32, SkillIndex, bool, bIsActive);

/**
 * RTS 모드에서 가디언이 사용할 수 있는 스킬들을 관리하는 컴포넌트
 * 에테르(Aether)를 자원으로 사용하여 스킬을 발동
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAS_API UGS_RTSSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_RTSSkillComponent();

	// 델리게이트
	UPROPERTY(BlueprintAssignable, Category="RTS|Skill")
	FOnRTSAetherChanged OnAetherChanged;

	UPROPERTY(BlueprintAssignable, Category="RTS|Skill")
	FOnRTSSkillCooldownChanged OnSkillCooldownChanged;

	UPROPERTY(BlueprintAssignable, Category="RTS|Skill")
	FOnSkillActivationStateChanged OnSkillActivationStateChanged;

	// 에테르 관련 Getter
	UFUNCTION(BlueprintCallable, Category="RTS|Aether")
	float GetCurrentAether() const { return CurrentAether; }

	UFUNCTION(BlueprintCallable, Category="RTS|Aether")
	float GetMaxAether() const { return MaxAether; }

	UFUNCTION(BlueprintCallable, Category="RTS|Aether")
	float GetAetherPercent() const { return MaxAether > 0 ? CurrentAether / MaxAether : 0.f; }

	// 에테르 소비/회복
	UFUNCTION(BlueprintCallable, Category="RTS|Aether")
	bool ConsumeAether(float Amount);

	UFUNCTION(BlueprintCallable, Category="RTS|Aether")
	void AddAether(float Amount);

	UFUNCTION(BlueprintCallable, Category="RTS|Aether")
	void SetAether(float Amount);

	// 스킬 관련
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	bool TryActivateSkill(int32 SkillIndex);

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	bool CanActivateSkill(int32 SkillIndex) const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	UGS_RTSSkillBase* GetSkill(int32 SkillIndex) const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	int32 GetSkillCount() const { return Skills.Num(); }

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	float GetSkillCooldownRemaining(int32 SkillIndex) const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	float GetSkillCooldownPercent(int32 SkillIndex) const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	bool IsSkillOnCooldown(int32 SkillIndex) const;

	// 스킬 타겟팅 모드
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void EnterSkillTargetingMode(int32 SkillIndex);

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void ExitSkillTargetingMode();

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void ExecuteSkillAtLocation(const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	bool IsInSkillTargetingMode() const { return bIsInTargetingMode; }

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	int32 GetTargetingSkillIndex() const { return TargetingSkillIndex; }

	// 디버그
	UFUNCTION(BlueprintCallable, Category="RTS|Debug")
	void DebugPrintSkillStatus() const;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 에테르 설정
	UPROPERTY(EditDefaultsOnly, Category="RTS|Aether", meta=(ClampMin="0.0"))
	float MaxAether;

	UPROPERTY(EditDefaultsOnly, Category="RTS|Aether", meta=(ClampMin="0.0"))
	float InitialAether;

	UPROPERTY(EditDefaultsOnly, Category="RTS|Aether", meta=(ClampMin="0.0"))
	float AetherRegenRate;

	// 스킬 데이터 에셋 설정 (에디터에서 지정)
	UPROPERTY(EditDefaultsOnly, Category="RTS|Skill")
	TArray<TObjectPtr<UGS_RTSSkillData>> SkillDataAssets;

private:
	// 현재 에테르
	UPROPERTY(ReplicatedUsing=OnRep_CurrentAether)
	float CurrentAether;

	// 스킬 인스턴스들
	UPROPERTY()
	TArray<UGS_RTSSkillBase*> Skills;

	// 스킬별 쿨다운 타이머
	UPROPERTY()
	TArray<FTimerHandle> CooldownTimers;

	UPROPERTY()
	TArray<float> CooldownRemaining;

	// 타겟팅 모드
	bool bIsInTargetingMode;
	int32 TargetingSkillIndex;

	// 에테르 회복 타이머
	FTimerHandle AetherRegenTimer;
	
	// 쿨다운 UI 업데이트 타이머
	FTimerHandle CooldownUpdateTimer;
	void UpdateCooldowns();

	UFUNCTION()
	void OnRep_CurrentAether();

	void InitializeSkills();
	void StartAetherRegen();
	void RegenAether();
	void StartSkillCooldown(int32 SkillIndex);
	void OnSkillCooldownFinished(int32 SkillIndex);

	// 서버 RPC
	UFUNCTION(Server, Reliable)
	void Server_ActivateSkill(int32 SkillIndex, const FVector& TargetLocation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnSkillActivated(int32 SkillIndex, const FVector& TargetLocation);
};
