#include "Sound/GS_AudioMixingComponent.h"
#include "AkAudioEvent.h"
#include "AkRtpc.h"
#include "AkGameplayStatics.h"
#include "Kismet/GameplayStatics.h"
#include "AI/RTS/GS_RTSController.h"
#include "Weapon/Equipable/GS_WeaponSword.h"
#include "Weapon/Equipable/GS_WeaponAxe.h"
#include "Character/GS_Character.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Engine/World.h"
#include "AkAudioDevice.h"
#include "AkGameplayStatics.h"

UGS_AudioMixingComponent::UGS_AudioMixingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGS_AudioMixingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UGS_AudioMixingComponent::PostLayeredSound(UAkAudioEvent* ImpactEvent, UAkAudioEvent* ReverbEvent, const FVector& Location, AActor* HitActor)
{
	// 월드 유효성 체크
	if (!GetWorld() || IsRunningDedicatedServer())
	{
		return;
	}

	// 1. 사운드 인스턴스 제한 및 중합(Clutter) 방지
	// 여러 적을 동시 타격하는 경우를 고려하여 제한을 15로 상향
	if (CurrentActiveSounds >= 15)
	{
		return;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastPlayTime < ClutterCooldown)
	{
		return;
	}
	LastPlayTime = CurrentTime;

	// 2. 믹싱 가중치 계산
	float FinalImpactWeight = ImpactVolumeWeight;
	float FinalReverbWeight = ReverbVolumeWeight;

	// 조종 중인 캐릭터인지 확인 (타인의 소음 방지)
	bool bIsLocallyControlled = false;
	AActor* CurrentOwner = GetOwner();
	while (CurrentOwner)
	{
		if (APawn* Pawn = Cast<APawn>(CurrentOwner))
		{
			bIsLocallyControlled = Pawn->IsLocallyControlled();
			break;
		}
		CurrentOwner = CurrentOwner->GetOwner();
	}

	if (bEnableDistanceMixing)
	{
		FVector ListenerLocation;
		if (GetListenerLocation(ListenerLocation))
		{
			float DistanceSq = FVector::DistSquared(Location, ListenerLocation);

			// 자신이 조종하지 않는 캐릭터의 타격음은 40m로 제한
			float CullDistance = bIsLocallyControlled ? GS_Rendering::VFX_DISABLE_DISTANCE : GS_Rendering::MONSTER_SMALL_CULL_DISTANCE;

			if (DistanceSq > FMath::Square(CullDistance))
			{
				return;
			}

			float Distance = FMath::Sqrt(DistanceSq);
			// 30m 거리에서 임팩트음은 절반으로 줄고, 잔향은 상대적으로 강조되는 커브
			float DistanceAlpha = FMath::Clamp(Distance / 3000.0f, 0.0f, 1.0f);

			FinalImpactWeight *= (1.0f - (DistanceAlpha * 0.5f));
			FinalReverbWeight *= (0.9f + (DistanceAlpha * 0.1f));
		}
	}

	// 3. RTPC 적용 (글로벌 팝핑 방지를 위해 대상 액터 기반으로 설정)
	AActor* SoundOwner = HitActor ? HitActor : GetOwner();
	ApplyMixingRTPCs(FinalImpactWeight, FinalReverbWeight, SoundOwner);

	// 4. 사운드 재생 - 정확한 위치(Location)에서 소리 재생
	// PostEvent 대신 PostEventAtLocation을 사용하여 피격 위치에서 정확히 소리가 나도록 함
	// 이렇게 해야 리스너(내 캐릭터)와의 거리/장애물에 따른 감쇠가 올바르게 적용됨
	FOnAkPostEventCallback Callback;
	Callback.BindUFunction(this, FName("OnSoundInstanceFinished"));

	bool bSoundStarted = false;

	if (ImpactEvent)
	{
		// PostEventAtLocation: 특정 월드 좌표(Location)에서 소리 재생
		// 잔향이 있으면 잔향 끝날 때 카운트를 줄이고, 없으면 임팩트 끝날 때 줄임
		uint8 CBType = ReverbEvent ? 0 : (uint8)EAkCallbackType::EndOfEvent;
		FOnAkPostEventCallback CB = ReverbEvent ? FOnAkPostEventCallback() : Callback;

		if (UAkGameplayStatics::PostEventAtLocation(ImpactEvent, Location, FRotator::ZeroRotator, GetWorld()) != AK_INVALID_PLAYING_ID)
		{
			bSoundStarted = true;
		}
	}

	if (ReverbEvent)
	{
		// 잔향음도 동일한 위치에서 재생
		if (UAkGameplayStatics::PostEventAtLocation(ReverbEvent, Location, FRotator::ZeroRotator, GetWorld()) != AK_INVALID_PLAYING_ID)
		{
			bSoundStarted = true;
		}
	}

	if (bSoundStarted)
	{
		CurrentActiveSounds++;
	}
}

void UGS_AudioMixingComponent::OnSoundInstanceFinished(EAkCallbackType CallbackType, UAkCallbackInfo* CallbackInfo)
{
	CurrentActiveSounds = FMath::Max(0, CurrentActiveSounds - 1);
}

void UGS_AudioMixingComponent::ApplyMixingRTPCs(float ImpactWeight, float ReverbWeight, AActor* TargetActor)
{
	// Wwise RTPC를 특정 액터에 바인딩하여 전역 팝핑 현상 방지


	if (ImpactVolumeRTPC)
	{
		UAkGameplayStatics::SetRTPCValue(ImpactVolumeRTPC, ImpactWeight, 0, TargetActor);
	}

	if (ReverbVolumeRTPC)
	{
		UAkGameplayStatics::SetRTPCValue(ReverbVolumeRTPC, ReverbWeight, 0, TargetActor);
	}
}

bool UGS_AudioMixingComponent::GetListenerLocation(FVector& OutLocation) const
{
	if (!GetWorld())
		return false;

	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!LocalPC)
		return false;

	if (AGS_RTSController* RTSController = Cast<AGS_RTSController>(LocalPC))
	{
		if (RTSController->GetViewTarget())
		{
			OutLocation = RTSController->GetViewTarget()->GetActorLocation();
			return true;
		}
	}
	else if (LocalPC->GetPawn())
	{
		OutLocation = LocalPC->GetPawn()->GetActorLocation();
		return true;
	}

	return false;
}

bool UGS_AudioMixingComponent::IsRTSMode() const
{
	if (!GetWorld())
		return false;
	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	return LocalPC && Cast<AGS_RTSController>(LocalPC) != nullptr;
}
