#include "AI/SeekerAI/EQS/GS_EQSTest_TrapAvoidance.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "Kismet/GameplayStatics.h"
#include "Props/Trap/GS_TrapBase.h"

UGS_EQSTest_TrapAvoidance::UGS_EQSTest_TrapAvoidance()
{
	Cost = EEnvTestCost::Medium;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);

	TrapDetectionRadius = 500.0f;
	MinSafeDistance = 300.0f;
	TrapClass = AGS_TrapBase::StaticClass();
}

void UGS_EQSTest_TrapAvoidance::RunTest(FEnvQueryInstance& QueryInstance) const
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

	// Find all traps in the world (cached for performance in real implementation)
	TArray<AActor*> AllTraps;
	if (TrapClass)
	{
		UGameplayStatics::GetAllActorsOfClass(World, TrapClass, AllTraps);
	}

	// If no traps found, all positions are equally safe
	if (AllTraps.Num() == 0)
	{
		for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
		{
			It.SetScore(TestPurpose, FilterType, 1.0f, 0.0f, 1.0f);
		}
		return;
	}

	// Iterate through all test items
	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector TestLocation = GetItemLocation(QueryInstance, It.GetIndex());

		// Find the closest trap
		float ClosestTrapDistance = TrapDetectionRadius;

		for (AActor* Trap : AllTraps)
		{
			if (!Trap)
			{
				continue;
			}

			float DistanceToTrap = FVector::Dist(TestLocation, Trap->GetActorLocation());

			if (DistanceToTrap < ClosestTrapDistance)
			{
				ClosestTrapDistance = DistanceToTrap;
			}
		}

		// Calculate score based on distance from nearest trap
		float Score;
		if (ClosestTrapDistance >= MinSafeDistance)
		{
			// Far enough from traps - perfect score
			Score = 1.0f;
		}
		else if (ClosestTrapDistance <= 0.0f)
		{
			// On top of a trap - worst score
			Score = 0.0f;
		}
		else
		{
			// Linear interpolation based on distance
			Score = ClosestTrapDistance / MinSafeDistance;
		}

		It.SetScore(TestPurpose, FilterType, Score, 0.0f, 1.0f);
	}
}

FText UGS_EQSTest_TrapAvoidance::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Trap Avoidance Distance"));
}

FText UGS_EQSTest_TrapAvoidance::GetDescriptionDetails() const
{
	return FText::Format(
	    NSLOCTEXT("EQS", "TrapAvoidanceDetails", "Detection Radius: {0}, Safe Distance: {1}"),
	    FText::AsNumber(TrapDetectionRadius),
	    FText::AsNumber(MinSafeDistance));
}
