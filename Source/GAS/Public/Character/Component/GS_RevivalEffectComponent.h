// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GS_RevivalEffectComponent.generated.h"

class UPostProcessComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 부활(구조 완료) 시 화면 효과 컴포넌트
 * 
 * - 로컬 플레이어에게만 적용 (멀티플레이 호환)
 * - CompleteRevive() 호출 시 발동
 * - 흰색/금색 플래시에서 점진적 페이드 아웃
 * - "다시 시작" 느낌의 긍정적 피드백 제공
 */
UCLASS(ClassGroup = (Effects), meta = (BlueprintSpawnableComponent))
class GAS_API UGS_RevivalEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_RevivalEffectComponent();

	// ===== 화면 효과 설정 =====

	/** 초기 플래시 밝기 강도 */
	UPROPERTY(EditDefaultsOnly, Category = "RevivalEffect|Visual", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float FlashIntensity = 1.2f;

	/** 페이드 아웃 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "RevivalEffect|Visual", meta = (ClampMin = "0.3", ClampMax = "3.0"))
	float FadeOutDuration = 1.0f;

	/** 부활 빛 색상 (흰색/금색 계열 권장) */
	UPROPERTY(EditDefaultsOnly, Category = "RevivalEffect|Visual")
	FLinearColor RevivalGlowColor = FLinearColor(1.0f, 0.95f, 0.8f, 1.0f);

	// ===== 머티리얼 파라미터 =====

	UPROPERTY(EditDefaultsOnly, Category = "RevivalEffect|Material")
	FName StrengthParamName = TEXT("Strength");

	UPROPERTY(EditDefaultsOnly, Category = "RevivalEffect|Material")
	FName ColorParamName = TEXT("EffectColor");

	// ===== 공개 함수 =====

	/** 초기화 (소유자 및 PostProcess 컴포넌트 설정) */
	void InitializeForOwner(AActor* InOwner, UPostProcessComponent* InPostProcessComp,
	                        UMaterialInterface* InMaterialOverride = nullptr);

	/** 부활 효과 재생 (로컬 플레이어 체크는 호출부에서 수행) */
	UFUNCTION(BlueprintCallable, Category = "RevivalEffect")
	void PlayRevivalEffect();

	/** 효과 즉시 중단 */
	UFUNCTION(BlueprintCallable, Category = "RevivalEffect")
	void StopEffect();

	/** 현재 재생 중인지 확인 */
	UFUNCTION(BlueprintPure, Category = "RevivalEffect")
	bool IsPlaying() const { return bIsPlaying; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 동적 머티리얼 인스턴스 생성 보장 */
	void EnsureMID();

	/** 타이머 시작/정지 */
	void StartTimer();
	void StopTimer();

	/** 틱 업데이트 (페이드 아웃 처리) */
	void TickUpdate();

	/** 효과 강도 및 색상 적용 */
	void ApplyEffect(float Strength, const FLinearColor& Color);

private:
	TWeakObjectPtr<AActor> OwnerActor;

	UPROPERTY()
	TWeakObjectPtr<UPostProcessComponent> ManagedPostProcessComp;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> EffectMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	FTimerHandle UpdateTimerHandle;

	bool bIsPlaying = false;
	float CurrentStrength = 0.0f;
	float ElapsedTime = 0.0f;
};
