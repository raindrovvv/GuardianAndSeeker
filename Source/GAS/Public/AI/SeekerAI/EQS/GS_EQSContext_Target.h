#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "GS_EQSContext_Target.generated.h"

/**
 * EQS Context that provides the current target enemy location
 * Used for queries like finding cover FROM the enemy, flanking positions, etc.
 */
UCLASS()
class GAS_API UGS_EQSContext_Target : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
