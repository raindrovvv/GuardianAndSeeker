#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "GS_EQSTest_VisibilityToTarget.generated.h"

/**
 * EQS Test that evaluates visibility between test location and target
 * Can be used to find positions with or without line of sight to enemies
 */
UCLASS()
class GAS_API UGS_EQSTest_VisibilityToTarget : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UGS_EQSTest_VisibilityToTarget();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Context for the target to check visibility against */
	UPROPERTY(EditDefaultsOnly, Category = "Visibility")
	TSubclassOf<UEnvQueryContext> TargetContext;

	/** Height offset for visibility traces (eye level) */
	UPROPERTY(EditDefaultsOnly, Category = "Visibility")
	float EyeHeightOffset = 150.0f;

	/** If true, prefer positions WITH visibility. If false, prefer hidden positions. */
	UPROPERTY(EditDefaultsOnly, Category = "Visibility")
	bool bPreferVisible = false;
};
