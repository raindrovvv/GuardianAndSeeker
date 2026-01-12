#pragma once

#include "CoreMinimal.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ProjectedPoints.h"
#include "GS_EQSGenerator_FlankingPositions.generated.h"

/**
 * EQS Generator that creates flanking positions around a target
 * Generates points at various angles around the target for tactical positioning
 */
UCLASS()
class GAS_API UGS_EQSGenerator_FlankingPositions : public UEnvQueryGenerator_ProjectedPoints
{
	GENERATED_BODY()

public:
	UGS_EQSGenerator_FlankingPositions();

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Distance from target for flanking positions */
	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	FAIDataProviderFloatValue FlankingRadius;

	/** Number of positions to generate in a ring around target */
	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	FAIDataProviderIntValue NumberOfPositions;

	/** Minimum angle from querier's current approach direction (to ensure actual flanking) */
	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	float MinFlankAngle = 45.0f;

	/** Maximum angle from querier's current approach direction */
	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	float MaxFlankAngle = 135.0f;

	/** Context for the target to flank around */
	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	TSubclassOf<UEnvQueryContext> FlankAroundContext;
};
