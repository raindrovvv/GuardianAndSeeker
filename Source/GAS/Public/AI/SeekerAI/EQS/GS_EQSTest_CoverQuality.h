#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "GS_EQSTest_CoverQuality.generated.h"

/**
 * EQS Test that evaluates how good a position is for taking cover
 * Checks line of sight blocking, distance from threat, and accessibility
 */
UCLASS()
class GAS_API UGS_EQSTest_CoverQuality : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UGS_EQSTest_CoverQuality();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Minimum height of cover to be considered valid */
	UPROPERTY(EditDefaultsOnly, Category = "Cover")
	float MinCoverHeight = 100.0f;

	/** How far to trace for cover objects */
	UPROPERTY(EditDefaultsOnly, Category = "Cover")
	float CoverTraceDistance = 150.0f;

	/** Preferred distance from threat for cover */
	UPROPERTY(EditDefaultsOnly, Category = "Cover")
	float PreferredCoverDistance = 500.0f;

	/** Collision channel for cover detection */
	UPROPERTY(EditDefaultsOnly, Category = "Cover")
	TEnumAsByte<ECollisionChannel> CoverTraceChannel = ECC_WorldStatic;

private:
	/** Check if position has cover from a specific threat direction */
	float EvaluateCoverFromDirection(const FVector& TestLocation, const FVector& ThreatLocation, UWorld* World) const;
};
