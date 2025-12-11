// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GS_RTSSkillBarWidget.generated.h"

class UHorizontalBox;
class UProgressBar;
class UTextBlock;
class UGS_RTSSkillSlotWidget;
class UGS_RTSSkillComponent;

/**
 * RTS 스킬 바 위젯
 * 에테르 바 + 4개 스킬 슬롯으로 구성
 */
UCLASS()
class GAS_API UGS_RTSSkillBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 스킬 바 초기화
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void InitializeSkillBar(UGS_RTSSkillComponent* InSkillComponent);

	// 에테르 바 업데이트
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void UpdateEtherBar(float CurrentEther, float MaxEther);

	// 특정 슬롯 업데이트
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void UpdateSkillSlot(int32 SlotIndex);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// C++로 UI 컴포넌트 생성
	void CreateUIComponents();

	// 에테르 바 UI (블루프린트에서 바인딩 또는 C++로 생성)
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	UProgressBar* EtherProgressBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	UTextBlock* EtherText;

	// 스킬 슬롯 컨테이너
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	UHorizontalBox* SkillSlotContainer;

	// 스킬 슬롯 위젯 클래스 (블루프린트에서 지정 가능, 없으면 C++ 기본 클래스 사용)
	UPROPERTY(EditDefaultsOnly, Category="RTS|Skill")
	TSubclassOf<UGS_RTSSkillSlotWidget> SkillSlotWidgetClass;

	// 스킬 컴포넌트 레퍼런스
	UPROPERTY()
	TWeakObjectPtr<UGS_RTSSkillComponent> SkillComponent;

	// 생성된 스킬 슬롯들
	UPROPERTY()
	TArray<UGS_RTSSkillSlotWidget*> SkillSlots;

	// 에테르 변경 이벤트 핸들러
	UFUNCTION()
	void HandleEtherChanged(float CurrentEther, float MaxEther);

	// 스킬 쿨다운 변경 이벤트 핸들러
	UFUNCTION()
	void HandleSkillCooldownChanged(int32 SkillIndex, float RemainingCooldown, float MaxCooldown);

	// 스킬 활성화 상태 변경 핸들러
	UFUNCTION()
	void HandleSkillActivationStateChanged(int32 SkillIndex, bool bIsActive);

private:
	void CreateSkillSlots();
	void BindToSkillComponent();
};
