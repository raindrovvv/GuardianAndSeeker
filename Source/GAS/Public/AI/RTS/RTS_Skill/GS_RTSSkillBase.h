// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GS_RTSSkillTypes.h"
#include "GS_RTSSkillBase.generated.h"

class UGS_RTSSkillComponent;
class AGS_RTSController;
class UGS_RTSSkillData;
class UParticleSystem;
class USoundBase;
class UTexture2D;

/**
 * RTS 스킬의 기본 클래스
 * 모든 가디언 RTS 스킬은 이 클래스를 상속받아 구현
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class GAS_API UGS_RTSSkillBase : public UObject
{
	GENERATED_BODY()

public:
	UGS_RTSSkillBase();

	// 스킬 정보 Getter
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	FText GetSkillName() const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	FText GetSkillDescription() const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	UTexture2D* GetSkillIcon() const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	float GetAetherCost() const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	float GetCooldownTime() const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	ERTSSkillTargetType GetTargetType() const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	float GetSkillRange() const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	float GetEffectRadius() const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	float GetSkillPower() const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	float GetEffectDuration() const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	UParticleSystem* GetActivationVFX() const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	USoundBase* GetCastSound() const;

	// 스킬 발동 가능 여부 체크
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	virtual bool CanActivate(UGS_RTSSkillComponent* SkillComponent) const;

	// 스킬 발동
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	virtual void ActivateSkill(UGS_RTSSkillComponent* SkillComponent, const FVector& TargetLocation);

	// 스킬 초기화 (컴포넌트에서 호출)
	virtual void Initialize(UGS_RTSSkillComponent* OwnerComponent);

	// 단축키 인덱스 (0-3 = 1-4키)
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	int32 GetSkillSlotIndex() const { return SkillSlotIndex; }

	void SetSkillSlotIndex(int32 Index) { SkillSlotIndex = Index; }

	void SetSkillData(UGS_RTSSkillData* InSkillData);
	const UGS_RTSSkillData* GetSkillData() const { return SkillData.Get(); }

protected:
	// 소유 컴포넌트 레퍼런스
	UPROPERTY()
	TWeakObjectPtr<UGS_RTSSkillComponent> OwnerSkillComponent;

	// 스킬 데이터 에셋
	UPROPERTY(BlueprintReadOnly, Category="RTS|Skill", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UGS_RTSSkillData> SkillData;

	int32 SkillSlotIndex;

	// 헬퍼 함수
	AGS_RTSController* GetRTSController() const;
	UWorld* GetSkillWorld() const;

	// 블루프린트에서 오버라이드 가능한 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category="RTS|Skill", meta=(DisplayName="On Skill Activated"))
	void BP_OnSkillActivated(const FVector& TargetLocation);

	// VFX/SFX 재생 헬퍼
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void PlaySkillVFX(UParticleSystem* ParticleSystem, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void PlaySkillSound(USoundBase* Sound, const FVector& Location);
};
