// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "Components/BillboardComponent.h"
#include "GS_AIGoalTrigger.generated.h"

class AGS_AISeeker;
class AGS_Seeker;

/**
 * Goal Trigger for AI Seeker
 * Place this in the dungeon as the destination for AI Seekers to reach
 */
UCLASS(Blueprintable, BlueprintType)
class GAS_API AGS_AIGoalTrigger : public AActor
{
	GENERATED_BODY()

public:
	AGS_AIGoalTrigger();

	// Trigger Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goal Trigger|Setup")
	float TriggerRadius = 150.0f;

	// Visual Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goal Trigger|Visual")
	bool bShowVisualIndicator = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goal Trigger|Visual")
	FLinearColor IndicatorColor = FLinearColor(0.0f, 1.0f, 0.5f, 1.0f);

	// Goal Properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goal Trigger|Properties")
	FString GoalName = TEXT("Dungeon Exit");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goal Trigger|Properties")
	int32 GoalPriority = 0; // Higher priority goals are preferred

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goal Trigger|Properties")
	bool bDestroyOnReached = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goal Trigger|Properties")
	bool bOnlyTriggerOnce = true;

	// Events
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGoalReached, AGS_AIGoalTrigger*, Goal, AActor*, Seeker);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSeekerEnteredArea, AGS_AIGoalTrigger*, Goal, AActor*, Seeker);

	UPROPERTY(BlueprintAssignable, Category = "Goal Trigger|Events")
	FOnGoalReached OnGoalReached;

	UPROPERTY(BlueprintAssignable, Category = "Goal Trigger|Events")
	FOnSeekerEnteredArea OnSeekerEnteredArea;

	// Functions
	UFUNCTION(BlueprintPure, Category = "Goal Trigger")
	FVector GetGoalLocation() const { return GetActorLocation(); }

	UFUNCTION(BlueprintPure, Category = "Goal Trigger")
	float GetTriggerRadius() const { return TriggerRadius; }

	UFUNCTION(BlueprintPure, Category = "Goal Trigger")
	bool HasBeenReached() const { return bHasBeenReached; }

	UFUNCTION(BlueprintCallable, Category = "Goal Trigger")
	void ResetGoal();

	UFUNCTION(BlueprintCallable, Category = "Goal Trigger")
	void SetGoalActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Goal Trigger")
	bool IsGoalActive() const { return bIsActive; }

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* TriggerSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* VisualIndicator;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UBillboardComponent* EditorBillboard;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                           UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                         UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	UPROPERTY()
	bool bHasBeenReached = false;

	UPROPERTY()
	bool bIsActive = true;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SeekersInTrigger;

	void UpdateVisualIndicator();
	bool IsSeekerActor(AActor* Actor) const;
	void HandleSeekerReachedGoal(AActor* Seeker);
};
