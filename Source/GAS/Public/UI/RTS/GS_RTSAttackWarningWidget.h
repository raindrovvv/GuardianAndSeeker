// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GS_RTSAttackWarningWidget.generated.h"

class UTextBlock;

/**
 * RTS Attack Warning Widget
 * Displays "유닛이 공격받고 있습니다" notification when units are attacked
 * Auto-hides after a configurable duration
 */
UCLASS()
class GAS_API UGS_RTSAttackWarningWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Show the warning message
	 * If already visible, resets the timer (latest notification only)
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|Notification")
	void ShowWarning();

	/**
	 * Hide the warning message
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|Notification")
	void HideWarning();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Warning text display (bound in Blueprint) */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> WarningText;

	/** Display duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="RTS|Notification")
	float DisplayDuration = 2.5f;

private:
	/** Timer handle for auto-hide */
	FTimerHandle HideTimer;
};
