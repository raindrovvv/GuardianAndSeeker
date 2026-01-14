// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/SeekerAI/GS_AIGoalTrigger.h"
#include "AI/SeekerAI/GS_AISeeker.h"
#include "AI/SeekerAI/GS_SeekerAIController.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AGS_AIGoalTrigger::AGS_AIGoalTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Create root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	// Create trigger sphere
	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetupAttachment(RootComponent);
	TriggerSphere->SetSphereRadius(TriggerRadius);
	TriggerSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerSphere->SetGenerateOverlapEvents(true);
	TriggerSphere->SetCanEverAffectNavigation(false);

	// Create visual indicator mesh (simple cylinder or sphere)
	VisualIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualIndicator"));
	VisualIndicator->SetupAttachment(RootComponent);
	VisualIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualIndicator->SetCastShadow(false);

	// Try to use a default mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		VisualIndicator->SetStaticMesh(CylinderMesh.Object);
		VisualIndicator->SetRelativeScale3D(FVector(1.5f, 1.5f, 3.0f));
		VisualIndicator->SetRelativeLocation(FVector(0, 0, 75.0f)); // Raise above ground
	}

#if WITH_EDITORONLY_DATA
	// Create editor billboard for visibility
	EditorBillboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("EditorBillboard"));
	EditorBillboard->SetupAttachment(RootComponent);
	EditorBillboard->bIsEditorOnly = true;
#endif
}

void AGS_AIGoalTrigger::BeginPlay()
{
	Super::BeginPlay();

	// Bind overlap events
	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AGS_AIGoalTrigger::OnTriggerBeginOverlap);
	TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &AGS_AIGoalTrigger::OnTriggerEndOverlap);

	// Update visual appearance
	UpdateVisualIndicator();

	// Register with subsystem
	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->RegisterGoalTrigger(this);
		}
	}
}

void AGS_AIGoalTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister from subsystem
	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->UnregisterGoalTrigger(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AGS_AIGoalTrigger::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Update trigger sphere radius
	if (TriggerSphere)
	{
		TriggerSphere->SetSphereRadius(TriggerRadius);
	}

	// Update visual
	UpdateVisualIndicator();
}

#if WITH_EDITOR
void AGS_AIGoalTrigger::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.Property != nullptr)
	                         ? PropertyChangedEvent.Property->GetFName()
	                         : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AGS_AIGoalTrigger, TriggerRadius))
	{
		if (TriggerSphere)
		{
			TriggerSphere->SetSphereRadius(TriggerRadius);
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AGS_AIGoalTrigger, IndicatorColor) ||
	         PropertyName == GET_MEMBER_NAME_CHECKED(AGS_AIGoalTrigger, bShowVisualIndicator))
	{
		UpdateVisualIndicator();
	}
}
#endif

void AGS_AIGoalTrigger::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                              UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bIsActive || !OtherActor)
	{
		return;
	}

	// Check if it's a Seeker (AI or player controlled)
	if (IsSeekerActor(OtherActor))
	{
		SeekersInTrigger.Add(OtherActor);

		// Broadcast entered event
		OnSeekerEnteredArea.Broadcast(this, OtherActor);

		// Check if we should mark as reached
		if (!bHasBeenReached || !bOnlyTriggerOnce)
		{
			HandleSeekerReachedGoal(OtherActor);
		}
	}
}

void AGS_AIGoalTrigger::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor)
	{
		return;
	}

	// Remove from tracking array
	SeekersInTrigger.RemoveAll([OtherActor](const TWeakObjectPtr<AActor>& WeakActor)
	                           { return !WeakActor.IsValid() || WeakActor.Get() == OtherActor; });
}

void AGS_AIGoalTrigger::UpdateVisualIndicator()
{
	if (!VisualIndicator)
	{
		return;
	}

	// Show/hide visual
	VisualIndicator->SetVisibility(bShowVisualIndicator);

	// Update material color
	if (bShowVisualIndicator)
	{
		UMaterialInstanceDynamic* DynMaterial = VisualIndicator->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMaterial)
		{
			// Adjust color based on state
			FLinearColor CurrentColor = IndicatorColor;
			if (bHasBeenReached)
			{
				CurrentColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.5f); // Gray when reached
			}
			else if (!bIsActive)
			{
				CurrentColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.5f); // Red when inactive
			}

			DynMaterial->SetVectorParameterValue(TEXT("BaseColor"), CurrentColor);
			DynMaterial->SetScalarParameterValue(TEXT("Opacity"), CurrentColor.A);
		}
	}
}

bool AGS_AIGoalTrigger::IsSeekerActor(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	// Check if it's a Seeker character
	if (Cast<AGS_Seeker>(Actor))
	{
		return true;
	}

	// Check if owned by AISeeker
	if (AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(Actor->GetOwner()))
	{
		return true;
	}

	return false;
}

void AGS_AIGoalTrigger::HandleSeekerReachedGoal(AActor* Seeker)
{
	bHasBeenReached = true;

	// Broadcast goal reached event
	OnGoalReached.Broadcast(this, Seeker);

	// Notify AI Controller if it's an AI Seeker
	if (AGS_Seeker* SeekerCharacter = Cast<AGS_Seeker>(Seeker))
	{
		if (AGS_SeekerAIController* AIController = Cast<AGS_SeekerAIController>(SeekerCharacter->GetController()))
		{
			AIController->OnGoalReached();
		}
	}

	// Also check the owner for AI Seeker wrapper
	if (AActor* SeekerOwner = Seeker->GetOwner())
	{
		if (AGS_AISeeker* AISeeker = Cast<AGS_AISeeker>(SeekerOwner))
		{
			AISeeker->NotifyGoalReached();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AGS_AIGoalTrigger: Goal '%s' reached by %s"), *GoalName, *Seeker->GetName());

	// Update visual
	UpdateVisualIndicator();

	// Optionally destroy
	if (bDestroyOnReached)
	{
		SetLifeSpan(1.0f); // Destroy after 1 second
	}
}

void AGS_AIGoalTrigger::ResetGoal()
{
	bHasBeenReached = false;
	SeekersInTrigger.Empty();
	UpdateVisualIndicator();

	UE_LOG(LogTemp, Log, TEXT("AGS_AIGoalTrigger: Goal '%s' reset"), *GoalName);
}

void AGS_AIGoalTrigger::SetGoalActive(bool bActive)
{
	bIsActive = bActive;
	UpdateVisualIndicator();

	// Update collision
	if (TriggerSphere)
	{
		TriggerSphere->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	UE_LOG(LogTemp, Log, TEXT("AGS_AIGoalTrigger: Goal '%s' active state: %s"), *GoalName, bActive ? TEXT("true") : TEXT("false"));
}
