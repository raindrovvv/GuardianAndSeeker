// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/Chan/GS_AN_RotateControllerYaw.h"
#include "Character/GS_TpsController.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"

UGS_AN_RotateControllerYaw::UGS_AN_RotateControllerYaw()
{
}

void UGS_AN_RotateControllerYaw::Notify(USkeletalMeshComponent* MeshComp,
										UAnimSequenceBase* Animation,
										const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	AGS_Seeker* Seeker = Cast<AGS_Seeker>(MeshComp->GetOwner());
	if (!Seeker)
	{
		return;
	}

	AGS_TpsController* TpsController = Cast<AGS_TpsController>(Seeker->GetController());
	if (!TpsController)
	{
		return;
	}

	UWorld* World = MeshComp->GetWorld();
	if (!World)
	{
		return;
	}

	// Calculate view direction from the controller
	FVector ViewLocation;
	FRotator ViewRotation;
	TpsController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();

	// Sweep collision parameters
	const FVector TraceStart = ViewLocation;
	const FVector TraceEnd = ViewLocation + (ViewDirection * MaxAssistRange);

	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_Pawn); // TODO: Change to custom monster channel when available

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BasicAttackAimAssist), false, Seeker);
	QueryParams.AddIgnoredActor(Seeker);

	TArray<FHitResult> HitResults;
	const FCollisionShape SweepSphere = FCollisionShape::MakeSphere(TargetSearchRadius);

	const bool bHasHits = World->SweepMultiByObjectType(
		HitResults, TraceStart, TraceEnd, FQuat::Identity, ObjectTypes, SweepSphere, QueryParams);

	if (bHasHits)
	{
		AActor* BestTarget = nullptr;
		float MinimumTime = TNumericLimits<float>::Max();
		TSet<AActor*> ProcessedActors;

		for (const FHitResult& Hit : HitResults)
		{
			AActor* TargetCandidate = Hit.GetActor();
			if (!TargetCandidate || TargetCandidate == Seeker || ProcessedActors.Contains(TargetCandidate))
			{
				continue;
			}

			// Add logic here to filter enemy vs teammate if necessary
			ProcessedActors.Add(TargetCandidate);

			if (Hit.Time < MinimumTime)
			{
				MinimumTime = Hit.Time;
				BestTarget = TargetCandidate;
			}
		}

		if (BestTarget)
		{
			// Rotate to face the target's center
			const FVector StartPos = Seeker->GetActorLocation();
			const FVector TargetPos = BestTarget->GetActorLocation();
			const FRotator TargetRotation((TargetPos - StartPos).Rotation().Yaw, 0.0f, 0.0f);

			Seeker->SetActorRotation(FRotator(
				0.0f, TargetRotation.Pitch, 0.0f)); // Fixed: Pitch is actually the Yaw in the calculation above
			// Corrected rotation logic:
			const FRotator NewRotation(0.0f, (TargetPos - StartPos).Rotation().Yaw, 0.0f);
			Seeker->SetActorRotation(NewRotation);
			return;
		}
	}

	// Default to facing the view direction if no target found
	Seeker->SetActorRotation(FRotator(0.0f, ViewRotation.Yaw, 0.0f));
}
