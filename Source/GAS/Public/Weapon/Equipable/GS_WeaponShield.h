// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GS_WeaponEquipable.h"
#include "Engine/HitResult.h"
#include "AkAudioEvent.h"
#include "NiagaraSystem.h"
#include "GS_WeaponShield.generated.h"

UENUM(BlueprintType)
enum class EShieldHitTargetType : uint8
{
	Guardian UMETA(DisplayName = "Guardian"),
	DungeonMonster UMETA(DisplayName = "DungeonMonster"),
	Seeker UMETA(DisplayName = "Seeker"),
	Structure UMETA(DisplayName = "Structure"),
	Other UMETA(DisplayName = "Other")
};

UCLASS()
class GAS_API AGS_WeaponShield : public AGS_WeaponEquipable
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AGS_WeaponShield();

	UPROPERTY(VisibleAnywhere, Category = "Mesh")
	USkeletalMeshComponent* ShieldMeshComponent;

	// 공격용 콜리전 이벤트
	UFUNCTION()
	void OnAttackHit(
	    UPrimitiveComponent* OverlappedComponent,
	    AActor* OtherActor,
	    UPrimitiveComponent* OtherComp,
	    int32 OtherBodyIndex,
	    bool bFromSweep,
	    const FHitResult& SweepResult);

	// 방어용 콜리전 이벤트
	UFUNCTION()
	void OnDefenseHit(
	    UPrimitiveComponent* OverlappedComponent,
	    AActor* OtherActor,
	    UPrimitiveComponent* OtherComp,
	    int32 OtherBodyIndex,
	    bool bFromSweep,
	    const FHitResult& SweepResult);

	// 방어용 콜리전 종료 이벤트
	UFUNCTION()
	void OnDefenseEndOverlap(
	    UPrimitiveComponent* OverlappedComponent,
	    AActor* OtherActor,
	    UPrimitiveComponent* OtherComp,
	    int32 OtherBodyIndex);

	// 공격용 콜리전 제어
	UFUNCTION()
	void EnableAttackHit();

	UFUNCTION()
	void DisableAttackHit();

	UFUNCTION(Server, Reliable)
	void ServerEnableAttackHit();
	UFUNCTION(Server, Reliable)
	void ServerDisableAttackHit();

	// 방어용 콜리전 제어
	UFUNCTION()
	void EnableDefenseHit();

	UFUNCTION()
	void DisableDefenseHit();

	UFUNCTION(Server, Reliable)
	void ServerEnableDefenseHit();

	UFUNCTION(Server, Reliable)
	void ServerDisableDefenseHit();

	// 방어 성공 효과 재생 (외부 호출용)
	UFUNCTION(BlueprintCallable, Category = "Defense")
	void PlayDefenseEffects(AActor* Attacker, const FHitResult& HitResult);

	// 기존 호환성을 위한 함수들 (공격용으로 리다이렉트)
	virtual void EnableHit() override;
	virtual void DisableHit() override;

	virtual void ServerEnableHit_Implementation() override;
	virtual void ServerDisableHit_Implementation() override;

	// 히트 사운드 에셋들
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	UAkAudioEvent* HitPawnSoundEvent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	UAkAudioEvent* HitStructureSoundEvent;

	// 가드 성공 사운드 에셋들
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Defense")
	UAkAudioEvent* GuardSuccessPawnSoundEvent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Defense")
	UAkAudioEvent* GuardSuccessStructureSoundEvent;

	// 히트 VFX 에셋들
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	UNiagaraSystem* HitPawnVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	UNiagaraSystem* HitStructureVFX;

	// 가드 성공 VFX 에셋들
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|Defense")
	UNiagaraSystem* GuardSuccessPawnVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|Defense")
	UNiagaraSystem* GuardSuccessStructureVFX;

	// 공격용 히트 액터 목록 (중복 히트 방지)
	UPROPERTY()
	TSet<AActor*> AttackHitActors;

	// 방어용 히트 액터 목록 (중복 히트 방지)
	UPROPERTY()
	TSet<AActor*> DefenseHitActors;

	// 방어용 콜리전
	UPROPERTY(VisibleAnywhere, Category = "Defense")
	class UBoxComponent* DefenseHitBox;

protected:
	AGS_Character* FindUltimateAttacker(AActor* InActor);

	FTimerHandle DefenseTimerHandle;
	void OnDefenseTimer();

	// Called when the game starts or when spawned
	virtual void PostInitializeComponents() override;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 공격용 콜리전
	UPROPERTY(VisibleAnywhere, Category = "Attack")
	class UBoxComponent* AttackHitBox;

	// 히트 이펙트 관련 함수들
	virtual EShieldHitTargetType DetermineTargetType(AActor* OtherActor) const;
	virtual void PlayHitSound(EShieldHitTargetType TargetType, const FHitResult& SweepResult);
	virtual void PlayHitVFX(EShieldHitTargetType TargetType, const FHitResult& SweepResult);
	virtual void PlayGuardSuccessVFX(EShieldHitTargetType TargetType, const FHitResult& SweepResult);
	virtual void PlayGuardSuccessSound(EShieldHitTargetType TargetType, const FHitResult& SweepResult);
	virtual class UBoxComponent* GetHitBox() const override;


	// 특화 헬퍼 함수
	void DisableAllCollisions();

	// 멀티캐스트 함수들
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitSound(EShieldHitTargetType TargetType, const FHitResult& SweepResult);
	bool Multicast_PlayHitSound_Validate(EShieldHitTargetType TargetType, const FHitResult& SweepResult);
	void Multicast_PlayHitSound_Implementation(EShieldHitTargetType TargetType, const FHitResult& SweepResult);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitVFX(EShieldHitTargetType TargetType, const FHitResult& SweepResult);
	bool Multicast_PlayHitVFX_Validate(EShieldHitTargetType TargetType, const FHitResult& SweepResult);
	void Multicast_PlayHitVFX_Implementation(EShieldHitTargetType TargetType, const FHitResult& SweepResult);


	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayGuardSuccessVFX(EShieldHitTargetType TargetType, const FHitResult& SweepResult);
	bool Multicast_PlayGuardSuccessVFX_Validate(EShieldHitTargetType TargetType, const FHitResult& SweepResult);
	void Multicast_PlayGuardSuccessVFX_Implementation(EShieldHitTargetType TargetType, const FHitResult& SweepResult);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayGuardSuccessSound(EShieldHitTargetType TargetType, const FHitResult& SweepResult);
	bool Multicast_PlayGuardSuccessSound_Validate(EShieldHitTargetType TargetType, const FHitResult& SweepResult);
	void Multicast_PlayGuardSuccessSound_Implementation(EShieldHitTargetType TargetType, const FHitResult& SweepResult);
};
