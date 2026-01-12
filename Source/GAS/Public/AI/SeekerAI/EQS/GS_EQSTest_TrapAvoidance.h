#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "GS_EQSTest_TrapAvoidance.generated.h"

class AGS_TrapBase;

/**
 * EQS Test that scores positions based on distance from detected traps
 * Higher scores for positions farther from traps
 */
UCLASS()
class GAS_API UGS_EQSTest_TrapAvoidance : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UGS_EQSTest_TrapAvoidance();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Radius to search for traps around each test point */
	UPROPERTY(EditDefaultsOnly, Category = "Trap Avoidance")
	float TrapDetectionRadius = 500.0f;

	/** Minimum safe distance from traps for max score */
	UPROPERTY(EditDefaultsOnly, Category = "Trap Avoidance")
	float MinSafeDistance = 300.0f;

	/** Actor class to detect as traps */
	UPROPERTY(EditDefaultsOnly, Category = "Trap Avoidance")
	TSubclassOf<AActor> TrapClass;
};
