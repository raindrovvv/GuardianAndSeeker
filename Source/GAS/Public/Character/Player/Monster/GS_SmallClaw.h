// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "NiagaraSystem.h"
#include "GS_SmallClaw.generated.h"

class UAkAudioEvent;

/**
 * 
 */
UCLASS()
class GAS_API AGS_SmallClaw : public AGS_Monster
{
	GENERATED_BODY()

public:
	AGS_SmallClaw();

	UPROPERTY(VisibleAnywhere, Category = "Attack")
	class UBoxComponent* BiteCollision;

	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	UNiagaraSystem* BloodEffectSystem;

	UFUNCTION()
	void SetBiteCollision(bool bEnable);

	UFUNCTION()
	void OnAttackBiteboxOverlap(
	    UPrimitiveComponent* OverlappedComponent,
	    AActor* OtherActor,
	    UPrimitiveComponent* OtherComp,
	    int32 OtherBodyIndex,
	    bool bFromSweep,
	    const FHitResult& SweepResult);

protected:
	virtual void BeginPlay() override;
	virtual float GetOptimalCullDistance() const override;

	/** 피격 레이어: 살점 타격음 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|LayeredImpact")
	TObjectPtr<UAkAudioEvent> ImpactFleshSoundEvent = nullptr;

	/** 피격 레이어: 갑옷/금속 타격음 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|LayeredImpact")
	TObjectPtr<UAkAudioEvent> ImpactArmorSoundEvent = nullptr;

	/** 잔향 레이어: 타격 후 공간 잔향음 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|LayeredImpact")
	TObjectPtr<UAkAudioEvent> ReverbSoundEvent = nullptr;

	/** 레이어드 사운드 재생 */
	void PlayLayeredHitSound(const FHitResult& HitResult, AActor* HitActor);

private:
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayBloodEffect(FVector HitLocation, FVector HitNormal);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayLayeredHitSound(const FHitResult& HitResult, AActor* HitActor);
};
