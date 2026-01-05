// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "E_HitReact.h"
#include "GS_HitReactComp.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHitReactEnd, UAnimMontage*, Montage, bool, bInterrupted);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))

class GAS_API UGS_HitReactComp : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_HitReactComp();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<UAnimMontage*> AM_HitReacts;

	UFUNCTION()
	void PlayHitReact(EHitReactType ReactType, FVector HitDirection);

	UFUNCTION()
	void StopHitReact(UAnimMontage* TargetMontage);

	UFUNCTION()
	FName CalculateHitDirection(FVector HitDirection);

	/*UFUNCTION()
	void CheckAxeState(UAnimMontage* Montage, bool bInterrupted);*/
	// SJE

	FOnMontageEnded HitReactEndDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHitReactEnd OnHitReactEnd;

	UFUNCTION()
	void OnEndDelegate(UAnimMontage* Montage, bool bInterrupted);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ============================================
	// Hit React Cooldown System
	// ============================================
	/** 마지막 피격모션이 재생된 시간 */
	float LastHitReactTime = 0.f;

	/** 피격모션 쿨다운 시간 (이 시간 내 연속 피격 시 DamageOnly 처리) */
	UPROPERTY(EditDefaultsOnly, Category = "Hit React")
	float HitReactCooldown = 1.5f;
};