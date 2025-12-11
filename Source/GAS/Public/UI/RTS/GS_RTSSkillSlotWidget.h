// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GS_RTSSkillSlotWidget.generated.h"

class UImage;
class UTextBlock;
class UButton;
class UProgressBar;
class UGS_RTSSkillBase;
class UGS_RTSSkillComponent;

/**
 * 개별 스킬 슬롯 위젯
 * 스킬 아이콘, 쿨다운, 에테르 비용을 표시
 */
UCLASS()
class GAS_API UGS_RTSSkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 스킬 슬롯 초기화
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void InitializeSlot(int32 InSlotIndex, UGS_RTSSkillComponent* InSkillComponent);

	// 슬롯 인덱스 Getter
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	int32 GetSlotIndex() const { return SlotIndex; }

	// UI 업데이트
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void UpdateCooldownDisplay(float CurrentCooldown, float MaxCooldown);

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void SetSkillAvailable(bool bAvailable);

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void SetTargetingActive(bool bActive);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// UI 컴포넌트 (블루프린트에서 바인딩)
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	UButton* SkillButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	UImage* SkillIcon;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	UTextBlock* EtherCostText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	UTextBlock* HotkeyText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	UProgressBar* CooldownOverlay;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	UTextBlock* CooldownText;

	// 스킬 컴포넌트 레퍼런스
	UPROPERTY()
	TWeakObjectPtr<UGS_RTSSkillComponent> SkillComponent;

	// 슬롯 인덱스
	int32 SlotIndex;

	// 현재 쿨다운 상태
	bool bIsOnCooldown;

	// 버튼 클릭 이벤트
	UFUNCTION()
	void OnSkillButtonClicked();

	UFUNCTION()
	void OnSkillButtonHovered();

	UFUNCTION()
	void OnSkillButtonUnhovered();

	// 쿨다운 변경 이벤트 핸들러
	UFUNCTION()
	void HandleCooldownChanged(int32 InSkillIndex, float RemainingCooldown, float MaxCooldown);

	// 에테르 변경 이벤트 핸들러  
	UFUNCTION()
	void HandleEtherChanged(float CurrentEther, float MaxEther);

private:
	void RefreshSkillInfo();
};
