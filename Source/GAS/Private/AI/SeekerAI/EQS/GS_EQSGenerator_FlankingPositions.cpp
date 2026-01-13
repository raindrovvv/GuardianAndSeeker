#include "AI/SeekerAI/EQS/GS_EQSGenerator_FlankingPositions.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "AI/SeekerAI/EQS/GS_EQSContext_Target.h"

UGS_EQSGenerator_FlankingPositions::UGS_EQSGenerator_FlankingPositions()
{
	FlankingRadius.DefaultValue = 400.0f;
	NumberOfPositions.DefaultValue = 8;
	MinFlankAngle = 45.0f;
	MaxFlankAngle = 135.0f;
	FlankAroundContext = UGS_EQSContext_Target::StaticClass();

	// Use point items
	ItemType = UEnvQueryItemType_Point::StaticClass();
}

void UGS_EQSGenerator_FlankingPositions::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	// Get querier location
	TArray<FVector> QuerierLocations;
	if (!QueryInstance.PrepareContext(UEnvQueryContext_Querier::StaticClass(), QuerierLocations) || QuerierLocations.Num() == 0)
	{
		return;
	}
	const FVector QuerierLocation = QuerierLocations[0];

	// Get target location (what we're flanking around)
	TArray<FVector> TargetLocations;
	if (!QueryInstance.PrepareContext(FlankAroundContext, TargetLocations) || TargetLocations.Num() == 0)
	{
		return;
	}
	const FVector TargetLocation = TargetLocations[0];

	// Get parameter values
	float Radius = FlankingRadius.GetValue();
	int32 NumPositions = NumberOfPositions.GetValue();

	// Calculate the current approach direction (from querier to target)
	FVector ApproachDirection = (TargetLocation - QuerierLocation).GetSafeNormal2D();
	float BaseAngle = FMath::Atan2(ApproachDirection.Y, ApproachDirection.X);

	// Generate points at various angles around the target
	TArray<FNavLocation> GeneratedPoints;

	for (int32 i = 0; i < NumPositions; ++i)
	{
		// Calculate angle for this position
		// Distribute positions between MinFlankAngle and MaxFlankAngle on both sides
		float AngleRatio = static_cast<float>(i) / static_cast<float>(NumPositions - 1);

		// Alternate between left and right flanks
		float FlankAngle;
		if (i % 2 == 0)
		{
			// Left flank
			FlankAngle = FMath::Lerp(MinFlankAngle, MaxFlankAngle, AngleRatio);
		}
		else
		{
			// Right flank
			FlankAngle = -FMath::Lerp(MinFlankAngle, MaxFlankAngle, AngleRatio);
		}

		float FinalAngle = BaseAngle + FMath::DegreesToRadians(FlankAngle);

		// Calculate position
		FVector FlankPosition;
		FlankPosition.X = TargetLocation.X + FMath::Cos(FinalAngle) * Radius;
		FlankPosition.Y = TargetLocation.Y + FMath::Sin(FinalAngle) * Radius;
		FlankPosition.Z = TargetLocation.Z;

		FNavLocation NavLoc(FlankPosition);
		GeneratedPoints.Add(NavLoc);
	}

	// Add additional positions at varying distances for more options
	for (int32 i = 0; i < NumPositions / 2; ++i)
	{
		float AngleRatio = static_cast<float>(i) / static_cast<float>(NumPositions / 2 - 1);
		float FlankAngle = FMath::Lerp(MinFlankAngle, MaxFlankAngle, AngleRatio);

		// Closer flanking positions
		float CloseRadius = Radius * 0.6f;
		float FinalAngle = BaseAngle + FMath::DegreesToRadians(FlankAngle);

		FVector CloseFlankPos;
		CloseFlankPos.X = TargetLocation.X + FMath::Cos(FinalAngle) * CloseRadius;
		CloseFlankPos.Y = TargetLocation.Y + FMath::Sin(FinalAngle) * CloseRadius;
		CloseFlankPos.Z = TargetLocation.Z;

		FNavLocation NavLoc(CloseFlankPos);
		GeneratedPoints.Add(NavLoc);

		// Far flanking positions
		float FarRadius = Radius * 1.4f;
		FVector FarFlankPos;
		FarFlankPos.X = TargetLocation.X + FMath::Cos(FinalAngle) * FarRadius;
		FarFlankPos.Y = TargetLocation.Y + FMath::Sin(FinalAngle) * FarRadius;
		FarFlankPos.Z = TargetLocation.Z;

		FNavLocation FarNavLoc(FarFlankPos);
		GeneratedPoints.Add(FarNavLoc);
	}

	// Project points to navigation mesh
	ProjectAndFilterNavPoints(GeneratedPoints, QueryInstance);

	// Store the generated points
	QueryInstance.AddItemData<UEnvQueryItemType_Point>(GeneratedPoints);
}

FText UGS_EQSGenerator_FlankingPositions::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Flanking Positions"));
}

FText UGS_EQSGenerator_FlankingPositions::GetDescriptionDetails() const
{
	return FText::FromString(FString::Printf(
	    TEXT("Radius: %.0f, Positions: %d, Angle: %.0f-%.0f"),
	    FlankingRadius.DefaultValue,
	    NumberOfPositions.DefaultValue,
	    MinFlankAngle,
	    MaxFlankAngle));
}
