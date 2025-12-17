// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GS_InteractableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UGS_InteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 상호작용 가능한 액터들이 구현해야 하는 인터페이스
 * 예: 불씨 보물상자, 문, 오브젝트 등
 */
class GAS_API IGS_InteractableInterface
{
	GENERATED_BODY()

public:
	/**
	 * 상호작용 가능 여부 확인
	 * @param Interactor 상호작용을 시도하는 액터 (시커)
	 * @return 상호작용 가능하면 true
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* Interactor) const;

	/**
	 * 상호작용에 필요한 시간 반환 (초)
	 * @return 상호작용 소요 시간. 0이면 즉시 완료.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	float GetInteractionDuration() const;

	/**
	 * 상호작용 시작 시 호출
	 * @param Interactor 상호작용을 시작한 액터
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void BeginInteract(AActor* Interactor);

	/**
	 * 상호작용 종료 시 호출
	 * @param Interactor 상호작용을 수행한 액터
	 * @param bCompleted true면 성공적으로 완료, false면 취소됨
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void EndInteract(AActor* Interactor, bool bCompleted);

	/**
	 * 상호작용 UI에 표시할 텍스트 반환
	 * @return 상호작용 안내 텍스트 (예: "E 획득")
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionText() const;

	/**
	 * 상호작용 우선순위 반환 (높을수록 우선)
	 * @return 우선순위 값
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	int32 GetInteractionPriority() const;
};
