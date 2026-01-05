// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GS_ArrowVisualActor.generated.h"

class UGS_VisualPoolComp;

UCLASS()
class GAS_API AGS_ArrowVisualActor : public AActor
{
	GENERATED_BODY()

public:
	AGS_ArrowVisualActor();

	UPROPERTY()
	UGS_VisualPoolComp* OwningPool;

	UFUNCTION(BlueprintCallable)
	void Activate(const FVector& Location, const FRotator& Rotation);

	UFUNCTION(BlueprintCallable)
	void Deactivate();

	bool IsReady() const { return !bActive; }

	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* ArrowMesh;

	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root;

	UPROPERTY(ReplicatedUsing = OnRep_SkeletalMesh)
	USkeletalMesh* CurrentMesh;

	UFUNCTION()
	void OnRep_SkeletalMesh();

	UPROPERTY(ReplicatedUsing = OnRep_Active)
	bool bActive = false;

	UFUNCTION()
	void OnRep_Active();

	UPROPERTY(EditAnywhere, Category = "Projectile")
	float LifeTime = 5.0f; // 5초 후 소멸

	void SetArrowMesh(USkeletalMesh* Mesh);
	void SetAttachedTargetActor(AActor* Target);

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** 생명주기 만료 시 풀에 반환 */
	UFUNCTION()
	void OnLifeTimeExpired();

private:
	FTimerHandle LifeTimeTimerHandle;

private:
	UPROPERTY()
	AActor* AttachedTargetActor;

	UFUNCTION()
	void OnAttachedTargetDestroyed(AActor* DestroyedActor);
};
