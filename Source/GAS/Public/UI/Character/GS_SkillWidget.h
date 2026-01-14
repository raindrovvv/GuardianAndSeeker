#pragma once

#include "CoreMinimal.h"
#include "Character/Skill//GS_SkillComp.h"
#include "Blueprint/UserWidget.h"
#include "GS_SkillWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class AGS_Player;

UCLASS()
class GAS_API UGS_SkillWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGS_SkillWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//skill image and reset cool time
	void InitSkill(UGS_SkillBase* Skill);

	UFUNCTION()
	void OnSkillCoolTimeChanged(ESkillSlot InSkillSlot, float InCurrentCoolTime) const;

	UFUNCTION()
	void OnHealCountChanged(ESkillSlot InSkillSlot, int32 CurrentCount, int32 MaxCount);

	UFUNCTION()
	void OnSkillActivated(ESkillSlot InSkillSlot);

	UFUNCTION()
	void OnSkillCooldownBlocked(ESkillSlot InSkillSlot);

	// 스킬 쿨다운 완료(준비 완료) 핸들러
	UFUNCTION()
	void OnSkillCooldownReady(ESkillSlot InSkillSlot);

	UFUNCTION(BlueprintImplementableEvent)
	void PlayHeartbeatAnimation();

	UFUNCTION(BlueprintImplementableEvent)
	void PlayGlowEffect();

	UFUNCTION(BlueprintImplementableEvent)
	void PlayCooldownBlockedAnimation();

	UFUNCTION(BlueprintImplementableEvent)
	void PlayRedFlashEffect();

	// 스킬 준비 완료 효과 (쿨다운 끝)
	UFUNCTION(BlueprintImplementableEvent)
	void PlaySkillReadyAnimation();

	UFUNCTION(BlueprintImplementableEvent)
	void PlaySkillReadyGlow();

	// 궁극기 준비 완료 시 화면 테두리 플래시
	UFUNCTION(BlueprintImplementableEvent, Category = "GS|Skill")
	void PlayUltimateReadyScreenFlash();

	UPROPERTY(EditAnywhere, Category = "GS|Skill|Visuals")
	FLinearColor UltimateReadyTint = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f); // 황금색 기본값

	UPROPERTY(EditAnywhere, Category = "GS|Skill|Visuals")
	float UltimateReadyScale = 1.3f; // 팝업 스케일 기본값

	/** Sound Assets */
	UFUNCTION(BlueprintCallable)
	void PlaySkillActivationSound();

	UFUNCTION(BlueprintCallable)
	void PlaySkillCooldownSound();

	// 궁극기 전용 사운드 재생 함수들 추가
	UFUNCTION(BlueprintCallable)
	void PlayUltimateSkillActivationSound();

	UFUNCTION(BlueprintCallable)
	void PlayUltimateSkillCooldownSound();

	// 스킬 준비 완료 사운드 (쿨다운 끝)
	UFUNCTION(BlueprintCallable)
	void PlaySkillReadySound();

	UFUNCTION(BlueprintCallable)
	void PlayUltimateSkillReadySound();

	// 궁극기 슬롯 확인 헬퍼 함수
	UFUNCTION(BlueprintCallable)
	bool IsUltimateSkillSlot() const;

	FORCEINLINE AGS_Player* GetOwningActor() const { return OwningCharacter; }
	FORCEINLINE ESkillSlot GetSkillSlot() const { return SkillSlot; }

	void SetOwningActor(AGS_Player* InOwningCharacter)
	{
		OwningCharacter = InOwningCharacter;
	}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CurrentCoolTimeText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> CoolTimeBar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> HealCountText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> SkillImage;

	// 오디오 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> SkillActivationSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> SkillCooldownSound;

	// 궁극기 전용 오디오 설정 추가
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> UltimateSkillActivationSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> UltimateSkillCooldownSound;

	// 스킬 준비 완료 사운드
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> SkillReadySound;

	// 궁극기 준비 완료 사운드
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> UltimateSkillReadySound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	bool bEnableAudio = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AudioVolume = 0.5f;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget, AllowPrivateAccess))
	ESkillSlot SkillSlot;

	UPROPERTY()
	TObjectPtr<AGS_Player> OwningCharacter;
};
