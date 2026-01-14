#include "AI/SeekerAI/EQS/GS_EQSTest_VisibilityToTarget.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "AI/SeekerAI/EQS/GS_EQSContext_Target.h"
#include "CollisionQueryParams.h"

UGS_EQSTest_VisibilityToTarget::UGS_EQSTest_VisibilityToTarget()
{
	Cost = EEnvTestCost::Medium;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);

	TargetContext = UGS_EQSContext_Target::StaticClass();
	EyeHeightOffset = 150.0f;
	bPreferVisible = false; // Default: prefer hidden positions for cover
}

void UGS_EQSTest_VisibilityToTarget::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (!QueryOwner)
	{
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(QueryOwner, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return;
	}

	// Get target locations for visibility check
	TArray<FVector> TargetLocations;
	if (!QueryInstance.PrepareContext(TargetContext, TargetLocations) || TargetLocations.Num() == 0)
	{
		return;
	}

	const FVector TargetLocation = TargetLocations[0] + FVector(0, 0, EyeHeightOffset);

	// Setup collision query
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.bReturnPhysicalMaterial = false;

	// Ignore the querier
	AActor* QuerierActor = Cast<AActor>(QueryOwner);
	if (QuerierActor)
	{
		QueryParams.AddIgnoredActor(QuerierActor);
	}

	// Iterate through all test items
	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector TestLocation = GetItemLocation(QueryInstance, It.GetIndex()) + FVector(0, 0, EyeHeightOffset);

		// Perform visibility trace
		FHitResult HitResult;
		bool bHit = World->LineTraceSingleByChannel(
		    HitResult,
		    TestLocation,
		    TargetLocation,
		    ECC_Visibility,
		    QueryParams);

		// Score based on visibility
		float Score;
		if (bHit)
		{
			// Something is blocking the view
			Score = bPreferVisible ? 0.0f : 1.0f;
		}
		else
		{
			// Clear line of sight
			Score = bPreferVisible ? 1.0f : 0.0f;
		}

		It.SetScore(TestPurpose, FilterType, Score, 0.0f, 1.0f);
	}
}

FText UGS_EQSTest_VisibilityToTarget::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Visibility To Target"));
}

FText UGS_EQSTest_VisibilityToTarget::GetDescriptionDetails() const
{
	return FText::Format(
	    NSLOCTEXT("EQS", "VisibilityDetails", "Eye Height: {0}, Prefer Visible: {1}"),
	    FText::AsNumber(EyeHeightOffset),
	    bPreferVisible ? FText::FromString(TEXT("Yes")) : FText::FromString(TEXT("No")));
}
