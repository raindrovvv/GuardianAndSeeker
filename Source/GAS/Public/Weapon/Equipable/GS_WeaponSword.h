// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/Equipable/GS_WeaponEquipable.h"
#include "Engine/HitResult.h"
#include "AkAudioEvent.h"
#include "NiagaraSystem.h"
#include "GS_WeaponSword.generated.h"

UENUM(BlueprintType)
enum class ESwordHitTargetType : uint8
{
	Guardian UMETA(DisplayName = "Guardian"),
	DungeonMonster UMETA(DisplayName = "DungeonMonster"),
	Seeker UMETA(DisplayName = "Seeker"),
	Structure UMETA(DisplayName = "Structure"),
	Other UMETA(DisplayName = "Other")
};

/**
 * 
 */
UCLASS()
class GAS_API AGS_WeaponSword : public AGS_WeaponEquipable
{
	GENERATED_BODY()

public:
	AGS_WeaponSword();

	virtual void EnableHit() override;
	virtual void DisableHit() override;

	virtual void ServerEnableHit_Implementation() override;
	virtual void ServerDisableHit_Implementation() override;

	UFUNCTION()
	void OnHit(
	    UPrimitiveComponent* OverlappedComponent,
	    AActor* OtherActor,
	    UPrimitiveComponent* OtherComp,
	    int32 OtherBodyIndex,
	    bool bFromSweep,
	    const FHitResult& SweepResult);

	// 히트 사운드 에셋들
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	UAkAudioEvent* HitPawnSoundEvent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	UAkAudioEvent* HitStructureSoundEvent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	UAkAudioEvent* HitSeekerSoundEvent;

	// 히트 VFX 에셋들
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	UNiagaraSystem* HitPawnVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	UNiagaraSystem* HitStructureVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	UNiagaraSystem* HitSeekerVFX;


protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Attack")
	USkeletalMeshComponent* Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Attack")
	class UBoxComponent* HitBox;

	// 히트 이펙트 관련 함수들
	virtual ESwordHitTargetType DetermineTargetType(AActor* OtherActor) const;
	virtual void PlayHitSound(ESwordHitTargetType TargetType, const FHitResult& SweepResult);
	virtual void PlayHitVFX(ESwordHitTargetType TargetType, const FHitResult& SweepResult);

	// FHitResult 보정 로직 (부모 클래스의 CalculateMoreAccurateHitPoint 활용)
	FHitResult CreateCorrectHitResult(const FHitResult& OriginalResult, bool bFromSweep) const override;
	virtual class UBoxComponent* GetHitBox() const override;

	// 멀티캐스트 함수들
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitSound(ESwordHitTargetType TargetType, const FHitResult& SweepResult);
	bool Multicast_PlayHitSound_Validate(ESwordHitTargetType TargetType, const FHitResult& SweepResult);
	void Multicast_PlayHitSound_Implementation(ESwordHitTargetType TargetType, const FHitResult& SweepResult);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitVFX(ESwordHitTargetType TargetType, const FHitResult& SweepResult);
	bool Multicast_PlayHitVFX_Validate(ESwordHitTargetType TargetType, const FHitResult& SweepResult);
	void Multicast_PlayHitVFX_Implementation(ESwordHitTargetType TargetType, const FHitResult& SweepResult);
};
