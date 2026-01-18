#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GS_DungeonEditorWidget.generated.h"

class UCommonButtonBase;

UCLASS()
class GAS_API UGS_DungeonEditorWidget : public UUserWidget
{
	GENERATED_BODY()

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> LoadButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> SaveButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> ResetButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> BackButton;

	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnSaveButtonClicked();
	UFUNCTION()
	void OnLoadButtonClicked();
	UFUNCTION()
	void OnBackButtonClicked();
	UFUNCTION()
	void OnResetButtonClicked();

private:
	// 버튼 연속 입력 방지 예외 처리
	void SetInputCooldown(float Duration = 1.0f);
	void OnInputCooldownExpired();

	bool bIsProcessing = false;
	FTimerHandle InputCooldownTimer;
};
