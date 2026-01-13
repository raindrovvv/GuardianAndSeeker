#include "AI/SeekerAI/EQS/GS_EQSTest_CoverQuality.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "DrawDebugHelpers.h"

UGS_EQSTest_CoverQuality::UGS_EQSTest_CoverQuality()
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);
}

void UGS_EQSTest_CoverQuality::RunTest(FEnvQueryInstance& QueryInstance) const
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

	// Get threat/target location from context
	TArray<FVector> ContextLocations;
	if (!QueryInstance.PrepareContext(UEnvQueryContext_Querier::StaticClass(), ContextLocations))
	{
		return;
	}

	// We need a threat location to evaluate cover FROM
	// Try to get it from the Target context if available
	TArray<AActor*> TargetActors;
	QueryInstance.PrepareContext(UEnvQueryContext_Querier::StaticClass(), TargetActors);

	FVector ThreatLocation = FVector::ZeroVector;

	// Get querier location as fallback threat direction
	if (ContextLocations.Num() > 0)
	{
		ThreatLocation = ContextLocations[0];
	}

	// Iterate through all test items (potential cover positions)
	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector TestLocation = GetItemLocation(QueryInstance, It.GetIndex());

		float CoverScore = EvaluateCoverFromDirection(TestLocation, ThreatLocation, World);

		It.SetScore(TestPurpose, FilterType, CoverScore, 0.0f, 1.0f);
	}
}

float UGS_EQSTest_CoverQuality::EvaluateCoverFromDirection(const FVector& TestLocation, const FVector& ThreatLocation, UWorld* World) const
{
	if (ThreatLocation.IsNearlyZero())
	{
		return 0.5f; // Neutral score if no threat
	}

	float TotalScore = 0.0f;
	int32 CheckCount = 0;

	// Direction from test location toward threat
	FVector DirectionToThreat = (ThreatLocation - TestLocation).GetSafeNormal();

	// 1. Check for cover object between test location and threat
	FVector CoverCheckStart = TestLocation + FVector(0, 0, 50.0f); // Slightly above ground
	FVector CoverCheckEnd = CoverCheckStart + DirectionToThreat * CoverTraceDistance;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;

	bool bHasCover = World->LineTraceSingleByChannel(
	    HitResult,
	    CoverCheckStart,
	    CoverCheckEnd,
	    CoverTraceChannel,
	    QueryParams);

	if (bHasCover)
	{
		// Cover exists - score based on how much it blocks
		float BlockPercentage = FMath::Clamp(HitResult.Distance / CoverTraceDistance, 0.0f, 1.0f);
		TotalScore += (1.0f - BlockPercentage) * 0.4f; // Closer cover is better
		CheckCount++;

		// Check cover height
		FVector CoverTop = HitResult.ImpactPoint + FVector(0, 0, MinCoverHeight);
		FHitResult HeightCheck;
		bool bCoverTall = World->LineTraceSingleByChannel(
		    HeightCheck,
		    CoverTop,
		    CoverTop + DirectionToThreat * 10.0f,
		    CoverTraceChannel,
		    QueryParams);

		if (bCoverTall)
		{
			TotalScore += 0.3f; // Bonus for tall cover
			CheckCount++;
		}
	}

	// 2. Distance from threat scoring
	float DistanceToThreat = FVector::Dist(TestLocation, ThreatLocation);
	float DistanceScore = 1.0f - FMath::Abs(DistanceToThreat - PreferredCoverDistance) / PreferredCoverDistance;
	DistanceScore = FMath::Clamp(DistanceScore, 0.0f, 1.0f);
	TotalScore += DistanceScore * 0.3f;
	CheckCount++;

	// 3. Check if position is accessible (not inside geometry)
	FVector NavCheckStart = TestLocation + FVector(0, 0, 200.0f);
	FVector NavCheckEnd = TestLocation - FVector(0, 0, 50.0f);
	FHitResult GroundCheck;
	bool bOnGround = World->LineTraceSingleByChannel(
	    GroundCheck,
	    NavCheckStart,
	    NavCheckEnd,
	    ECC_WorldStatic,
	    QueryParams);

	if (bOnGround)
	{
		TotalScore += 0.2f; // Valid ground position
	}

	return CheckCount > 0 ? TotalScore : 0.0f;
}

FText UGS_EQSTest_CoverQuality::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Cover Quality"));
}

FText UGS_EQSTest_CoverQuality::GetDescriptionDetails() const
{
	return FText::Format(
	    NSLOCTEXT("EQS", "CoverQualityDetails", "Min Height: {0}, Trace Dist: {1}, Preferred Dist: {2}"),
	    FText::AsNumber(MinCoverHeight),
	    FText::AsNumber(CoverTraceDistance),
	    FText::AsNumber(PreferredCoverDistance));
}
