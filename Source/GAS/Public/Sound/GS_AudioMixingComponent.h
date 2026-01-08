#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AkGameplayStatics.h"
#include "GS_AudioMixingComponent.generated.h"

class UAkAudioEvent;
class UAkRtpc;

/**
 * 다이내믹 사운드 믹싱 및 레이어드 사운드 제어를 위한 컴포넌트.
 * 무기 타격음, 캐릭터 발소리 등 거리와 상황에 따른 정교한 사운드 믹싱이 필요한 모든 액터에서 사용 가능.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class GAS_API UGS_AudioMixingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_AudioMixingComponent();

protected:
	virtual void BeginPlay() override;

public:
	/**
	 * 레이어드 사운드(임팩트 + 잔향)를 믹싱 규칭에 따라 재생합니다.
	 * 
	 * @param ImpactEvent 메인 타격음/충격음 레이어
	 * @param ReverbEvent 공간감을 위한 잔향 레이어
	 * @param Location 재생 위치
	 * @param HitActor 타겟 액터 (필요 시 거리 계산 등에 활용)
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Mixing")
	void PostLayeredSound(UAkAudioEvent* ImpactEvent, UAkAudioEvent* ReverbEvent, const FVector& Location, AActor* HitActor = nullptr);

	/** 현재 설정된 믹싱 파라미터를 기반으로 RTPC 값을 Wwise에 적용합니다. */
	void ApplyMixingRTPCs(float ImpactWeight, float ReverbWeight, AActor* TargetActor);

	/** 믹싱에 필요한 리스너 위치를 가져옵니다 (RTS/TPS 모드 지원). */
	bool GetListenerLocation(FVector& OutLocation) const;

	/** 현재 게임이 RTS 모드인지 확인합니다. */
	bool IsRTSMode() const;

private:
protected:
	/** [Mixing] 임팩트(메인) 레이어 기본 볼륨 가중치 (0~1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Mixing")
	float ImpactVolumeWeight = 1.0f;

	/** [Mixing] 잔향 레이어 기본 볼륨 가중치 (0~1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Mixing")
	float ReverbVolumeWeight = 0.7f;

	/** [Mixing] 거리 기반 자동 믹싱 활성화 (멀수록 잔향 비중 증가) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Mixing")
	bool bEnableDistanceMixing = true;

	/** [Mixing] 사운드 중첩 방지 쿨타임 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Mixing")
	float ClutterCooldown = 0.05f;

	/** [Mixing] RTPC 업데이트 임계값 (이 변화량보다 작으면 전송하지 않음) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Mixing")
	float RTPCUpdateThreshold = 0.05f;

	/** [RTPC] 임팩트 볼륨 제어용 Wwise RTPC */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Mixing|RTPC")
	TObjectPtr<UAkRtpc> ImpactVolumeRTPC = nullptr;

	/** [RTPC] 잔향 볼륨 제어용 Wwise RTPC */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Mixing|RTPC")
	TObjectPtr<UAkRtpc> ReverbVolumeRTPC = nullptr;

	/** [Mixing] 동시 재생 가능한 최대 레이어드 사운드 수 (사운드 중첩 방지) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Mixing")
	int32 MaxSimultaneousSounds = 3;

private:
	/** 현재 재생 중인 레이어드 사운드 수 */
	int32 CurrentActiveSounds = 0;

	/** 마지막 사운드 재생 시간 (중합 방지용) */
	float LastPlayTime = 0.0f;

	/** 마지막으로 전송된 RTPC 값 (최적화용) */
	float LastImpactWeight = -1.0f;
	float LastReverbWeight = -1.0f;

	/** 사운드 종료 시 호출되는 콜백 함수 */
	UFUNCTION()
	void OnSoundInstanceFinished(EAkCallbackType CallbackType, UAkCallbackInfo* CallbackInfo);
};
