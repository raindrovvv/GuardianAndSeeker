// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GS_PositiveEffectComponent.generated.h"

class UPostProcessComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 긍정적 효과 타입 정의 (힐, 버프 등)
 */
UENUM(BlueprintType)
enum class EPositiveEffectType : uint8
{
	None,
	Heal UMETA(DisplayName = "Heal"), // 녹색
	AttackBuff UMETA(DisplayName = "Attack Buff"), // 주황
	DefenseBuff UMETA(DisplayName = "Defense Buff"), // 파랑
	SpeedBuff UMETA(DisplayName = "Speed Buff"), // 노랑
};

/**
 * 힐/버프 등 긍정적 효과 수신 시 화면 가장자리 시각 효과 컴포넌트
 * 
 * GS_LowHealthEffectComponent의 PostProcess 패턴을 활용하여
 * 회복/버프 수신 시 색상별 화면 테두리 펄스 효과 제공
 */
UCLASS(ClassGroup = (Effects), meta = (BlueprintSpawnableComponent))
class GAS_API UGS_PositiveEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_PositiveEffectComponent();

	// ===== 설정값 =====

	/** 효과용 PostProcess 머티리얼 */
	UPROPERTY()
	UMaterialInterface* EffectMaterial = nullptr;

	/** PostProcess 우선순위 */
	UPROPERTY(EditDefaultsOnly, Category = "PositiveEffect", meta = (ClampMin = "0"))
	int32 PostProcessPriority = 5;

	/** 효과 보간 속도 */
	UPROPERTY(EditDefaultsOnly, Category = "PositiveEffect", meta = (ClampMin = "0.01"))
	float EffectInterpSpeed = 8.0f;

	/** 업데이트 간격 */
	UPROPERTY(EditDefaultsOnly, Category = "PositiveEffect", meta = (ClampMin = "0.01"))
	float UpdateInterval = 0.05f;

	/** 효과 지속 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "PositiveEffect", meta = (ClampMin = "0.1"))
	float EffectDuration = 0.2f;

	// ===== 색상 설정 =====

	/** 힐 효과 색상 (녹색) */
	UPROPERTY(EditDefaultsOnly, Category = "PositiveEffect|Colors")
	FLinearColor HealColor = FLinearColor(0.2f, 1.0f, 0.3f, 1.0f);

	/** 공격 버프 색상 (주황) */
	UPROPERTY(EditDefaultsOnly, Category = "PositiveEffect|Colors")
	FLinearColor AttackBuffColor = FLinearColor(1.0f, 0.6f, 0.1f, 1.0f);

	/** 방어 버프 색상 (파랑) */
	UPROPERTY(EditDefaultsOnly, Category = "PositiveEffect|Colors")
	FLinearColor DefenseBuffColor = FLinearColor(0.2f, 0.5f, 1.0f, 1.0f);

	/** 속도 버프 색상 (노랑) */
	UPROPERTY(EditDefaultsOnly, Category = "PositiveEffect|Colors")
	FLinearColor SpeedBuffColor = FLinearColor(1.0f, 1.0f, 0.2f, 1.0f);

	// ===== 머티리얼 파라미터 이름 =====

	UPROPERTY(EditDefaultsOnly, Category = "PositiveEffect|Material")
	FName StrengthParamName = TEXT("Strength");

	UPROPERTY(EditDefaultsOnly, Category = "PositiveEffect|Material")
	FName ColorParamName = TEXT("EffectColor");

	// ===== 공개 함수 =====

	/** 초기화 (소유자 및 PostProcess 컴포넌트 설정) */
	void InitializeForOwner(AActor* InOwner, UPostProcessComponent* InPostProcessComp,
	                        UMaterialInterface* InMaterialOverride = nullptr);

	/** 힐 수신 시 호출 */
	UFUNCTION(BlueprintCallable, Category = "PositiveEffect")
	void OnHealed(float HealAmount);

	/** 버프 수신 시 호출 */
	UFUNCTION(BlueprintCallable, Category = "PositiveEffect")
	void OnBuffReceived(EPositiveEffectType BuffType);

	/** 효과 즉시 정지 */
	void StopEffect();

	UPostProcessComponent* GetPostProcessComponent() const { return ManagedPostProcessComp.Get(); }

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

	/** 강도 및 색상 적용 */
	void ApplyEffect(float Strength, const FLinearColor& Color);

	/** 효과 타입에 해당하는 색상 반환 */
	FLinearColor GetColorForType(EPositiveEffectType Type) const;

private:
	TWeakObjectPtr<AActor> OwnerActor;

	UPROPERTY()
	TWeakObjectPtr<UPostProcessComponent> ManagedPostProcessComp;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	FTimerHandle UpdateTimerHandle;

	bool bIsActive = false;
	float CurrentStrength = 0.0f;
	float TargetStrength = 0.0f;
	float RemainingDuration = 0.0f;
	FLinearColor CurrentColor = FLinearColor::Green;
};
