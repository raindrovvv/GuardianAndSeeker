// Copyright Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/Skill/Seeker/GS_SeekerSkillBase.h"
#include "GS_HealSkill.generated.h"

/**
 * @brief Universal healing skill used by all Seeker characters.
 * Manages a limited pool of health potions, provides temporary health restoration,
 * and handles interrupt logic during the drinking animation.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GS Seeker Heal Skill"))
class GAS_API UGS_HealSkill : public UGS_SeekerSkillBase
{
	GENERATED_BODY()

public:
	UGS_HealSkill();

	// UGS_SkillBase interface
	virtual void ActiveSkill() override;
	virtual void DeactiveSkill() override;
	virtual void InterruptSkill() override;
	virtual bool CanActive() const override;
	// ~UGS_SkillBase interface

	// UGS_SeekerSkillBase interface
	virtual void InitializeDelegate() override;
	// ~UGS_SeekerSkillBase interface

	/** Triggers when the healing montage ends naturally */
	UFUNCTION()
	void HandleHealMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Callback when the owner character takes damage to potentially reset 'HealthFull' block */
	UFUNCTION()
	void HandleOwnerDamaged(AActor* DamagedActor,
							float DamageAmount,
							const class UDamageType* DamageType,
							class AController* InstigatedBy,
							AActor* DamageCauser);

	/** Manually set the current number of potions */
	UFUNCTION(BlueprintCallable, Category = "Heal|State")
	void SetCurrentHealCount(int32 NewCount);

	/** Returns true if the seeker has at least one potion remaining */
	UFUNCTION(BlueprintPure, Category = "Heal|State")
	bool HasPotionsRemaining() const
	{
		return CurrentHealCount > 0;
	}

	/** Returns true if the character's health is already at max */
	UFUNCTION(BlueprintPure, Category = "Heal|State")
	bool IsCharacterHealthFull() const;

	/** Internal check for all conditions required to trigger a heal */
	UFUNCTION(BlueprintPure, Category = "Heal|State")
	bool CanActivateHealLogic() const;

	/** Decrements the potion count by one */
	void ConsumeHealCharge();

	// Getters
	UFUNCTION(BlueprintPure, Category = "Heal|Stats")
	float GetHealAmountValue() const
	{
		return HealAmountValue;
	}

	UFUNCTION(BlueprintPure, Category = "Heal|Stats")
	int32 GetCurrentHealCount() const
	{
		return CurrentHealCount;
	}

	UFUNCTION(BlueprintPure, Category = "Heal|Stats")
	int32 GetMaxHealCount() const
	{
		return MaxHealthPotions;
	}

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Amount of health restored per drink */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heal|Stats")
	float HealAmountValue = 200.0f;

	/** Maximum number of health potions the character can carry */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heal|Stats")
	int32 MaxHealthPotions = 5;

	/** Current number of available potions */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentHealCount, BlueprintReadOnly, Category = "Heal|State")
	int32 CurrentHealCount = 0;

	/** Internal flag indicating the skill is blocked due to empty potions or full health */
	UPROPERTY(BlueprintReadOnly, Category = "Heal|State")
	bool bIsActivationBlocked = false;

	/** Notifies client when potion count is replicated */
	UFUNCTION()
	void OnRep_CurrentHealCount();

	/** Weak pointer to the seeker character owning this skill */
	UPROPERTY()
	TWeakObjectPtr<class AGS_Seeker> CachedSeeker;

private:
	/** Broadcasts a notification when the user tries to heal without charges */
	void NotifyHealBlocked();
};
