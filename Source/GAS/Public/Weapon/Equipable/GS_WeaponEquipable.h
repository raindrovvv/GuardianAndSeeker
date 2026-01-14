// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/GS_Weapon.h"
#include "Weapon/Component/GS_WeaponVFXComponent.h"
#include "Character/GS_Character.h"
#include "GS_WeaponEquipable.generated.h"

class AGS_Seeker;
class UBoxComponent;

/** 타격 판정 전용 콜리전 채널 (DefaultEngine.ini의 GameTraceChannel7) */
#define COLLISION_WEAPON_HIT ECC_GameTraceChannel7

UCLASS()
class GAS_API AGS_WeaponEquipable : public AGS_Weapon
{
	GENERATED_BODY()
public:
	AGS_WeaponEquipable();

	AGS_Character* GetOwnerChar() { return OwnerChar; };

protected:
	virtual void PostInitializeComponents() override;

	// 헬퍼 함수들
	bool IsOwnerCharValid() const;
	void ClearHitActors();
	virtual void ClearSafetyTimer();
	virtual FHitResult CreateCorrectHitResult(const FHitResult& OriginalResult, bool bFromSweep) const;

	// 더 정확한 히트 포인트 계산 (라인 트레이스 활용)
	virtual FHitResult CalculateMoreAccurateHitPoint(AActor* OtherActor) const;

	// 각 무기클래스의 히트박스 컴포넌트를 반환 (자식에서 오버라이드)
	virtual class UBoxComponent* GetHitBox() const { return nullptr; }

	/** 리스너(카메라) 위치를 가져옵니다. (RTS/TPS 대응) */
	bool GetListenerLocation(FVector& OutLocation) const;

	/** 안전한 거리 기반 히트 사운드 재생 */
	void PlayHitSoundAtLocation(class UAkAudioEvent* SoundEvent, const FVector& Location);

	/**
	 * 다음 프레임에 안전하게 HitBox 콜리전을 비활성화합니다.
	 * 물리 엔진이 현재 프레임의 오버랩 쿼리를 완료한 후 비활성화하여
	 * ensure() 오류를 방지합니다.
	 */
	/** 가디언/몬스터 인식용 전면 공격 각도 임계값 (140도) */
	static constexpr float ATTACK_FRONT_ANGLE = 140.0f;

	/** 무기 활성화 노티파이가 누락될 경우를 대비한 자동 비활성화 기본 시간 */
	static constexpr float DEFAULT_SAFETY_DURATION = 3.0f;

	/** 타겟이 캐릭터의 앞쪽 각도 내에 있는지 확인합니다. */
	bool IsInFrontAngle(AActor* TargetActor, float AngleDegrees = ATTACK_FRONT_ANGLE) const;

	void SafeDisableHitBoxCollision(UBoxComponent* InHitBox);

public:
	// 무기 히트박스 제어 (상속받는 무기에서 구현)
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void EnableHit() {}

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void DisableHit() {}

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Weapon")
	virtual void ServerEnableHit();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Weapon")
	virtual void ServerDisableHit();

	/**
	 * 모든 노티파이 카운트를 초기화하고 콜리전을 즉시 비활성화합니다.
	 * 상태 이상이나 사망 시 호출하여 콜리전 잔류를 방지합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ForceDisableHit();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Weapon")
	void ServerForceDisableHit();

protected:
	// 공통 멤버 변수들
	UPROPERTY()
	class AGS_Character* OwnerChar;

	UPROPERTY()
	TSet<AActor*> HitActors;

	// 현재 활성화된 타격 노티파이 개수 (중첩된 노티파이 타이밍 이슈 해결용)
	UPROPERTY(Replicated)
	int32 ActiveNotifyCount = 0;

	FTimerHandle SafetyTimerHandle;

	// 무기 VFX 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UGS_WeaponVFXComponent* WeaponVFXComponent;

public:
	// 무기 VFX 관련 함수들
	UFUNCTION(BlueprintCallable, Category = "WeaponVFX")
	void TriggerHitAuraOnHit(class AGS_Character* HitTarget);

	UFUNCTION(BlueprintCallable, Category = "WeaponVFX")
	ESeekerAuraType GetSeekerAuraType(class AGS_Character* SeekerChar) const;

protected:
	// 아우라 트리거 조건 확인
	virtual bool ShouldTriggerAuraOnHit(class AGS_Character* HitTarget) const;
};
