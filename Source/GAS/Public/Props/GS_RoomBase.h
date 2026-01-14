#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ChildActorComponent.h"
#include "GS_RoomBase.generated.h"

class ULightComponent;
class UPrimitiveComponent;

UCLASS()
class GAS_API AGS_RoomBase : public AActor
{
	GENERATED_BODY()

public:
	AGS_RoomBase();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Floor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Ceiling;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Wall;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UChildActorComponent> BGMTrigger;

	void HideCeiling();
	void ShowCeiling();

	void UseDepthStencil();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Significance Manager 등록 */
	void RegisterSignificanceManager();

	/** 중요도 계산 (거리 기반) */
	virtual float CalculateSignificance(const FTransform& Viewpoint);

	/** 중요도 변경 시 호출 */
	virtual void OnSignificanceChanged(float NewSignificance);

	/** 현재 중요도 단계 */
	float CurrentSignificance = 1.0f;

	/** 그림자 컬링 타이머 */
	FTimerHandle ShadowCullingTimerHandle;

	// ========================================
	// 캐싱된 상수값
	// ========================================

	/** 캐싱된 그림자 비활성화 거리 제곱값 */
	float CachedShadowDisableDistanceSq = 0.0f;

	/** 보스룸 여부 (한 번만 판단) */
	bool bIsBossRoom = false;

	// ========================================
	// 그림자 상태 캐싱 (중복 토글 방지)
	// ========================================

	/** 마지막 그림자 활성화 상태 (-1: 미초기화, 0: 비활성, 1: 활성) */
	int8 LastShadowState = -1;

	/** 마지막 동적 그림자 활성화 상태 */
	int8 LastDynamicShadowState = -1;

	// ========================================
	// 최적화 대상 컴포넌트 캐싱
	// ========================================

	/** 가시성 토글 대상 Primitive 컴포넌트 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> CachedPrimitiveComponents;

	/** 가시성 토글 대상 Light 컴포넌트 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULightComponent>> CachedLightComponents;

	/** 가시성 토글 대상 Niagara 컴포넌트 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UNiagaraComponent>> CachedNiagaraComponents;

	/** 컴포넌트 캐싱 초기화 */
	void CacheOptimizedComponents();

	/** 가시성 및 그림자 컬링 업데이트 */
	void UpdateVisibilityAndShadowCulling();
};
