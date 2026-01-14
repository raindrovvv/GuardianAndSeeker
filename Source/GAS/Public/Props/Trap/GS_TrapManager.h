#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GS_TrapManager.generated.h"


class AGS_TrapBase;
UCLASS()
class GAS_API AGS_TrapManager : public AActor
{
	GENERATED_BODY()

public:
	AGS_TrapManager();

	virtual void BeginPlay() override;

	void RegisterTrap(AGS_TrapBase* Trap);
	void UnregisterTrap(AGS_TrapBase* Trap);

protected:
	// 서버 전용: TWeakObjectPtr은 UE5 복제 시스템에서 지원되지 않으며, 클라이언트에서 사용되지 않음
	UPROPERTY()
	TArray<TWeakObjectPtr<AGS_TrapBase>> RegisteredTraps;

	FTimerHandle MotionLoopTimerHandle;

	UPROPERTY(EditAnywhere)
	float MotionInterval = 0.1f;

	//void DoAllTrapMotion();
	//void DoClientLerpUpdate();
	void TimerTrapManager();


	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
