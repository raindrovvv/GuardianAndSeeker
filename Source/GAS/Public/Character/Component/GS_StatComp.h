#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GS_StatRow.h"
#include "Character/Component/GS_PositiveEffectComponent.h"
#include "GS_StatComp.generated.h"

class AGS_Character;
class UAkAudioEvent;
class UGS_StatComp;
class AGS_Seeker;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCurrentHPChangedDelegate, UGS_StatComp*);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GAS_API UGS_StatComp : public UActorComponent
{
	GENERATED_BODY()

public:
	FOnCurrentHPChangedDelegate OnCurrentHPChanged;

	UGS_StatComp();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Stat")
	TObjectPtr<UDataTable> StatDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<UAnimMontage*> TakeDamageMontages;

	void InitStat(FName RowName);

	//[Change Stats when use buff skills]
	void ChangeStat(const FGS_StatRow& InChangeStat);
	void ResetStat(const FGS_StatRow& InChangeStat);

	UFUNCTION(Server, Reliable)
	void UpdateStat(const FGS_StatRow& RuneStats);

	/**
	 * 데미지 계산 (크리티컬 판정 포함)
	 * @param bOutIsCritical 크리티컬 발생 여부 (출력 파라미터)
	 */
	float CalculateDamage(AGS_Character* InDamageCauser, AGS_Character* InDamagedCharacter, bool& bOutIsCritical, float InSkillCoefficient = 1.f, float SlopeCoefficient = 1.f);

	/** 하위 호환용 오버로드 (크리티컬 정보 필요 없는 경우) */
	float CalculateDamage(AGS_Character* InDamageCauser, AGS_Character* InDamagedCharacter, float InSkillCoefficient = 1.f, float SlopeCoefficient = 1.f);

	//getter
	UFUNCTION(BlueprintCallable, Category = "Stats")
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
	UFUNCTION(BlueprintCallable, Category = "Stats")
	FORCEINLINE float GetCurrentHealth() const { return CurrentHealth; }
	FORCEINLINE float GetAttackPower() const { return AttackPower; }
	FORCEINLINE float GetDefense() const { return Defense; }
	FORCEINLINE float GetAgility() const { return Agility; }
	FORCEINLINE float GetAttackSpeed() const { return AttackSpeed; }
	FORCEINLINE float GetCriticalRate() const { return CriticalRate; }
	FORCEINLINE float GetCriticalDamage() const { return CriticalDamage; }

	//setter
	void SetCurrentHealth(float InHealth, bool bIsHealing);
	void SetMaxHealth(float InMaxHealth);
	void SetAttackPower(float InAttackPower);
	void SetDefense(float InDefense);
	void SetAgility(float InAgility);
	void SetAttackSpeed(float InAttackSpeed);

	/** 힐 이펙트 없이 직접 HP 설정 (부활 등 특수 상황용) */
	void DirectSetHealth(float InHealth);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPCPlayTakeDamageMontage();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPCNotifyPositiveEffect(EPositiveEffectType EffectType);

	UFUNCTION()
	void OnRep_CurrentHealth(float OldHealth);

	// heal system
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerRPCHeal(float InHealAmount);

protected:
	float CharacterWalkSpeed;

private:
	//stat
	UPROPERTY(VisibleAnywhere)
	float MaxHealth = 1000.f;
	UPROPERTY(ReplicatedUsing = OnRep_CurrentHealth)
	float CurrentHealth;
	UPROPERTY(EditDefaultsOnly)
	float AttackPower;
	UPROPERTY(EditDefaultsOnly)
	float Defense;
	UPROPERTY(EditDefaultsOnly)
	float Agility; // 민첩
	UPROPERTY(EditDefaultsOnly)
	float AttackSpeed; // 공격속도
	UPROPERTY(EditDefaultsOnly)
	float CriticalRate = 0.1f; // 크리티컬 확률 (10%)
	UPROPERTY(EditDefaultsOnly)
	float CriticalDamage = 1.5f; // 크리티컬 배율 (150%)

	UFUNCTION()
	void OnDamageMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	//Enum 통일 전 임시
	UFUNCTION()
	ECharacterClass MapCharacterTypeToCharacterClass(ECharacterType CharacterType);

	// Health 변화 처리 헬퍼 함수 (RepNotify + Server 공통)
	void HandleHealthDamage(float OldHealth, float NewHealth);

	/**
	 * 시커의 빈사 상태 진입 처리
	 *
	 * @param Seeker 빈사 상태로 전환할 시커
	 */
	void HandleSeekerDyingTransition(AGS_Seeker* Seeker);
};
