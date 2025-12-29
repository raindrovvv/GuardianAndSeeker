// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GS_InteractionWidget.generated.h"

/**
 * 상호작용 진행도 표시 위젯
 * E키를 누르고 있는 동안 진행도 표시
 */
UCLASS()
class GAS_API UGS_InteractionWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** 상호작용 UI 표시 시작 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ShowInteraction(AActor* Target, float Duration, const FText& ActionText);

	/** 상호작용 UI 숨기기 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void HideInteraction();

	/** 진행도 업데이트 (0.0 ~ 1.0) */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void UpdateProgress(float Progress);

	/** 상호작용 완료 시 호출 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OnInteractionComplete();

	/** 상호작용 취소 시 호출 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OnInteractionCancelled();

	/** 근처 상호작용 가능 알림 표시 ("E 획득" 등) */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ShowNearbyIndicator(AActor* Target, const FText& ActionText);

	/** 근처 상호작용 가능 알림 숨기기 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void HideNearbyIndicator();

protected:
	virtual void NativeConstruct() override;

	// ==========================================
	// UI 컴포넌트 바인딩 (이름을 꼭 맞춰야 함)
	// ==========================================

	/** 진행도 바 (이름: ProgressBar_Interaction) */
	UPROPERTY(meta = (BindWidgetOptional))
	class UProgressBar* ProgressBar_Interaction;

	/** 행동 이름 텍스트 (이름: Text_ActionName) */
	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* Text_ActionName;

	/** 키 안내 텍스트/이미지 등을 포함한 컨테이너 (이름: InteractionPromptBox) */
	UPROPERTY(meta = (BindWidgetOptional))
	class UWidget* InteractionPromptBox;

	/** 근처 알림 텍스트 (이름: Text_NearbyInfo) */
	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* Text_NearbyInfo;

	/** 근처 알림 컨테이너 (이름: NearbyIndicatorBox) */
	UPROPERTY(meta = (BindWidgetOptional))
	class UWidget* NearbyIndicatorBox;
};
