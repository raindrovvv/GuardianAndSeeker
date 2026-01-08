#include "Character/Component/GS_DrakharAudioComponent.h"
#include "Character/Player/Guardian/GS_Drakhar.h"
#include "AkGameplayStatics.h"
#include "AkAudioEvent.h"
#include "AkComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AkAudioDevice.h"

UGS_DrakharAudioComponent::UGS_DrakharAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bDraconicFurySoundPlayed = false;
	bHurtSoundPlayed = false;
	bDashSkillSoundPlayed = false;
	FeverModeStateSoundPlayingID = AK_INVALID_PLAYING_ID;
}

void UGS_DrakharAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerDrakhar = Cast<AGS_Drakhar>(GetOwner());
	if (OwnerDrakhar)
	{
		// 베이스 클래스의 공통 죽음 사운드 포인터 설정
		DeathSound = OwnerDrakhar->DeathSoundEvent;
	}
}

void UGS_DrakharAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 모든 타이머 정리 (레벨 전환 시 크래시 방지)
	if (UWorld* World = GetWorld())
	{
		if (World->IsValidLowLevel() && !World->bIsTearingDown)
		{
			FTimerManager& TimerManager = World->GetTimerManager();

			if (DraconicFurySoundCooldownTimer.IsValid())
			{
				TimerManager.ClearTimer(DraconicFurySoundCooldownTimer);
				DraconicFurySoundCooldownTimer.Invalidate();
			}

			if (HurtSoundCooldownTimer.IsValid())
			{
				TimerManager.ClearTimer(HurtSoundCooldownTimer);
				HurtSoundCooldownTimer.Invalidate();
			}

			if (DashSkillSoundCooldownTimer.IsValid())
			{
				TimerManager.ClearTimer(DashSkillSoundCooldownTimer);
				DashSkillSoundCooldownTimer.Invalidate();
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

// === 사운드 재생 함수 구현 ===
void UGS_DrakharAudioComponent::PlayComboAttackSound()
{
	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayComboAttackSound();
}

void UGS_DrakharAudioComponent::Multicast_PlayComboAttackSound_Implementation()
{
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// 소리의 주인이 로컬 플레이어인지 확인
	const bool bIsLocalPlayer = (OwnerDrakhar && OwnerDrakhar->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bIsLocalPlayer)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerDrakhar, true))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	PlaySoundEvent(OwnerDrakhar->ComboAttackSoundEvent, OwnerDrakhar->GetActorLocation());
}

void UGS_DrakharAudioComponent::PlayDashSkillSound()
{
	if (bDashSkillSoundPlayed || !ValidateServerRPCCall())
	{
		return;
	}

	// 서버에서도 쿨다운 플래그를 즉시 설정하여 중복 RPC 송신 방지
	bDashSkillSoundPlayed = true;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(
		    DashSkillSoundCooldownTimer,
		    this,
		    &UGS_DrakharAudioComponent::ResetDashSkillSoundCooldown,
		    DashSkillSoundCooldown,
		    false);
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayDashSkillSound();
}

void UGS_DrakharAudioComponent::Multicast_PlayDashSkillSound_Implementation()
{
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// 소리의 주인이 로컬 플레이어인지 확인
	const bool bIsLocalPlayer = (OwnerDrakhar && OwnerDrakhar->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bIsLocalPlayer)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerDrakhar, true))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	if (bDashSkillSoundPlayed)
	{
		return;
	}

	PlaySoundEvent(OwnerDrakhar->DashSkillSoundEvent, OwnerDrakhar->GetActorLocation());
	bDashSkillSoundPlayed = true;

	// 클라이언트에서도 로컬 중복 재생 방지를 위해 타이머 설정
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(
		    DashSkillSoundCooldownTimer,
		    this,
		    &UGS_DrakharAudioComponent::ResetDashSkillSoundCooldown,
		    DashSkillSoundCooldown,
		    false);
	}
}

void UGS_DrakharAudioComponent::PlayEarthquakeSkillSound()
{
	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayEarthquakeSkillSound();
}

void UGS_DrakharAudioComponent::Multicast_PlayEarthquakeSkillSound_Implementation()
{
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// 소리의 주인이 로컬 플레이어인지 확인
	const bool bIsLocalPlayer = (OwnerDrakhar && OwnerDrakhar->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bIsLocalPlayer)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerDrakhar, true))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	PlaySoundEvent(OwnerDrakhar->EarthquakeSkillSoundEvent, OwnerDrakhar->GetActorLocation());
}

void UGS_DrakharAudioComponent::PlayDraconicFurySkillSound()
{
	if (bDraconicFurySoundPlayed || !ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayDraconicFurySkillSound();
}

void UGS_DrakharAudioComponent::Multicast_PlayDraconicFurySkillSound_Implementation()
{
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// 소리의 주인이 로컬 플레이어인지 확인
	const bool bIsLocalPlayer = (OwnerDrakhar && OwnerDrakhar->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bIsLocalPlayer)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerDrakhar, true))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	if (bDraconicFurySoundPlayed)
	{
		return;
	}

	PlaySoundEvent(OwnerDrakhar->DraconicFurySkillSoundEvent, OwnerDrakhar->GetActorLocation());
	bDraconicFurySoundPlayed = true;

	// 타이머 설정
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(
		    DraconicFurySoundCooldownTimer,
		    this,
		    &UGS_DrakharAudioComponent::ResetDraconicFurySoundCooldown,
		    DraconicFurySoundCooldown,
		    false);
	}
}

void UGS_DrakharAudioComponent::PlayDraconicProjectileSound(const FVector& Location)
{
	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayDraconicProjectileSound(Location);
}

void UGS_DrakharAudioComponent::Multicast_PlayDraconicProjectileSound_Implementation(const FVector& Location)
{
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// 소리의 주인이 로컬 플레이어인지 확인
	const bool bIsLocalPlayer = (OwnerDrakhar && OwnerDrakhar->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bIsLocalPlayer)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerDrakhar, true))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	PlaySoundEvent(OwnerDrakhar->DraconicProjectileSoundEvent, Location);
}

void UGS_DrakharAudioComponent::PlayAttackHitSound()
{
	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayAttackHitSound();
}

void UGS_DrakharAudioComponent::Multicast_PlayAttackHitSound_Implementation()
{
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// 소리의 주인이 로컬 플레이어인지 확인
	const bool bIsLocalPlayer = (OwnerDrakhar && OwnerDrakhar->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bIsLocalPlayer)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerDrakhar, true))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	if (!OwnerDrakhar->AttackHitSoundEvent)
	{
		return;
	}

	PlaySoundEvent(OwnerDrakhar->AttackHitSoundEvent, OwnerDrakhar->GetActorLocation());
}

void UGS_DrakharAudioComponent::PlayFeverModeStartSound(bool bForcePlay)
{
	// 컴포넌트 유효성 검증
	if (!IsValid(this))
	{
		return;
	}

	// 월드 컨텍스트 유효성 검증
	UWorld* World = GetWorld();
	if (!World || !World->IsValidLowLevel() || World->bIsTearingDown)
	{
		return;
	}

	// 오너 유효성 검증
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	// 피버모드 사운드는 중요하므로 강제 재생 옵션 제공
	if (!bForcePlay && !CanSendRPC())
	{
		return;
	}

	LastMulticastTime = World->GetTimeSeconds();
	Multicast_PlayFeverModeStartSound();
}

void UGS_DrakharAudioComponent::Multicast_PlayFeverModeStartSound_Implementation()
{
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// OwnerDrakhar 초기화 확인 (BeginPlay가 호출되지 않았을 경우 대비)
	if (!OwnerDrakhar)
	{
		OwnerDrakhar = Cast<AGS_Drakhar>(GetOwner());
		if (!OwnerDrakhar)
		{
			UE_LOG(LogTemp, Warning, TEXT("DrakharAudioComponent: OwnerDrakhar is null in Multicast_PlayFeverModeStartSound"));
			return;
		}
	}

	// 소리의 주인이 로컬 플레이어인지 확인
	const bool bIsLocalPlayer = (OwnerDrakhar && OwnerDrakhar->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bIsLocalPlayer)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerDrakhar, true))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	PlayFeverModeStartSoundLocal();
}

void UGS_DrakharAudioComponent::PlayFeverModeStartSoundLocal()
{
	if (!OwnerDrakhar || !OwnerDrakhar->FeverModeStartSoundEvent)
		return;

	PlaySoundEvent(OwnerDrakhar->FeverModeStartSoundEvent, OwnerDrakhar->GetActorLocation());
}

void UGS_DrakharAudioComponent::PlayFeverModeEndSound()
{
	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayFeverModeEndSound();
}

void UGS_DrakharAudioComponent::Multicast_PlayFeverModeEndSound_Implementation()
{
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// 소리의 주인이 로컬 플레이어인지 확인
	const bool bIsLocalPlayer = (OwnerDrakhar && OwnerDrakhar->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bIsLocalPlayer)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerDrakhar, true))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	PlayFeverModeEndSoundLocal();
}

void UGS_DrakharAudioComponent::PlayFeverModeEndSoundLocal()
{
	if (!OwnerDrakhar || !OwnerDrakhar->FeverModeEndSoundEvent)
		return;

	PlaySoundEvent(OwnerDrakhar->FeverModeEndSoundEvent, OwnerDrakhar->GetActorLocation());
}

void UGS_DrakharAudioComponent::PlayFeverModeStateSound(bool bForcePlay)
{
	// 컴포넌트 유효성 검증
	if (!IsValid(this))
	{
		return;
	}

	// 월드 컨텍스트 유효성 검증
	UWorld* World = GetWorld();
	if (!World || !World->IsValidLowLevel() || World->bIsTearingDown)
	{
		return;
	}

	// 오너 유효성 검증
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	// 피버모드 사운드는 중요하므로 강제 재생 옵션 제공
	if (!bForcePlay && !CanSendRPC())
	{
		return;
	}

	LastMulticastTime = World->GetTimeSeconds();
	Multicast_PlayFeverModeStateSound();
}

void UGS_DrakharAudioComponent::Multicast_PlayFeverModeStateSound_Implementation()
{
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// OwnerDrakhar 초기화 확인
	if (!OwnerDrakhar)
	{
		OwnerDrakhar = Cast<AGS_Drakhar>(GetOwner());
		if (!OwnerDrakhar)
		{
			UE_LOG(LogTemp, Warning, TEXT("DrakharAudioComponent: OwnerDrakhar is null in Multicast_PlayFeverModeStateSound"));
			return;
		}
	}

	// 소리의 주인이 로컬 플레이어인지 확인
	const bool bIsLocalPlayer = (OwnerDrakhar && OwnerDrakhar->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bIsLocalPlayer)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerDrakhar, true))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	PlayFeverModeStateSoundLocal();
}

void UGS_DrakharAudioComponent::PlayFeverModeStateSoundLocal()
{
	if (!OwnerDrakhar || !OwnerDrakhar->FeverModeStateSoundEvent || !IsAudioSystemValid())
		return;

	// 피버모드 스테이트 사운드 재생 및 Playing ID 저장
	FeverModeStateSoundPlayingID = UAkGameplayStatics::PostEvent(
	    OwnerDrakhar->FeverModeStateSoundEvent,
	    OwnerDrakhar,
	    0,
	    FOnAkPostEventCallback());
}

void UGS_DrakharAudioComponent::StopFeverModeStateSound()
{
	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_StopFeverModeStateSound();
}

void UGS_DrakharAudioComponent::Multicast_StopFeverModeStateSound_Implementation()
{
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// 소리의 주인이 로컬 플레이어인지 확인
	const bool bIsLocalPlayer = (OwnerDrakhar && OwnerDrakhar->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bIsLocalPlayer)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerDrakhar, true))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	StopFeverModeStateSoundLocal();
}

void UGS_DrakharAudioComponent::StopFeverModeStateSoundLocal()
{
	// Playing ID가 유효하면 FAkAudioDevice를 통해 중지
	if (FeverModeStateSoundPlayingID != AK_INVALID_PLAYING_ID)
	{

		// FAkAudioDevice를 통해 StopPlayingID 호출
		FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
		if (AudioDevice != nullptr)
		{
			AudioDevice->StopPlayingID(FeverModeStateSoundPlayingID, FeverModeStateFadeOutDuration);
			FeverModeStateSoundPlayingID = AK_INVALID_PLAYING_ID;
		}
	}
}

// 로컬 전용 Hurt 사운드 재생 (RPC 없음 - RepNotify에서 호출)
void UGS_DrakharAudioComponent::PlayHurtSoundLocal()
{
	// 통합 체크 및 Distance Scaling 설정 (보스는 항상 재생, bSkipViewFrustumCheck = true)
	if (!PrepareMulticastSound(OwnerDrakhar, true))
	{
		return;
	}

	if (bHurtSoundPlayed)
	{
		return;
	}

	PlaySoundEvent(OwnerDrakhar->HurtSoundEvent, OwnerDrakhar->GetActorLocation());
	bHurtSoundPlayed = true;

	// 타이머 설정 (PrepareMulticastSound가 World 검증을 완료했으므로 안전)
	UWorld* World = GetWorld();
	World->GetTimerManager().SetTimer(
	    HurtSoundCooldownTimer,
	    this,
	    &UGS_DrakharAudioComponent::ResetHurtSoundCooldown,
	    HurtSoundCooldown,
	    false);
}


void UGS_DrakharAudioComponent::PlayDraconicProjectileImpactSoundLocal(const FVector& ImpactLocation, bool bHitCharacter)
{
	// 컴포넌트 유효성 검증
	if (!IsValid(this))
	{
		return;
	}

	// 월드 컨텍스트 유효성 검증
	UWorld* World = GetWorld();
	if (!World || !World->IsValidLowLevel() || World->bIsTearingDown)
	{
		return;
	}

	// 통합 체크 및 Distance Scaling 설정 (보스는 항상 재생, bSkipViewFrustumCheck = true)
	if (!PrepareMulticastSound(OwnerDrakhar, true))
	{
		return;
	}

	UAkAudioEvent* SoundToPlay = bHitCharacter ? OwnerDrakhar->DraconicProjectileExplosionSoundEvent : OwnerDrakhar->DraconicProjectileImpactSoundEvent;
	if (SoundToPlay)
	{
		PlaySoundEvent(SoundToPlay, ImpactLocation);
	}
}

void UGS_DrakharAudioComponent::PlayComboFinisherSound()
{
	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayComboFinisherSound();
}

void UGS_DrakharAudioComponent::Multicast_PlayComboFinisherSound_Implementation()
{
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// 소리의 주인이 로컬 플레이어인지 확인
	const bool bIsLocalPlayer = (OwnerDrakhar && OwnerDrakhar->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bIsLocalPlayer)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerDrakhar, true))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	if (!OwnerDrakhar->ComboFinisherSoundEvent)
	{
		return;
	}

	UAkGameplayStatics::PostEvent(
	    OwnerDrakhar->ComboFinisherSoundEvent,
	    OwnerDrakhar,
	    0,
	    FOnAkPostEventCallback());
}

// === 타이머 콜백 함수 구현 ===
void UGS_DrakharAudioComponent::ResetDraconicFurySoundCooldown()
{
	if (!IsValid(this))
		return;

	bDraconicFurySoundPlayed = false;
}

void UGS_DrakharAudioComponent::ResetHurtSoundCooldown()
{
	if (!IsValid(this))
		return;

	bHurtSoundPlayed = false;
}

void UGS_DrakharAudioComponent::PlayLandingSound()
{
	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayLandingSound();
}

void UGS_DrakharAudioComponent::ResetDashSkillSoundCooldown()
{
	if (!IsValid(this))
		return;

	bDashSkillSoundPlayed = false;
}

void UGS_DrakharAudioComponent::Multicast_PlayLandingSound_Implementation()
{
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// 소리의 주인이 로컬 플레이어인지 확인
	const bool bIsLocalPlayer = (OwnerDrakhar && OwnerDrakhar->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bIsLocalPlayer)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerDrakhar, true))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	if (!OwnerDrakhar->LandingSoundEvent)
	{
		return;
	}

	PlaySoundEvent(OwnerDrakhar->LandingSoundEvent, OwnerDrakhar->GetActorLocation());
}

// === Wwise 헬퍼 함수 구현 ===
void UGS_DrakharAudioComponent::PlaySoundEvent(UAkAudioEvent* SoundEvent, const FVector& Location)
{
	if (!OwnerDrakhar || !SoundEvent)
	{
		return;
	}

	if (!IsAudioSystemValid())
	{
		return;
	}

	if (Location != FVector::ZeroVector)
	{
		UAkGameplayStatics::PostEventAtLocation(SoundEvent, Location, FRotator::ZeroRotator, GetWorld());
	}
	else
	{
		UAkComponent* AkComp = GetOrCreateAkComponent();
		if (IsValid(AkComp))
		{
			AkComp->PostAkEvent(SoundEvent);
		}
	}
}