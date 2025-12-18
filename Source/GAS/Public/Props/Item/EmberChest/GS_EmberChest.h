// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Props/Item/EmberChest/EEmberRewardType.h"
#include "Props/Item/EmberChest/GS_EmberChestDataAsset.h"
#include "Interface/GS_InteractableInterface.h"
#include "GS_EmberChest.generated.h"

class USphereComponent;
class UNiagaraComponent;
class AGS_Seeker;


/**
 * 불씨 보물상자 (Ember Chest)
 * 프로메테우스가 던전의 방어벽이 약해진 지점에 불꽃 전송으로 보내는 보급품
 * 시커만 획득 가능하며, 버프/포션/장비강화 등의 보상 제공
 */
UCLASS()
class GAS_API AGS_EmberChest : public AActor, public IGS_InteractableInterface
{
	GENERATED_BODY()

public:
	AGS_EmberChest();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	virtual void Tick(float DeltaTime) override;

	// ========================
	// 컴포넌트
	// ========================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* ChestVFX;

	// ========================
	// 설정
	// ========================

	/** 데이터 에셋 참조 (블루프린트에서 설정) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ember Chest")
	UGS_EmberChestDataAsset* ChestDataAsset;

	// ========================
	// 상태
	// ========================

	/** 현재 상자 상태 */
	UPROPERTY(ReplicatedUsing = OnRep_ChestState, BlueprintReadOnly, Category = "Ember Chest")
	EEmberChestState CurrentState;

	/** 할당된 보상 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Ember Chest")
	EEmberRewardType AssignedRewardType;

	// ========================
	// 사운드
	// ========================

	/** 상자 획득 시 재생할 Wwise 사운드 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ember Chest|Sound")
	class UAkAudioEvent* CollectedSound;

	// ========================
	// 상호작용 설정
	// ========================

	/** 상호작용 소요 시간 (초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ember Chest|Interaction")
	float InteractionDuration = 2.0f;

	/** 상호작용 UI 텍스트 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ember Chest|Interaction")
	FText InteractionText = NSLOCTEXT("EmberChest", "Interact", "획득");

	// ========================
	// IInteractable 인터페이스 구현
	// ========================

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual float GetInteractionDuration_Implementation() const override;
	virtual void BeginInteract_Implementation(AActor* Interactor) override;
	virtual void EndInteract_Implementation(AActor* Interactor, bool bCompleted) override;
	virtual FText GetInteractionText_Implementation() const override;
	virtual int32 GetInteractionPriority_Implementation() const override;

	// ========================
	// 공개 함수
	// ========================

	/** 보상 설정 (스포너에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "Ember Chest")
	void SetReward(const FEmberRewardConfig& InRewardConfig);

	/** 수명 타이머 시작 (스포너에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "Ember Chest")
	void StartLifetimeTimer(float Lifetime);

protected:
	// ========================
	// 오버랩 이벤트 (상호작용 범위 감지용)
	// ========================

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// ========================
	// 상태 전환
	// ========================

	/** 상태 변경 (서버에서만 호출) */
	void SetChestState(EEmberChestState NewState);

	UFUNCTION()
	void OnRep_ChestState();

	/** 실체화 완료 시 호출 */
	void OnMaterializingComplete();

	// ========================
	// 보상 지급
	// ========================

	/** 시커에게 보상 지급 */
	UFUNCTION(Server, Reliable)
	void ServerGrantReward(AGS_Seeker* Seeker);

	/** 버프 적용 */
	void ApplyBuff(AGS_Seeker* Seeker);

	/** 체력 회복 적용 */
	void ApplyHealthRestore(AGS_Seeker* Seeker);

	// ========================
	// VFX
	// ========================

	/** 현재 상태에 맞는 VFX 재생 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayStateVFX(EEmberChestState State);

	/** 획득 VFX 재생 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayCollectedVFX();

	// ========================
	// 타이머
	// ========================

	FTimerHandle MaterializingTimerHandle;
	FTimerHandle LifetimeTimerHandle;

	/** 수명 종료 시 호출 */
	void OnLifetimeExpired();

private:
	/** 캐싱된 보상 설정 */
	FEmberRewardConfig CachedRewardConfig;

	/** 버프 제거 타이머 핸들 맵 (Seeker별) */
	TMap<AGS_Seeker*, FTimerHandle> BuffRemovalTimers;

	/** 현재 상호작용 중인 시커 (WeakPtr로 안전하게 참조) */
	TWeakObjectPtr<AGS_Seeker> CurrentInteractor;
};
