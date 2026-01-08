#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AkGameplayStatics.h"
#include "Sound/GS_AudioMixingComponent.h"
#include "GS_Weapon.generated.h"

class UAkRtpc;
class UGS_AudioMixingComponent;

UCLASS()
class GAS_API AGS_Weapon : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AGS_Weapon();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	/** 타격 임팩트 포커스(BGM Ducking) 효과 적용 (서버/클라이언트 공용) */
	UFUNCTION(BlueprintCallable, Category = "Audio|Impact")
	void ApplyImpactFocus();

	/** 클라이언트에서 실제 사운드를 조절하도록 하는 RPC */
	UFUNCTION(Client, Unreliable)
	void Client_ApplyImpactFocus();

	/** 클라이언트들에게 특수 타격 VFX 재생을 요청하는 RPC (Ares 4타 등) */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySpecialHitVFX(UNiagaraSystem* VFXToPlay, const FHitResult& HitResult);

protected:
	/** 레벨 전환 중이거나 액터가 유효하지 않은지 확인 */
	bool IsValidForLevelTransition() const;

	/** 실제 사운드 조절 로직 (Internal) */
	void Internal_ApplyImpactFocus();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void PostInitializeComponents() override;

	/** BGM 볼륨 복구 */
	void RestoreImpactFocus();

	/** 멀티 레이어 사운드 재생 (피격/잔향 레이어) */
	virtual void PlayLayeredHitSound(const FHitResult& HitResult, AActor* HitActor);

	/** 클라이언트들에게 레이어드 사운드 재생을 요청하는 RPC */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayLayeredHitSound(const FHitResult& HitResult, AActor* HitActor);

protected:
	/** [Mixing] 다이내믹 사운드 믹싱 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<UGS_AudioMixingComponent> AudioMixingComponent;

protected:
	/** 타격 임팩트 포커스(BGM Ducking)를 위한 RTPC 에셋 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|Impact")
	TObjectPtr<UAkRtpc> ImpactFocusRTPC = nullptr;

	/** 덕킹 강도 (0: 소리 없음, 1: 원래 볼륨) */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|Impact")
	float DuckingValue = 0.3f;

	/** 덕킹 회복 시간 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|Impact")
	float FocusDuration = 0.15f;

	FTimerHandle AudioFocusTimerHandle;

protected:
	/** 피격 레이어: 살점 타격음 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|LayeredImpact")
	TObjectPtr<UAkAudioEvent> ImpactFleshSoundEvent = nullptr;

	/** 피격 레이어: 갑옷/금속 타격음 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|LayeredImpact")
	TObjectPtr<UAkAudioEvent> ImpactArmorSoundEvent = nullptr;

	/** 피격 레이어: 돌/단단한 껍질 타격음 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|LayeredImpact")
	TObjectPtr<UAkAudioEvent> ImpactStoneSoundEvent = nullptr;

	/** 피격 레이어: 나무 타격음 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|LayeredImpact")
	TObjectPtr<UAkAudioEvent> ImpactWoodSoundEvent = nullptr;

	/** 피격 레이어: 벽 타격음 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|LayeredImpact")
	TObjectPtr<UAkAudioEvent> ImpactWallSoundEvent = nullptr;

	/** 잔향 레이어: 타격 후 공간 잔향음 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|LayeredImpact")
	TObjectPtr<UAkAudioEvent> ReverbSoundEvent = nullptr;
};
