#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GS_EnvironmentProp.generated.h"

/**
 * 나나이트 및 거리 컬링 최적화 기능이 내장된 환경 프랍용 베이스 클래스입니다.
 * BP_DynamicWall 등 일반적인 환경 오브젝트의 부모 클래스로 사용하여 성능을 최적화할 수 있습니다.
 */
UCLASS()
class GAS_API AGS_EnvironmentProp : public AActor
{
	GENERATED_BODY()

public:
	AGS_EnvironmentProp();

	/** 액터가 컬링될 기본 거리를 설정합니다. (단위: cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization")
	float BaseCullDistance = 5000.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Significance Manager 등록 */
	void RegisterSignificanceManager();

	/** 중요도 계산 (거리 기반) */
	virtual float CalculateSignificance(const FTransform& Viewpoint);

	/** 중요도 변경 시 호출 */
	virtual void OnSignificanceChanged(float NewSignificance);

	/** 현재 중요도 단계 */
	float CurrentSignificance = 1.0f;

	/** 그림자 및 거리 컬링 업데이트 타이머 */
	FTimerHandle ShadowCullingTimerHandle;

	/** 컬링 대상이 되는 컴포넌트 캐싱 (매번 GetComponents 호출 방지) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> OptimizedComponents;

	/** 초기 컬링 설정 적용 */
	void ApplyDistanceCulling();

	/** 주기적으로 가시성 및 그림자를 업데이트 */
	void UpdateCulling();
};
