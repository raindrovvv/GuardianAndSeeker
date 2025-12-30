#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GS_VisualPoolComp.generated.h"

class AGS_ArrowVisualActor;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GAS_API UGS_VisualPoolComp : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_VisualPoolComp();

	UFUNCTION(BlueprintCallable, Category = "Optimization")
	void Initialize(TSubclassOf<AGS_ArrowVisualActor> InActorClass, int32 PoolSize);

	UFUNCTION(BlueprintCallable, Category = "Optimization")
	AGS_ArrowVisualActor* GetActorFromPool(const FVector& Location, const FRotator& Rotation);

	UFUNCTION(BlueprintCallable, Category = "Optimization")
	void ReturnToPool(AGS_ArrowVisualActor* Actor);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TArray<AGS_ArrowVisualActor*> ActorPool;

	UPROPERTY()
	TSubclassOf<AGS_ArrowVisualActor> ActorClass;
};
