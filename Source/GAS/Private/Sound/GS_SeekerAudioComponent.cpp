#include "Sound/GS_SeekerAudioComponent.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "AkComponent.h"
#include "AkGameplayStatics.h"
#include "AkAudioDevice.h"
#include "AkAudioEvent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "AI/RTS/GS_RTSController.h"
#include "Character/Skill/GS_SkillComp.h"
#include "UObject/UObjectGlobals.h"
#include "Character/Skill/GS_SkillSet.h"
#include "Character/GS_Character.h"
#include "Character/Player/GS_Player.h"
#include "Character/Component/GS_StatComp.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "System/Utility/GS_AssetLoader.h"


UGS_SeekerAudioComponent::UGS_SeekerAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	CurrentAudioState = ESeekerAudioState::Idle;
	PreviousAudioState = ESeekerAudioState::Idle;

	AudioConfig.MaxAudioDistance = 2000.0f;

	SkillEventID = AK_INVALID_PLAYING_ID;

	bIsComponentShuttingDown = false;
}

void UGS_SeekerAudioComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGS_SeekerAudioComponent, CurrentAudioState);
}

void UGS_SeekerAudioComponent::OnRep_CurrentAudioState()
{
	if (OwnerSeeker && GetWorld() && GetWorld()->IsNetMode(NM_Client))
	{
		UpdateSoundTimer();
	}
	PreviousAudioState = CurrentAudioState;
}

void UGS_SeekerAudioComponent::BeginPlay()
{
	Super::BeginPlay(); // Base 클래스에서 AkComponent 재초기화 처리됨

	// Owner가 GS_Character인지 확인
	OwnerCharacter = Cast<AGS_Character>(GetOwner());
	if (OwnerCharacter)
	{
		// Owner의 CharacterType을 자동으로 설정
		ECharacterType DetectedType = OwnerCharacter->GetCharacterType();
		CharacterType = DetectedType;

		// Owner가 GS_Seeker인지도 확인
		OwnerSeeker = Cast<AGS_Seeker>(GetOwner());
	}
}

void UGS_SeekerAudioComponent::PreloadSeekerAssets()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	TArray<FSoftObjectPath> AssetsToLoad;

	// 공통 사운드
	if (!AudioConfig.HurtSound.IsNull())
		AssetsToLoad.Add(AudioConfig.HurtSound.ToSoftObjectPath());
	if (!DeathSound.IsNull())
		AssetsToLoad.Add(DeathSound.ToSoftObjectPath());
	if (!SkillEvent.IsNull())
		AssetsToLoad.Add(SkillEvent.ToSoftObjectPath());

	// 캐릭터 타입별 사운드
	switch ((uint8)CharacterType)
	{
	case (uint8)ECharacterType::Chan:
		if (!ShieldSlamStartSound.IsNull())
			AssetsToLoad.Add(ShieldSlamStartSound.ToSoftObjectPath());
		if (!ShieldSlamImpactSound.IsNull())
			AssetsToLoad.Add(ShieldSlamImpactSound.ToSoftObjectPath());
		if (!ChanAxeSwingSound.IsNull())
			AssetsToLoad.Add(ChanAxeSwingSound.ToSoftObjectPath());
		if (!ChanAxeSwingStopEvent.IsNull())
			AssetsToLoad.Add(ChanAxeSwingStopEvent.ToSoftObjectPath());
		if (!ChanFinalAttackExtraSound.IsNull())
			AssetsToLoad.Add(ChanFinalAttackExtraSound.ToSoftObjectPath());
		if (!ChanAttackVoiceSound.IsNull())
			AssetsToLoad.Add(ChanAttackVoiceSound.ToSoftObjectPath());
		if (!ChanDefenseSound.IsNull())
			AssetsToLoad.Add(ChanDefenseSound.ToSoftObjectPath());
		break;
	case (uint8)ECharacterType::Ares:
		if (!AresSwordSwingStopEvent.IsNull())
			AssetsToLoad.Add(AresSwordSwingStopEvent.ToSoftObjectPath());
		for (const TSoftObjectPtr<UAkAudioEvent>& Sound : AresComboSwingSounds)
			if (!Sound.IsNull())
				AssetsToLoad.Add(Sound.ToSoftObjectPath());
		for (const TSoftObjectPtr<UAkAudioEvent>& Sound : AresComboVoiceSounds)
			if (!Sound.IsNull())
				AssetsToLoad.Add(Sound.ToSoftObjectPath());
		for (const TSoftObjectPtr<UAkAudioEvent>& Sound : AresComboExtraSounds)
			if (!Sound.IsNull())
				AssetsToLoad.Add(Sound.ToSoftObjectPath());
		break;
	case (uint8)ECharacterType::Merci:
		if (!BowDrawSound.IsNull())
			AssetsToLoad.Add(BowDrawSound.ToSoftObjectPath());
		if (!BowReleaseSound.IsNull())
			AssetsToLoad.Add(BowReleaseSound.ToSoftObjectPath());
		if (!ArrowShotSound.IsNull())
			AssetsToLoad.Add(ArrowShotSound.ToSoftObjectPath());
		if (!ArrowTypeChangeSound.IsNull())
			AssetsToLoad.Add(ArrowTypeChangeSound.ToSoftObjectPath());
		if (!ArrowEmptySound.IsNull())
			AssetsToLoad.Add(ArrowEmptySound.ToSoftObjectPath());
		if (!HitFeedbackSound.IsNull())
			AssetsToLoad.Add(HitFeedbackSound.ToSoftObjectPath());
		break;
	}

	if (AssetsToLoad.Num() > 0)
	{
		TWeakObjectPtr<UGS_SeekerAudioComponent> WeakThis(this);
		UGS_AssetLoader::AsyncLoadMultipleAssets(AssetsToLoad, [WeakThis]()
		                                         {
			if (UGS_SeekerAudioComponent* Strong = WeakThis.Get())
			{
				// 로드 완료 후 캐싱 (GC 방지)
				Strong->CachedHurtSound = Strong->AudioConfig.HurtSound.Get();
				Strong->CachedSkillEvent = Strong->SkillEvent.Get();
				
				// 캐릭터별 특수 사운드 전체 캐싱
				Strong->CachedActionSounds.Empty();
				Strong->CachedComboSwingSounds.Empty();
				Strong->CachedComboVoiceSounds.Empty();
				Strong->CachedComboExtraSounds.Empty();
				
				auto CacheSound = [&](const TSoftObjectPtr<UAkAudioEvent>& SoftPtr) {
					if (SoftPtr.Get()) Strong->CachedActionSounds.Add(SoftPtr.Get());
				};

				if (Strong->CharacterType == ECharacterType::Chan)
				{
					CacheSound(Strong->ShieldSlamStartSound);
					CacheSound(Strong->ShieldSlamImpactSound);
					CacheSound(Strong->ChanAxeSwingSound);
					CacheSound(Strong->ChanAxeSwingStopEvent);
					CacheSound(Strong->ChanFinalAttackExtraSound);
					CacheSound(Strong->ChanAttackVoiceSound);
					CacheSound(Strong->ChanDefenseSound);
				}
				else if (Strong->CharacterType == ECharacterType::Ares)
				{
					CacheSound(Strong->AresSwordSwingStopEvent);
					for (auto& Sound : Strong->AresComboSwingSounds)
						if (Sound.Get()) Strong->CachedComboSwingSounds.Add(Sound.Get());
					for (auto& Sound : Strong->AresComboVoiceSounds)
						if (Sound.Get()) Strong->CachedComboVoiceSounds.Add(Sound.Get());
					for (auto& Sound : Strong->AresComboExtraSounds)
						if (Sound.Get()) Strong->CachedComboExtraSounds.Add(Sound.Get());
				}
				else if (Strong->CharacterType == ECharacterType::Merci)
				{
					CacheSound(Strong->BowDrawSound);
					CacheSound(Strong->BowReleaseSound);
					CacheSound(Strong->ArrowShotSound);
					CacheSound(Strong->ArrowTypeChangeSound);
					CacheSound(Strong->ArrowEmptySound);
					CacheSound(Strong->HitFeedbackSound);
				}
			} });
	}
}

void UGS_SeekerAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 가장 먼저 셧다운 플래그 설정 (RPC 크래시 방지)
	bIsComponentShuttingDown = true;

	// LowHP Pain 사운드 정리 (최우선)
	ForceStopLowHPPainSound();

	StopSoundTimer();

	if (AttackSoundResetTimerHandle.IsValid())
	{
		UWorld* World = GetWorld();
		if (World && World->IsValidLowLevel() && !World->bIsTearingDown)
		{
			World->GetTimerManager().ClearTimer(AttackSoundResetTimerHandle);
		}
		AttackSoundResetTimerHandle.Invalidate();
	}

	// LowHP 체크 타이머 정리
	if (LowHPCheckTimerHandle.IsValid())
	{
		UWorld* World = GetWorld();
		if (World && World->IsValidLowLevel() && !World->bIsTearingDown)
		{
			World->GetTimerManager().ClearTimer(LowHPCheckTimerHandle);
		}
		LowHPCheckTimerHandle.Invalidate();
	}

	StopAllActiveSounds();

	Super::EndPlay(EndPlayReason);
}

void UGS_SeekerAudioComponent::OnSpecificSoundFinished(AkPlayingID FinishedID)
{
	// 스킬 이벤트가 완료된 경우 SkillEventID 초기화
	if (SkillEventID == FinishedID)
	{
		SkillEventID = AK_INVALID_PLAYING_ID;
	}

	Super::OnSpecificSoundFinished(FinishedID);
}

float UGS_SeekerAudioComponent::GetMaxAudioDistance() const
{
	return AudioConfig.MaxAudioDistance;
}

void UGS_SeekerAudioComponent::SetSeekerAudioState(ESeekerAudioState NewState)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
		return;

	if (CurrentAudioState == NewState)
		return;

	PreviousAudioState = CurrentAudioState;
	CurrentAudioState = NewState;

	UpdateSoundTimer();
}

void UGS_SeekerAudioComponent::PlaySound(ESeekerAudioState SoundType, bool bForcePlay)
{
	// 컴포넌트 유효성 검증
	if (!IsValid(this))
	{
		return;
	}

	// 오너 검증
	if (!IsValid(OwnerSeeker))
	{
		return;
	}

	// 월드 컨텍스트 유효성 검증
	UWorld* World = GetWorld();
	if (!World || !World->IsValidLowLevel() || World->bIsTearingDown)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	if (!CanSendRPC())
		return;

	if (!bForcePlay)
	{
		float CurrentTime = World->GetTimeSeconds();
		if ((CurrentTime - ServerLastBroadcastTime.FindOrAdd(SoundType, 0.0f)) < 1.0f)
		{
			return;
		}
		ServerLastBroadcastTime.Emplace(SoundType, CurrentTime);
	}

	LastMulticastTime = World->GetTimeSeconds();
	Multicast_TriggerSound(SoundType, bForcePlay);
}

void UGS_SeekerAudioComponent::Multicast_TriggerSound_Implementation(ESeekerAudioState SoundTypeToTrigger, bool bIsImmediate)
{
	// 리슨 서버 중복 재생 방지
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	if (!OwnerSeeker)
	{
		return;
	}

	// 통합 체크 및 Distance Scaling 설정
	if (!PrepareMulticastSound(OwnerSeeker, false))
	{
		return;
	}

	UAkAudioEvent* SoundEvent = GetSoundEvent(SoundTypeToTrigger);
	if (!SoundEvent)
	{
		return;
	}

	if (!bIsImmediate)
	{
		UWorld* World = GetWorld();
		float CurrentTime = World->GetTimeSeconds();
		if ((CurrentTime - LocalLastSoundPlayTimes.FindOrAdd(SoundTypeToTrigger, 0.0f)) < (1.0f * LocalSoundCooldownMultiplier))
		{
			return;
		}
		LocalLastSoundPlayTimes.Emplace(SoundTypeToTrigger, CurrentTime);
	}

	AkPlayingID NewPlayingID = UAkGameplayStatics::PostEvent(SoundEvent, OwnerSeeker, 0, FOnAkPostEventCallback());
	RegisterPlayingID(NewPlayingID);
}

void UGS_SeekerAudioComponent::PlayBowDrawSound()
{
	// 메르시만 활 사운드 재생 가능
	if (CharacterType != ECharacterType::Merci || !IsValid(OwnerSeeker) || !OwnerSeeker->IsMerci())
	{
		return;
	}

	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayBowDrawSound();
}

void UGS_SeekerAudioComponent::Multicast_PlayBowDrawSound_Implementation()
{
	// 오디오 시스템 검증 (데디케이티드 서버 체크)
	if (!IsAudioSystemValid())
	{
		return;
	}

	// 소리의 주인이 로컬 플레이어인지 확인하여 거리 체크를 생략할지 결정
	const bool bShouldSkipDistanceCheck = (OwnerSeeker && OwnerSeeker->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bShouldSkipDistanceCheck)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerSeeker, false))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	// 사운드 선택
	UAkAudioEvent* SoundToPlay = UGS_AssetLoader::SyncLoadAsset(BowDrawSound);

	if (!SoundToPlay || !IsValid(OwnerSeeker))
	{
		return;
	}

	// 사운드 재생
	AkPlayingID BowPlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
	RegisterPlayingID(BowPlayingID);
}

void UGS_SeekerAudioComponent::PlayBowReleaseSound()
{
	// 메르시만 활 사운드 재생 가능
	if (CharacterType != ECharacterType::Merci || !IsValid(OwnerSeeker) || !OwnerSeeker->IsMerci())
	{
		return;
	}

	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayBowReleaseSound();
}

void UGS_SeekerAudioComponent::Multicast_PlayBowReleaseSound_Implementation()
{
	// 오디오 시스템 검증 (데디케이티드 서버 체크)
	if (!IsAudioSystemValid())
	{
		return;
	}

	// 소리의 주인이 로컬 플레이어인지 확인하여 거리 체크를 생략할지 결정
	const bool bShouldSkipDistanceCheck = (OwnerSeeker && OwnerSeeker->IsLocallyControlled());

	// 로컬 플레이어가 아닌 경우에만 거리/시야각 체크
	if (!bShouldSkipDistanceCheck)
	{
		// 통합 체크 및 Distance Scaling 설정
		if (!PrepareMulticastSound(OwnerSeeker, false))
		{
			return;
		}
	}
	else
	{
		// 로컬 플레이어이므로 거리 체크 스킵, Distance Scaling만 설정
		SetDistanceScaling(IsRTSMode());
	}

	// 사운드 선택
	UAkAudioEvent* SoundToPlay = UGS_AssetLoader::SyncLoadAsset(BowReleaseSound);

	if (!SoundToPlay || !IsValid(OwnerSeeker))
	{
		return;
	}

	// 사운드 재생
	AkPlayingID ReleasePlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
	RegisterPlayingID(ReleasePlayingID);
}


void UGS_SeekerAudioComponent::StartSoundTimer()
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

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	StopSoundTimer();

	FTimerDelegate TimerDelegate;

	switch (CurrentAudioState)
	{
	case ESeekerAudioState::Idle:
		break;

	case ESeekerAudioState::Combat:
		break;

	case ESeekerAudioState::Aiming:
		break;

	default:
		break;
	}
}

void UGS_SeekerAudioComponent::StopSoundTimer()
{
	// 월드 컨텍스트 안전성 검증
	UWorld* World = GetWorld();
	if (!World || !World->IsValidLowLevel() || World->bIsTearingDown)
	{
		return;
	}

	if (CombatSoundTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(CombatSoundTimerHandle);
	}
}

void UGS_SeekerAudioComponent::UpdateSoundTimer()
{
	StartSoundTimer();
}

UAkAudioEvent* UGS_SeekerAudioComponent::GetSoundEvent(ESeekerAudioState SoundType) const
{
	UAkAudioEvent* SoundToPlay = nullptr;

	switch ((uint8)SoundType)
	{
	case (uint8)ESeekerAudioState::Idle:
		return nullptr;
	case (uint8)ESeekerAudioState::Combat:
		return nullptr;
	case (uint8)ESeekerAudioState::Aiming:
		return nullptr;
	case (uint8)ESeekerAudioState::Hurt:
		SoundToPlay = CachedHurtSound ? CachedHurtSound.Get() : UGS_AssetLoader::SyncLoadAsset(AudioConfig.HurtSound);
		break;
	case (uint8)ESeekerAudioState::Death:
		// DeathSound는 TSoftObjectPtr이므로 SyncLoad 시도
		SoundToPlay = UGS_AssetLoader::SyncLoadAsset(DeathSound);
		break;
	default:
		return nullptr;
	}
	return SoundToPlay;
}

void UGS_SeekerAudioComponent::CheckForStateChanges()
{
	if (!OwnerSeeker || !GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
		return;

	if (CurrentAudioState == ESeekerAudioState::Death)
	{
		return;
	}

	if (OwnerSeeker->GetStatComp() && IsValid(OwnerSeeker->GetStatComp()))
	{
		if (OwnerSeeker->GetStatComp()->GetCurrentHealth() <= 0.0f)
		{
			if (CurrentAudioState != ESeekerAudioState::Death)
			{
				SetSeekerAudioState(ESeekerAudioState::Death);
			}
			return;
		}
	}

	if (CurrentAudioState == ESeekerAudioState::Hurt)
		return;

	// 시커의 상태에 따른 오디오 상태 변경
	if (OwnerSeeker->GetAimState())
	{
		if (CurrentAudioState != ESeekerAudioState::Aiming)
		{
			SetSeekerAudioState(ESeekerAudioState::Aiming);
		}
		return;
	}
}

// =============================================
// 스킬 관련 기능들
// =============================================

void UGS_SeekerAudioComponent::PlaySkill()
{
	FOnAkPostEventCallback DummyCallback;

	SkillEventID = UAkGameplayStatics::PostEvent(UGS_AssetLoader::SyncLoadAsset(SkillEvent),
	                                             GetOwner(), // Post the event to the owner of this component
	                                             0, // No callback mask
	                                             DummyCallback, // No callback
	                                             false // bStopWhenAttachedToDestroyed
	);

	// 스킬 이벤트도 ActivePlayingIDs에서 관리 (선택적 정지를 위해)
	RegisterPlayingID(SkillEventID);
}

void UGS_SeekerAudioComponent::StopSkill()
{
	// 선택적 정지: 스킬 사운드만 정지
	if (SkillEventID != AK_INVALID_PLAYING_ID)
	{
		if (FAkAudioDevice* AkDevice = FAkAudioDevice::Get())
		{
			AkDevice->StopPlayingID(SkillEventID);
		}
		SkillEventID = AK_INVALID_PLAYING_ID;
	}
}

void UGS_SeekerAudioComponent::PlaySoundAtLocation(UAkAudioEvent* SoundEvent, const FVector& Location)
{
	// 오디오 시스템 검증 (데디케이티드 서버 및 Wwise 초기화 체크)
	if (!IsAudioSystemValid())
	{
		return;
	}

	if (!SoundEvent)
	{
		return;
	}

	// 월드 컨텍스트 유효성 검사
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (Location != FVector::ZeroVector)
	{
		// PostEventAtLocation 호출 전 추가 안전성 검사
		if (IsValid(SoundEvent) && World->IsValidLowLevel())
		{
			UAkGameplayStatics::PostEventAtLocation(SoundEvent, Location, FRotator::ZeroRotator, World);
		}
	}
	else
	{
		// 위치가 Zero Vector면 Owner 위치에서 재생
		UAkComponent* AkComp = GetOrCreateAkComponent();
		if (AkComp && IsValid(AkComp) && IsValid(GetOwner()))
		{
			AkPlayingID LocationPlayingID = UAkGameplayStatics::PostEvent(SoundEvent, GetOwner(), 0, FOnAkPostEventCallback());
			RegisterPlayingID(LocationPlayingID);
		}
	}
}

const struct FSkillInfo* UGS_SeekerAudioComponent::GetSkillInfoFromDataTable(ESkillSlot SkillSlot) const
{
	// Owner가 AGS_Player인지 확인
	AGS_Player* OwnerPlayer = Cast<AGS_Player>(GetOwner());
	if (!OwnerPlayer)
	{
		return nullptr;
	}

	// SkillComp 가져오기
	UGS_SkillComp* SkillComp = OwnerPlayer->GetSkillComp();
	if (!SkillComp)
	{
		return nullptr;
	}

	// 데이터 테이블 가져오기
	UDataTable* SkillDataTable = SkillComp->GetSkillDataTable();
	if (!SkillDataTable)
	{
		return nullptr;
	}

	// 캐릭터 타입을 기반으로 RowName 구하기
	FString CharTypeString = UEnum::GetValueAsString(OwnerPlayer->GetCharacterType());
	int32 SeparatorIndex;
	if (CharTypeString.FindChar(TEXT(':'), SeparatorIndex))
	{
		CharTypeString = CharTypeString.RightChop(SeparatorIndex + 2);
	}
	FName RowName = FName(*CharTypeString);

	// 스킬셋 찾기
	FString Context;
	FGS_SkillSet* SkillSet = SkillDataTable->FindRow<FGS_SkillSet>(RowName, Context);
	if (!SkillSet)
	{
		return nullptr;
	}

	// 스킬 슬롯에 따라 적절한 스킬 정보 반환
	switch (SkillSlot)
	{
	case ESkillSlot::Ready:
		return &SkillSet->ReadySkill;
	case ESkillSlot::Aiming:
		return &SkillSet->AimingSkill;
	case ESkillSlot::Moving:
		return &SkillSet->MovingSkill;
	case ESkillSlot::Ultimate:
		return &SkillSet->UltimateSkill;
	case ESkillSlot::Rolling:
		return &SkillSet->RollingSkill;
	case ESkillSlot::HealPotion:
		return &SkillSet->HealPotionSkill;
	default:
		return nullptr;
	}
}

void UGS_SeekerAudioComponent::PlaySkillSoundFromDataTable(ESkillSlot SkillSlot, bool bIsSkillStart)
{
	const FSkillInfo* SkillInfo = GetSkillInfoFromDataTable(SkillSlot);
	if (!SkillInfo)
	{
		return;
	}

	UAkAudioEvent* StartSound = UGS_AssetLoader::SyncLoadAsset(SkillInfo->SkillStartSound);
	UAkAudioEvent* EndSound = UGS_AssetLoader::SyncLoadAsset(SkillInfo->SkillEndSound);

	// 스킬 시작/종료 사운드 재생 (모드별 선택)
	PlaySkillSoundFromSkillInfo(bIsSkillStart, StartSound, EndSound);
}

void UGS_SeekerAudioComponent::PlaySkillLoopSoundFromDataTable(ESkillSlot SkillSlot)
{
	const FSkillInfo* SkillInfo = GetSkillInfoFromDataTable(SkillSlot);
	if (!SkillInfo || !SkillInfo->SkillLoopSound)
	{
		return;
	}

	UAkAudioEvent* LoopSound = UGS_AssetLoader::SyncLoadAsset(SkillInfo->SkillLoopSound);

	// 루프 사운드 재생 (모드별 선택)
	AkPlayingID LoopPlayingID = UAkGameplayStatics::PostEvent(LoopSound, GetOwner(), 0, FOnAkPostEventCallback());
	RegisterPlayingID(LoopPlayingID);
}

void UGS_SeekerAudioComponent::StopSkillLoopSoundFromDataTable(ESkillSlot SkillSlot)
{
	const FSkillInfo* SkillInfo = GetSkillInfoFromDataTable(SkillSlot);
	if (!SkillInfo || !SkillInfo->SkillLoopStopSound)
	{
		return;
	}

	UAkAudioEvent* LoopStopSound = UGS_AssetLoader::SyncLoadAsset(SkillInfo->SkillLoopStopSound);

	// 루프 사운드 정지 (모드별 선택)
	AkPlayingID StopPlayingID = UAkGameplayStatics::PostEvent(LoopStopSound, GetOwner(), 0, FOnAkPostEventCallback());
	RegisterPlayingID(StopPlayingID);
}

void UGS_SeekerAudioComponent::PlaySkillCollisionSoundFromDataTable(ESkillSlot SkillSlot, uint8 CollisionType)
{
	const FSkillInfo* SkillInfo = GetSkillInfoFromDataTable(SkillSlot);
	if (!SkillInfo)
	{
		return;
	}

	// 충돌 타입에 따른 사운드 선택
	UAkAudioEvent* CollisionSound = nullptr;
	switch (CollisionType)
	{
	case 0: // 벽 충돌
		CollisionSound = UGS_AssetLoader::SyncLoadAsset(SkillInfo->WallCollisionSound);
		break;
	case 1: // 몬스터 충돌
		CollisionSound = UGS_AssetLoader::SyncLoadAsset(SkillInfo->MonsterCollisionSound);
		break;
	case 2: // 가디언 충돌
		CollisionSound = UGS_AssetLoader::SyncLoadAsset(SkillInfo->GuardianCollisionSound);
		break;
	default:
		return;
	}

	if (CollisionSound)
	{
		AkPlayingID CollisionPlayingID = UAkGameplayStatics::PostEvent(CollisionSound, GetOwner(), 0, FOnAkPostEventCallback());
		RegisterPlayingID(CollisionPlayingID);
	}
}

void UGS_SeekerAudioComponent::RequestSkillAudio(ESkillSlot SkillSlot, int32 AudioEventType, FVector Location)
{
	// 셧다운 중이면 RPC 호출 금지 (레벨 전환/액터 파괴 시 크래시 방지)
	if (bIsComponentShuttingDown)
	{
		return;
	}

	// 컴포넌트 유효성 검증 (RPC 호출 전 필수)
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

	// 오너 액터 유효성 검증
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	// 액터 파괴 상태 확인 (가비지 컬렉션 대상 객체에서 RPC 호출 방지)
	if (Owner->IsActorBeingDestroyed())
	{
		return;
	}

	// 네트워크 복제가 비활성화된 경우 RPC 호출 금지
	if (!Owner->GetIsReplicated())
	{
		return;
	}

	// 서버 권한에서 멀티캐스트로 전체 동기화
	if (CanSendRPC())
	{
		Multicast_RequestSkillAudio(SkillSlot, AudioEventType, Location);
		return;
	}

	// 로컬에서도 즉시 재생 (서버가 아닌 경우 시각적/청각적 반응용)
	Multicast_RequestSkillAudio(SkillSlot, AudioEventType, Location);
}

void UGS_SeekerAudioComponent::Multicast_RequestSkillAudio_Implementation(ESkillSlot SkillSlot, int32 AudioEventType, FVector Location)
{
	// 셧다운 중이면 RPC 처리 거부 (레벨 전환/액터 파괴 시 크래시 방지)
	if (bIsComponentShuttingDown)
	{
		return;
	}

	// 컴포넌트 유효성 검증 (RPC 수신 시 가장 먼저 체크)
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

	// 오너 액터 유효성 검증 및 파괴 상태 확인
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	// 액터 파괴 중이거나 가비지 컬렉션 대상인 경우 RPC 처리 중단
	if (Owner->IsActorBeingDestroyed() || Owner->IsUnreachable())
	{
		return;
	}

	// 오디오 시스템 검증 (데디케이티드 서버 및 Wwise 초기화 체크)
	if (!IsAudioSystemValid())
	{
		return;
	}

	// 리슨 서버 중복 재생 방지
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// Event-Driven 방식으로 적절한 함수 호출 (RTS/TPS 모드 내장 분기 포함)
	switch (AudioEventType)
	{
	case 0: // 스킬 시작
		PlaySkillSoundFromDataTable(SkillSlot, true);
		break;
	case 1: // 스킬 종료
		PlaySkillSoundFromDataTable(SkillSlot, false);
		break;
	case 2: // 루프 시작 (궁극기)
		PlaySkillLoopSoundFromDataTable(SkillSlot);
		break;
	case 3: // 루프 정지 (궁극기)
		StopSkillLoopSoundFromDataTable(SkillSlot);
		break;
	default:
		// 충돌 사운드 (4=벽, 5=몬스터, 6=가디언)
		if (AudioEventType >= 4 && AudioEventType <= 6)
		{
			PlaySkillCollisionSoundFromDataTable(SkillSlot, AudioEventType - 4);
		}
		break;
	}
}

void UGS_SeekerAudioComponent::PlaySkillSoundFromSkillInfo(bool bIsSkillStart, UAkAudioEvent* SkillStartSound, UAkAudioEvent* SkillEndSound)
{
	if (!PrepareMulticastSound(GetOwner(), false))
	{
		return;
	}

	UAkAudioEvent* SoundToPlay = bIsSkillStart ? SkillStartSound : SkillEndSound;
	if (SoundToPlay)
	{
		AkPlayingID SkillPlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, GetOwner(), 0, FOnAkPostEventCallback());
		RegisterPlayingID(SkillPlayingID);
	}
}

void UGS_SeekerAudioComponent::PlayComboAttackSound(UAkAudioEvent* SwingSound, UAkAudioEvent* VoiceSound, UAkAudioEvent* StopEvent, float ResetTime)
{
	if (!PrepareMulticastSound(GetOwner(), false))
	{
		return;
	}

	// 콤보 공격 사운드 구현
	if (SwingSound)
	{
		AkPlayingID SwingPlayingID = UAkGameplayStatics::PostEvent(SwingSound, GetOwner(), 0, FOnAkPostEventCallback());
		RegisterPlayingID(SwingPlayingID);
	}

	if (VoiceSound)
	{
		AkPlayingID VoicePlayingID = UAkGameplayStatics::PostEvent(VoiceSound, GetOwner(), 0, FOnAkPostEventCallback());
		RegisterPlayingID(VoicePlayingID);
	}

	CurrentStopEvent = StopEvent;
	if (ResetTime > 0.0f && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(AttackSoundResetTimerHandle, this, &UGS_SeekerAudioComponent::ResetAttackSoundSequence, ResetTime, false);
	}
}

void UGS_SeekerAudioComponent::PlayComboAttackSoundByIndex(int32 ComboIndex, const TArray<UAkAudioEvent*>& SwingSounds, const TArray<UAkAudioEvent*>& VoiceSounds, UAkAudioEvent* StopEvent, float ResetTime)
{
	if (!PrepareMulticastSound(GetOwner(), false))
	{
		return;
	}

	if (SwingSounds.IsValidIndex(ComboIndex))
	{
		AkPlayingID SwingPlayingID = UAkGameplayStatics::PostEvent(SwingSounds[ComboIndex], GetOwner(), 0, FOnAkPostEventCallback());
		RegisterPlayingID(SwingPlayingID);
	}

	if (VoiceSounds.IsValidIndex(ComboIndex))
	{
		AkPlayingID VoicePlayingID = UAkGameplayStatics::PostEvent(VoiceSounds[ComboIndex], GetOwner(), 0, FOnAkPostEventCallback());
		RegisterPlayingID(VoicePlayingID);
	}

	CurrentStopEvent = StopEvent;
	if (ResetTime > 0.0f && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(AttackSoundResetTimerHandle, this, &UGS_SeekerAudioComponent::ResetAttackSoundSequence, ResetTime, false);
	}
}

void UGS_SeekerAudioComponent::PlayComboAttackSoundByIndexWithExtra(int32 ComboIndex, const TArray<UAkAudioEvent*>& SwingSounds, const TArray<UAkAudioEvent*>& VoiceSounds, const TArray<UAkAudioEvent*>& ExtraSounds, UAkAudioEvent* StopEvent, float ResetTime)
{
	PlayComboAttackSoundByIndex(ComboIndex, SwingSounds, VoiceSounds, StopEvent, ResetTime);

	if (ExtraSounds.IsValidIndex(ComboIndex))
	{
		AkPlayingID ExtraPlayingID = UAkGameplayStatics::PostEvent(ExtraSounds[ComboIndex], GetOwner(), 0, FOnAkPostEventCallback());
		RegisterPlayingID(ExtraPlayingID);
	}
}

void UGS_SeekerAudioComponent::PlayFinalAttackSound(UAkAudioEvent* ExtraSound)
{
	if (!PrepareMulticastSound(GetOwner(), false))
	{
		return;
	}

	if (ExtraSound)
	{
		AkPlayingID FinalPlayingID = UAkGameplayStatics::PostEvent(ExtraSound, GetOwner(), 0, FOnAkPostEventCallback());
		RegisterPlayingID(FinalPlayingID);
	}
}

void UGS_SeekerAudioComponent::PlayGenericSound(UAkAudioEvent* SoundToPlay, bool bPlayOnLocalOnly)
{
	// 오디오 시스템 검증 (데디케이티드 서버 및 Wwise 초기화 체크)
	if (!IsAudioSystemValid())
	{
		return;
	}

	if (!SoundToPlay || !IsValid(GetOwner()))
	{
		return;
	}

	AkPlayingID GenericPlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, GetOwner(), 0, FOnAkPostEventCallback());
	RegisterPlayingID(GenericPlayingID);
}

void UGS_SeekerAudioComponent::ResetAttackSoundSequence()
{
	// 타이머 콜백이므로 유효성 검증 필수
	if (!IsValid(this))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !World->IsValidLowLevel() || World->bIsTearingDown)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	if (CurrentStopEvent)
	{
		AkPlayingID StopPlayingID = UAkGameplayStatics::PostEvent(CurrentStopEvent, Owner, 0, FOnAkPostEventCallback());
		RegisterPlayingID(StopPlayingID);
		CurrentStopEvent = nullptr;
	}
}

// ===================
// 화살 관련 함수 구현
// ===================

void UGS_SeekerAudioComponent::PlayArrowShotSound()
{
	// 메르시만 화살 사운드 재생 가능
	if (!OwnerSeeker || !OwnerSeeker->IsMerci())
	{
		return;
	}

	// 이 함수는 메르시의 멀티캐스트 RPC에서 호출되므로 직접 재생
	// 근접 캐릭터(아레스, 찬)는 이 함수를 사용하지 않음
	Multicast_PlayArrowShotSound_Implementation();
}

// ==============================
// 찬 전용 방패 슬램 사운드 함수 구현
// ==============================

void UGS_SeekerAudioComponent::PlayShieldSlamStartSound()
{
	// 찬만 방패 슬램 사운드 재생 가능
	if (CharacterType != ECharacterType::Chan || !IsValid(OwnerSeeker) || !OwnerSeeker->IsChan())
	{
		return;
	}

	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayShieldSlamStartSound();
}

void UGS_SeekerAudioComponent::PlayShieldSlamImpactSound()
{
	// 찬만 방패 슬램 사운드 재생 가능
	if (!IsValid(OwnerSeeker) || !OwnerSeeker->IsChan())
	{
		return;
	}

	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayShieldSlamImpactSound();
}

// ==============================
// 찬 전용 콤보 공격 사운드 함수 구현
// ==============================

void UGS_SeekerAudioComponent::PlayChanComboAttackSound(int32 ComboIndex)
{
	// 찬만 찬 콤보 공격 사운드 재생 가능
	if (CharacterType != ECharacterType::Chan || !IsValid(OwnerSeeker) || !OwnerSeeker->IsChan())
	{
		return;
	}

	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayChanComboAttackSound(ComboIndex);
}

void UGS_SeekerAudioComponent::PlayChanFinalAttackSound()
{
	// 찬만 찬 최종 공격 사운드 재생 가능
	if (!OwnerSeeker || !OwnerSeeker->IsChan())
	{
		return;
	}

	UAkAudioEvent* FinalSound = ChanFinalAttackExtraSound.Get();
	if (!OwnerSeeker || !FinalSound)
	{
		return;
	}

	// 찬 전용 최종 공격 추가 사운드 재생
	AkPlayingID FinalPlayingID = UAkGameplayStatics::PostEvent(FinalSound, OwnerSeeker, 0, FOnAkPostEventCallback());
	RegisterPlayingID(FinalPlayingID);
}

void UGS_SeekerAudioComponent::PlayDefenseSound()
{
	// 찬만 방어 사운드 재생 가능
	if (!OwnerSeeker || !OwnerSeeker->IsChan())
	{
		return;
	}

	// 서버 권한 체크 및 멀티캐스트 호출
	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayDefenseSound();
}

void UGS_SeekerAudioComponent::Multicast_PlayDefenseSound_Implementation()
{
	// 리슨 서버 중복 재생 방지 및 통합 체크
	if (ShouldSkipListenServerRPC() || !PrepareMulticastSound(OwnerSeeker, false))
	{
		return;
	}

	// 찬만 방어 사운드 재생 가능
	if (!OwnerSeeker || !OwnerSeeker->IsChan())
	{
		return;
	}

	// 통합 사운드 에셋 사용 (동적 스케일링 적용)
	UAkAudioEvent* DefenseSoundToPlay = ChanDefenseSound.Get();

	if (DefenseSoundToPlay)
	{
		// 방어 사운드 재생
		AkPlayingID DefensePlayingID = UAkGameplayStatics::PostEvent(DefenseSoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(DefensePlayingID);
	}
}

// ==============================
// 아레스 전용 콤보 공격 사운드 함수 구현
// ==============================

void UGS_SeekerAudioComponent::PlayAresComboAttackSound(int32 ComboIndex)
{
	// 아레스만 아레스 콤보 공격 사운드 재생 가능
	if (CharacterType != ECharacterType::Ares || !IsValid(OwnerSeeker) || !OwnerSeeker->IsAres())
	{
		return;
	}

	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayAresComboAttackSound(ComboIndex);
}

void UGS_SeekerAudioComponent::PlayAresComboAttackSoundWithExtra(int32 ComboIndex)
{
	// 기본 콤보 공격 사운드 재생
	PlayAresComboAttackSound(ComboIndex);

	if (!ValidateServerRPCCall())
	{
		return;
	}

	LastMulticastTime = GetWorld()->GetTimeSeconds();
	Multicast_PlayAresComboAttackSoundWithExtra(ComboIndex);
}

// ==============================
// TPS 콤보 사운드 멀티캐스트 RPC 구현
// ==============================

void UGS_SeekerAudioComponent::Multicast_PlayChanComboAttackSound_Implementation(int32 ComboIndex)
{
	// 리슨 서버 중복 재생 방지 및 통합 체크
	if (ShouldSkipListenServerRPC() || !PrepareMulticastSound(OwnerSeeker, false))
	{
		return;
	}

	// 통합 사운드 에셋 사용 (동적 스케일링 적용)
	UAkAudioEvent* SwingSoundToPlay = CachedActionSounds.Contains(ChanAxeSwingSound.Get()) ? ChanAxeSwingSound.Get() : UGS_AssetLoader::SyncLoadAsset(ChanAxeSwingSound);
	UAkAudioEvent* VoiceSoundToPlay = CachedActionSounds.Contains(ChanAttackVoiceSound.Get()) ? ChanAttackVoiceSound.Get() : UGS_AssetLoader::SyncLoadAsset(ChanAttackVoiceSound);

	if (SwingSoundToPlay)
	{
		AkPlayingID SwingPlayingID = UAkGameplayStatics::PostEvent(SwingSoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(SwingPlayingID);
	}

	if (VoiceSoundToPlay)
	{
		AkPlayingID VoicePlayingID = UAkGameplayStatics::PostEvent(VoiceSoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(VoicePlayingID);
	}

	// 콤보 정지 사운드 설정 (TPS 모드에서만)
	const bool bIsRTS = IsRTSMode();
	UAkAudioEvent* StopEvent = ChanAxeSwingStopEvent.Get();
	if (!bIsRTS && StopEvent)
	{
		CurrentStopEvent = StopEvent;
		UWorld* World = GetWorld();
		if (World)
		{
			World->GetTimerManager().SetTimer(AttackSoundResetTimerHandle, this, &UGS_SeekerAudioComponent::ResetAttackSoundSequence, 1.0f, false);
		}
	}
}

void UGS_SeekerAudioComponent::Multicast_PlayAresComboAttackSound_Implementation(int32 ComboIndex)
{
	// 리슨 서버 중복 재생 방지 및 통합 체크
	if (ShouldSkipListenServerRPC() || !PrepareMulticastSound(OwnerSeeker, false))
	{
		return;
	}

	// 콤보 인덱스 검증 및 변환
	int32 ArrayIndex = ValidateAndConvertComboIndex(ComboIndex, AresComboSwingSounds.Num());
	if (ArrayIndex == INDEX_NONE)
	{
		return;
	}

	// 로드된 포인터로 변환 (TPS/RTS 통합 로직)
	static constexpr float ComboResetDelay = 1.0f;
	TArray<UAkAudioEvent*> LoadedSwingSounds;
	TArray<UAkAudioEvent*> LoadedVoiceSounds;

	for (const TSoftObjectPtr<UAkAudioEvent>& SoftPtr : AresComboSwingSounds)
	{
		if (UAkAudioEvent* Event = SoftPtr.Get())
		{
			LoadedSwingSounds.Add(Event);
		}
	}
	for (const TSoftObjectPtr<UAkAudioEvent>& SoftPtr : AresComboVoiceSounds)
	{
		if (UAkAudioEvent* Event = SoftPtr.Get())
		{
			LoadedVoiceSounds.Add(Event);
		}
	}

	UAkAudioEvent* StopEvent = IsRTSMode() ? nullptr : AresSwordSwingStopEvent.Get();
	PlayComboAttackSoundByIndex(ArrayIndex, LoadedSwingSounds, LoadedVoiceSounds, StopEvent, IsRTSMode() ? 0.0f : ComboResetDelay);
}

void UGS_SeekerAudioComponent::Multicast_PlayAresComboAttackSoundWithExtra_Implementation(int32 ComboIndex)
{
	// 리슨 서버 중복 재생 방지 및 통합 체크
	if (ShouldSkipListenServerRPC() || !PrepareMulticastSound(OwnerSeeker, false))
	{
		return;
	}

	// 추가 사운드 재생 (통합 에셋 사용)
	int32 ArrayIndex = ComboIndex - 1; // 1-based에서 0-based로 변환

	if (AresComboExtraSounds.IsValidIndex(ArrayIndex))
	{
		if (UAkAudioEvent* ExtraEvent = AresComboExtraSounds[ArrayIndex].Get())
		{
			AkPlayingID ExtraPlayingID = UAkGameplayStatics::PostEvent(ExtraEvent, OwnerSeeker, 0, FOnAkPostEventCallback());
			RegisterPlayingID(ExtraPlayingID);
		}
	}
}

void UGS_SeekerAudioComponent::PlayArrowTypeChangeSound()
{
	// 메르시만 화살 사운드 재생 가능
	if (!OwnerSeeker || !OwnerSeeker->IsMerci())
	{
		return;
	}

	if (!ArrowTypeChangeSound)
	{
		return;
	}

	UAkAudioEvent* SoundToPlay = ArrowTypeChangeSound.Get();
	if (!SoundToPlay)
	{
		SoundToPlay = UGS_AssetLoader::SyncLoadAsset(ArrowTypeChangeSound);
	}

	if (SoundToPlay)
	{
		AkPlayingID ChangePlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(ChangePlayingID);
	}
}

void UGS_SeekerAudioComponent::PlayArrowEmptySound()
{
	// 메르시만 화살 사운드 재생 가능
	if (!OwnerSeeker || !OwnerSeeker->IsMerci())
	{
		return;
	}

	if (!ArrowEmptySound)
	{
		return;
	}

	UAkAudioEvent* SoundToPlay = ArrowEmptySound.Get();
	if (!SoundToPlay)
	{
		SoundToPlay = UGS_AssetLoader::SyncLoadAsset(ArrowEmptySound);
	}

	if (SoundToPlay)
	{
		AkPlayingID EmptyPlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(EmptyPlayingID);
	}
}

void UGS_SeekerAudioComponent::PlayHitFeedbackSound()
{
	// 메르시만 타격 피드백 사운드 재생 가능
	if (!OwnerSeeker || !OwnerSeeker->IsMerci())
	{
		return;
	}

	if (!HitFeedbackSound)
	{
		return;
	}

	UAkAudioEvent* SoundToPlay = HitFeedbackSound.Get();
	if (!SoundToPlay)
	{
		SoundToPlay = UGS_AssetLoader::SyncLoadAsset(HitFeedbackSound);
	}

	if (SoundToPlay)
	{
		AkPlayingID HitPlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(HitPlayingID);
	}
}

// ===================
// 화살 관련 멀티캐스트 RPC 구현
// ===================

void UGS_SeekerAudioComponent::Multicast_PlayArrowShotSound_Implementation()
{
	// 컴포넌트 유효성 검증 (RPC 수신 시 가장 먼저 체크)
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

	// 리슨 서버 중복 재생 방지
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// 통합 체크 및 Distance Scaling 설정
	if (!PrepareMulticastSound(OwnerSeeker, false))
	{
		return;
	}

	// 통합 에셋 사용 및 재생
	UAkAudioEvent* SoundToPlay = UGS_AssetLoader::SyncLoadAsset(ArrowShotSound);
	if (SoundToPlay)
	{
		AkPlayingID ArrowShotPlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(ArrowShotPlayingID);
	}
}

// ===================
// 방패 슬램 사운드 멀티캐스트 RPC 구현
// ===================

void UGS_SeekerAudioComponent::Multicast_PlayShieldSlamStartSound_Implementation()
{
	// 컴포넌트 유효성 검증 (RPC 수신 시 가장 먼저 체크)
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

	// 리슨 서버 중복 재생 방지
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// 통합 체크 및 Distance Scaling 설정
	if (!PrepareMulticastSound(OwnerSeeker, false))
	{
		return;
	}

	// 통합 에셋 사용
	UAkAudioEvent* SoundToPlay = UGS_AssetLoader::SyncLoadAsset(ShieldSlamStartSound);
	if (SoundToPlay)
	{
		AkPlayingID ShieldSlamPlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(ShieldSlamPlayingID);
	}
}

void UGS_SeekerAudioComponent::Multicast_PlayShieldSlamImpactSound_Implementation()
{
	// 컴포넌트 유효성 검증 (RPC 수신 시 가장 먼저 체크)
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

	// 리슨 서버 중복 재생 방지
	if (ShouldSkipListenServerRPC())
	{
		return;
	}

	// 통합 체크 및 Distance Scaling 설정
	if (!PrepareMulticastSound(OwnerSeeker, false))
	{
		return;
	}

	// 통합 에셋 사용
	UAkAudioEvent* SoundToPlay = UGS_AssetLoader::SyncLoadAsset(ShieldSlamImpactSound);
	if (SoundToPlay)
	{
		AkPlayingID ImpactPlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(ImpactPlayingID);
	}
}

// 로컬 전용 Hurt 사운드 재생 (RPC 없음 - RepNotify에서 호출)
void UGS_SeekerAudioComponent::PlayHurtSoundLocal()
{
	// 통합 체크 및 Distance Scaling 설정 (피격 사운드는 ViewFrustum 체크 제외, bSkipViewFrustumCheck = true)
	if (!PrepareMulticastSound(OwnerSeeker, true))
	{
		return;
	}

	// 1. 쿨다운 체크
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		float CurrentTime = World->GetTimeSeconds();
		if (CurrentTime - LastHurtSoundPlayTime < AudioConfig.HurtSoundCooldown)
		{
			return;
		}
	}

	// 2. 재생 확률 체크
	if (AudioConfig.HurtSoundProbability < 1.0f)
	{
		if (FMath::FRand() > AudioConfig.HurtSoundProbability)
		{
			return;
		}
	}

	// 통합 에셋 사용 (폴백 포함)
	UAkAudioEvent* SoundToPlay = UGS_AssetLoader::SyncLoadAsset(AudioConfig.HurtSound);
	if (!SoundToPlay)
	{
		return;
	}

	// 거리 기반 RTPC 설정
	FVector ListenerLocation;
	if (GetListenerLocation(ListenerLocation))
	{
		const bool bRTS = IsRTSMode();
		const float DistanceToListener = FVector::Dist(OwnerSeeker->GetActorLocation(), ListenerLocation);
		const float RTPCMaxDistance = GetMaxAudioDistance();
		const float EffectiveMaxDistance = bRTS ? FMath::Max(RTPCMaxDistance, 10000.0f) : RTPCMaxDistance;
		const float DistanceRatio = FMath::Clamp(DistanceToListener / EffectiveMaxDistance, 0.0f, 1.0f);
		const float NormalizedDistance = 1.0f - DistanceRatio;
		SetUnifiedRTPCValue(DistanceToPlayerRTPC, NormalizedDistance);
	}

	// 사운드 재생
	AkPlayingID HurtPlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
	if (HurtPlayingID != AK_INVALID_PLAYING_ID)
	{
		RegisterPlayingID(HurtPlayingID);

		if (UWorld* World = GetWorld())
		{
			LastHurtSoundPlayTime = World->GetTimeSeconds();
		}
	}
}


// ===================
// RTS 사운드 재생 함수들 구현
// ===================

void UGS_SeekerAudioComponent::PlayRTSAresSwordSwingSound(int32 ComboIndex)
{
	if (!OwnerSeeker || !OwnerSeeker->IsAres())
	{
		return;
	}

	// RTS 모드에서만 재생
	if (!IsRTSMode())
	{
		return;
	}

	if (!PrepareMulticastSound(OwnerSeeker, false))
	{
		return;
	}

	// 배열 인덱스 체크 (0-based)
	int32 ArrayIndex = ComboIndex - 1; // 1-based에서 0-based로 변환
	if (!AresComboSwingSounds.IsValidIndex(ArrayIndex) || !AresComboSwingSounds[ArrayIndex].IsValid())
	{
		// 유효하지 않으면 동적 로딩 시도 (캐시 확인)
		if (AresComboSwingSounds.IsValidIndex(ArrayIndex) && AresComboSwingSounds[ArrayIndex].IsNull())
		{
			return;
		}
	}

	// RTS 모드에 따른 Distance Scaling 설정
	SetDistanceScaling(true);

	UAkAudioEvent* SoundToPlay = UGS_AssetLoader::SyncLoadAsset(AresComboSwingSounds[ArrayIndex]);
	if (SoundToPlay)
	{
		AkPlayingID RTSSwordPlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(RTSSwordPlayingID);
	}
}

void UGS_SeekerAudioComponent::PlayRTSAresComboVoiceSound(int32 ComboIndex)
{
	if (!OwnerSeeker || !OwnerSeeker->IsAres())
	{
		return;
	}

	// RTS 모드에서만 재생
	if (!IsRTSMode())
	{
		return;
	}

	// 거리 및 시야각 체크
	FVector ListenerLocation;
	if (!GetListenerLocation(ListenerLocation))
	{
		return;
	}

	if (!IsInViewFrustum(OwnerSeeker->GetActorLocation()))
	{
		return;
	}

	// 배열 인덱스 체크 (0-based)
	int32 ArrayIndex = ComboIndex - 1; // 1-based에서 0-based로 변환
	if (!AresComboVoiceSounds.IsValidIndex(ArrayIndex))
	{
		return;
	}

	// RTS 모드에 따른 Distance Scaling 설정
	SetDistanceScaling(true);

	UAkAudioEvent* SoundToPlay = UGS_AssetLoader::SyncLoadAsset(AresComboVoiceSounds[ArrayIndex]);
	if (SoundToPlay)
	{
		AkPlayingID RTSVoicePlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(RTSVoicePlayingID);
	}
}

void UGS_SeekerAudioComponent::PlayRTSAresComboExtraSound(int32 ComboIndex)
{
	if (!OwnerSeeker || !OwnerSeeker->IsAres())
	{
		return;
	}

	// RTS 모드에서만 재생
	if (!IsRTSMode())
	{
		return;
	}

	// 거리 및 시야각 체크
	FVector ListenerLocation;
	if (!GetListenerLocation(ListenerLocation))
	{
		return;
	}

	if (!IsInViewFrustum(OwnerSeeker->GetActorLocation()))
	{
		return;
	}

	// 배열 인덱스 체크 (0-based)
	int32 ArrayIndex = ComboIndex - 1; // 1-based에서 0-based로 변환
	if (!AresComboExtraSounds.IsValidIndex(ArrayIndex))
	{
		return;
	}

	// RTS 모드에 따른 Distance Scaling 설정
	SetDistanceScaling(true);

	UAkAudioEvent* SoundToPlay = UGS_AssetLoader::SyncLoadAsset(AresComboExtraSounds[ArrayIndex]);
	if (SoundToPlay)
	{
		AkPlayingID RTSExtraPlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(RTSExtraPlayingID);
	}
}

// ===================
// RTS 콤보 사운드 통합 함수들 구현
// ===================


void UGS_SeekerAudioComponent::PlayRTSAresComboAttackSound(int32 ComboIndex)
{
	// 아레스만 아레스 RTS 콤보 공격 사운드 재생 가능
	if (CharacterType != ECharacterType::Ares)
	{
		return;
	}

	if (!OwnerSeeker || !OwnerSeeker->IsAres())
	{
		return;
	}

	// RTS 모드에서만 재생
	if (!IsRTSMode())
	{
		return;
	}

	// 오디오 시스템 검증 (데디케이티드 서버 및 Wwise 초기화 체크)
	if (!IsAudioSystemValid())
	{
		return;
	}

	if (!OwnerSeeker || !GetWorld())
	{
		return;
	}

	// 아레스 전용 통합 콤보 공격 사운드 재생
	int32 ArrayIndex = ComboIndex - 1; // 1-based에서 0-based로 변환

	if (AresComboSwingSounds.IsValidIndex(ArrayIndex))
	{
		UAkAudioEvent* SwingSound = UGS_AssetLoader::SyncLoadAsset(AresComboSwingSounds[ArrayIndex]);
		if (SwingSound)
		{
			AkPlayingID SwingPlayingID = UAkGameplayStatics::PostEvent(SwingSound, OwnerSeeker, 0, FOnAkPostEventCallback());
			RegisterPlayingID(SwingPlayingID);
		}
	}

	if (AresComboVoiceSounds.IsValidIndex(ArrayIndex))
	{
		UAkAudioEvent* VoiceSound = UGS_AssetLoader::SyncLoadAsset(AresComboVoiceSounds[ArrayIndex]);
		if (VoiceSound)
		{
			AkPlayingID VoicePlayingID = UAkGameplayStatics::PostEvent(VoiceSound, OwnerSeeker, 0, FOnAkPostEventCallback());
			RegisterPlayingID(VoicePlayingID);
		}
	}
}

void UGS_SeekerAudioComponent::PlayRTSAresComboAttackSoundWithExtra(int32 ComboIndex)
{
	// 기본 RTS 콤보 공격 사운드 재생
	PlayRTSAresComboAttackSound(ComboIndex);

	// 추가 사운드 재생
	int32 ArrayIndex = ComboIndex - 1; // 1-based에서 0-based로 변환
	if (AresComboExtraSounds.IsValidIndex(ArrayIndex))
	{
		UAkAudioEvent* ExtraSound = UGS_AssetLoader::SyncLoadAsset(AresComboExtraSounds[ArrayIndex]);
		if (ExtraSound)
		{
			AkPlayingID ExtraPlayingID = UAkGameplayStatics::PostEvent(ExtraSound, OwnerSeeker, 0, FOnAkPostEventCallback());
			RegisterPlayingID(ExtraPlayingID);
		}
	}
}

// ===================
// 찬 전용 RTS 공격 사운드 함수 구현
// ===================

void UGS_SeekerAudioComponent::PlayRTSChanAttackSound()
{
	if (!OwnerSeeker || !OwnerSeeker->IsChan() || !ChanAxeSwingSound)
	{
		return;
	}

	// RTS 모드에서만 재생
	if (!IsRTSMode())
	{
		return;
	}

	if (!PrepareMulticastSound(OwnerSeeker, false))
	{
		return;
	}

	// RTS 모드에 따른 Distance Scaling 설정
	SetDistanceScaling(true);

	// 도끼 휘두르기 사운드 재생 (통합 에셋 사용)
	UAkAudioEvent* AxeSound = ChanAxeSwingSound.Get();
	if (AxeSound)
	{
		AkPlayingID AxePlayingID = UAkGameplayStatics::PostEvent(AxeSound, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(AxePlayingID);
	}

	// 공격 음성 사운드 재생 (통합 에셋 사용)
	UAkAudioEvent* VoiceSound = ChanAttackVoiceSound.Get();
	if (VoiceSound)
	{
		AkPlayingID VoicePlayingID = UAkGameplayStatics::PostEvent(VoiceSound, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(VoicePlayingID);
	}
}

void UGS_SeekerAudioComponent::PlayRTSChanShieldSlamSound()
{
	if (!OwnerSeeker || !OwnerSeeker->IsChan())
	{
		return;
	}

	// RTS 모드에서만 재생
	if (!IsRTSMode())
	{
		return;
	}

	// 거리 및 시야각 체크
	if (!PrepareMulticastSound(OwnerSeeker, false))
	{
		return;
	}

	// RTS 모드에 따른 Distance Scaling 설정
	SetDistanceScaling(true);

	// 방패 슬램 시작 사운드 재생 (통합 에셋 사용)
	UAkAudioEvent* StartSound = ShieldSlamStartSound.Get();
	if (StartSound)
	{
		AkPlayingID StartPlayingID = UAkGameplayStatics::PostEvent(StartSound, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(StartPlayingID);
	}

	// 방패 슬램 충돌 사운드 재생 (통합 에셋 사용)
	UAkAudioEvent* ImpactSound = ShieldSlamImpactSound.Get();
	if (ImpactSound)
	{
		AkPlayingID ImpactPlayingID = UAkGameplayStatics::PostEvent(ImpactSound, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(ImpactPlayingID);
	}
}

void UGS_SeekerAudioComponent::PlayRTSMerciBowDrawSound()
{
	if (!OwnerSeeker || !OwnerSeeker->IsMerci() || !BowDrawSound)
	{
		return;
	}

	// RTS 모드에서만 재생
	if (!IsRTSMode())
	{
		return;
	}

	// 거리 및 시야각 체크
	if (!PrepareMulticastSound(OwnerSeeker, false))
	{
		return;
	}

	// RTS 모드에 따른 Distance Scaling 설정
	SetDistanceScaling(true);

	UAkAudioEvent* SoundToPlay = BowDrawSound.Get();
	if (!SoundToPlay)
	{
		SoundToPlay = UGS_AssetLoader::SyncLoadAsset(BowDrawSound);
	}

	if (SoundToPlay)
	{
		AkPlayingID RTSBowPlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(RTSBowPlayingID);
	}
}

void UGS_SeekerAudioComponent::PlayRTSMerciArrowShotSound()
{
	if (!OwnerSeeker || !OwnerSeeker->IsMerci() || !ArrowShotSound)
	{
		return;
	}

	// RTS 모드에서만 재생
	if (!IsRTSMode())
	{
		return;
	}

	// 거리 및 시야각 체크
	if (!PrepareMulticastSound(OwnerSeeker, false))
	{
		return;
	}

	// RTS 모드에 따른 Distance Scaling 설정
	SetDistanceScaling(true);

	UAkAudioEvent* SoundToPlay = ArrowShotSound.Get();
	if (!SoundToPlay)
	{
		SoundToPlay = UGS_AssetLoader::SyncLoadAsset(ArrowShotSound);
	}

	if (SoundToPlay)
	{
		AkPlayingID RTSArrowPlayingID = UAkGameplayStatics::PostEvent(SoundToPlay, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(RTSArrowPlayingID);
	}
}

// ===================
// 공통 헬퍼 함수들 구현
// ===================

void UGS_SeekerAudioComponent::PlayComboSounds(int32 ArrayIndex, const TArray<TObjectPtr<UAkAudioEvent>>& SwingSounds,
                                               const TArray<TObjectPtr<UAkAudioEvent>>& VoiceSounds,
                                               const TArray<TObjectPtr<UAkAudioEvent>>* ExtraSounds,
                                               UAkAudioEvent* StopEvent, float ResetTime)
{
	if (!IsValid(OwnerSeeker))
	{
		return;
	}

	// Swing 사운드 재생
	if (SwingSounds.IsValidIndex(ArrayIndex) && SwingSounds[ArrayIndex])
	{
		AkPlayingID SwingPlayingID = UAkGameplayStatics::PostEvent(SwingSounds[ArrayIndex].Get(), OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(SwingPlayingID);
	}

	// Voice 사운드 재생
	if (VoiceSounds.IsValidIndex(ArrayIndex) && VoiceSounds[ArrayIndex])
	{
		AkPlayingID VoicePlayingID = UAkGameplayStatics::PostEvent(VoiceSounds[ArrayIndex].Get(), OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(VoicePlayingID);
	}

	// Extra 사운드 재생 (옵션)
	if (ExtraSounds && ExtraSounds->IsValidIndex(ArrayIndex) && (*ExtraSounds)[ArrayIndex])
	{
		AkPlayingID ExtraPlayingID = UAkGameplayStatics::PostEvent((*ExtraSounds)[ArrayIndex].Get(), OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(ExtraPlayingID);
	}

	// Stop 이벤트 설정 (옵션)
	if (StopEvent && ResetTime > 0.0f)
	{
		CurrentStopEvent = StopEvent;
		UWorld* World = GetWorld();
		if (World)
		{
			World->GetTimerManager().SetTimer(AttackSoundResetTimerHandle, this, &UGS_SeekerAudioComponent::ResetAttackSoundSequence, ResetTime, false);
		}
	}
}

bool UGS_SeekerAudioComponent::ShouldPlaySoundAtLocation(const FVector& SourceLocation, bool bSkipViewFrustumCheck) const
{
	// 오디오 시스템 검증 (데디케이티드 서버 및 Wwise 초기화 체크)
	if (!IsAudioSystemValid())
	{
		return false;
	}

	if (!OwnerSeeker || !GetWorld())
	{
		return false;
	}

	FVector ListenerLocation;
	if (!GetListenerLocation(ListenerLocation))
	{
		// GetListenerLocation 실패 시 (서버에서 로컬 플레이어가 없을 수 있음)
		// Multicast RPC는 클라이언트에서도 실행되므로 서버에서는 스킵해도 됨
		return false;
	}

	// RTS 모드와 TPS 모드에 따른 거리 체크
	const bool bRTS = IsRTSMode();
	const float MaxDistance = GetMaxDistanceForMode(bRTS);

	const float DistanceToListener = FVector::Dist(SourceLocation, ListenerLocation);

	// 모드별 체크 로직
	if (bRTS)
	{
		// RTS 모드: View Frustum 체크 (화면에 보이는지 확인)
		if (!bSkipViewFrustumCheck && !IsInViewFrustum(SourceLocation))
		{
			return false;
		}
	}
	else
	{
		// TPS 모드: 기존 거리 기반 체크
		if (DistanceToListener > MaxDistance)
		{
			return false;
		}
	}

	return true;
}

int32 UGS_SeekerAudioComponent::ValidateAndConvertComboIndex(int32 ComboIndex, int32 ArraySize) const
{
	// 1-based에서 0-based로 변환
	int32 ArrayIndex = ComboIndex - ComboIndexOffset;

	// 유효성 검사
	if (ArrayIndex < 0 || ArrayIndex >= ArraySize)
	{
		return INDEX_NONE;
	}

	return ArrayIndex;
}

// ==========================================
// UI 사운드 함수 (가디언 감지 시스템)
// ==========================================

void UGS_SeekerAudioComponent::PlayDetectionWarningSound()
{
	if (!DetectionWarningSound)
	{
		return;
	}

	if (OwnerSeeker && OwnerSeeker->IsLocallyControlled())
	{
		UGameplayStatics::PlaySound2D(GetWorld(), DetectionWarningSound);
	}
}

void UGS_SeekerAudioComponent::PlayDetectionClearedSound()
{
	if (!DetectionClearedSound)
	{
		return;
	}

	if (OwnerSeeker && OwnerSeeker->IsLocallyControlled())
	{
		UGameplayStatics::PlaySound2D(GetWorld(), DetectionClearedSound);
	}
}

// =========================
// LowHP Pain Sound 시스템
// =========================

void UGS_SeekerAudioComponent::StartLowHPPainSound()
{
	// 오디오 시스템 유효성 검증
	if (!IsAudioSystemValid() || bIsLowHPPainPlaying || !LowHPPainLoopSound)
	{
		return;
	}

	// Owner 검증
	if (!IsValid(OwnerSeeker))
	{
		return;
	}

	// Distance Scaling 설정
	if (!PrepareMulticastSound(OwnerSeeker, true))
	{
		return;
	}

	// 루핑 사운드 재생
	UAkAudioEvent* PainSound = LowHPPainLoopSound.Get();
	if (!PainSound)
	{
		return;
	}

	LowHPPainPlayingID = UAkGameplayStatics::PostEvent(
	    PainSound,
	    OwnerSeeker,
	    0,
	    FOnAkPostEventCallback());

	if (LowHPPainPlayingID != AK_INVALID_PLAYING_ID)
	{
		RegisterPlayingID(LowHPPainPlayingID);
		bIsLowHPPainPlaying = true;

		// 0.5초마다 HP 체크 타이머 시작
		UWorld* World = GetWorld();
		if (World && World->IsValidLowLevel() && !World->bIsTearingDown)
		{
			World->GetTimerManager().SetTimer(
			    LowHPCheckTimerHandle,
			    this,
			    &UGS_SeekerAudioComponent::OnLowHPPainCheck,
			    0.5f,
			    true // 반복
			);
		}
	}
}

void UGS_SeekerAudioComponent::StopLowHPPainSound()
{
	if (!bIsLowHPPainPlaying)
	{
		return;
	}

	// 정지 이벤트가 있으면 재생
	if (!LowHPPainStopSound.IsNull() && IsValid(OwnerSeeker))
	{
		UAkAudioEvent* StopSoundEvent = UGS_AssetLoader::SyncLoadAsset(LowHPPainStopSound);
		if (StopSoundEvent)
		{
			UAkGameplayStatics::PostEvent(
			    StopSoundEvent,
			    OwnerSeeker,
			    0,
			    FOnAkPostEventCallback());
		}
	}
	// 정지 이벤트가 없으면 Playing ID로 직접 중지
	else if (LowHPPainPlayingID != AK_INVALID_PLAYING_ID)
	{
		if (FAkAudioDevice* AkDevice = FAkAudioDevice::Get())
		{
			AkDevice->StopPlayingID(LowHPPainPlayingID, 500); // 500ms 페이드아웃
		}
	}

	// 상태 초기화
	bIsLowHPPainPlaying = false;
	LowHPPainPlayingID = AK_INVALID_PLAYING_ID;
	LastLowHPVolumeRatio = -1.0f;
	LastLowHPFilterRatio = -1.0f;

	// 타이머 중지
	if (LowHPCheckTimerHandle.IsValid())
	{
		UWorld* World = GetWorld();
		if (World && World->IsValidLowLevel() && !World->bIsTearingDown)
		{
			World->GetTimerManager().ClearTimer(LowHPCheckTimerHandle);
		}
		LowHPCheckTimerHandle.Invalidate();
	}
}

void UGS_SeekerAudioComponent::UpdateLowHPPainVolume(float CurrentHP, float MaxHP)
{
	if (!bIsLowHPPainPlaying)
	{
		return;
	}

	// HP 비율 계산 (30% 이하에서 0~1로 정규화)
	const float SafeMax = FMath::Max(1.0f, MaxHP);
	const float LowHPThresholdValue = SafeMax * LowHPThreshold;
	const float CurrentHPRatio = FMath::Clamp(CurrentHP / LowHPThresholdValue, 0.0f, 1.0f);

	// 볼륨/필터 비율 (HP 낮을수록 증가: 30% HP = 0.0, 0% HP = 1.0)
	const float VolumeRatio = 1.0f - CurrentHPRatio;
	const float FilterRatio = 1.0f - CurrentHPRatio;

	// 볼륨 RTPC 업데이트 (변화가 0.05 이상일 때만)
	if (LowHPPainVolumeRTPC && FMath::Abs(VolumeRatio - LastLowHPVolumeRatio) >= 0.05f)
	{
		SetUnifiedRTPCValue(LowHPPainVolumeRTPC, VolumeRatio);
		LastLowHPVolumeRatio = VolumeRatio;
	}

	// 필터 RTPC 업데이트 (변화가 0.05 이상일 때만)
	if (LowHPPainFilterRTPC && FMath::Abs(FilterRatio - LastLowHPFilterRatio) >= 0.05f)
	{
		SetUnifiedRTPCValue(LowHPPainFilterRTPC, FilterRatio);
		LastLowHPFilterRatio = FilterRatio;
	}
}

void UGS_SeekerAudioComponent::OnLowHPPainCheck()
{
	// 컴포넌트/Owner 유효성 검증
	if (!IsValid(this) || !IsValid(OwnerSeeker))
	{
		StopLowHPPainSound();
		return;
	}

	// StatComp 가져오기
	UGS_StatComp* StatComp = OwnerSeeker->GetStatComp();
	if (!StatComp)
	{
		StopLowHPPainSound();
		return;
	}

	const float CurrentHP = StatComp->GetCurrentHealth();
	const float MaxHP = StatComp->GetMaxHealth();
	const float HPRatio = CurrentHP / FMath::Max(1.0f, MaxHP);

	// HP 30% 이상 회복 시 중지
	if (HPRatio > LowHPThreshold)
	{
		StopLowHPPainSound();
		return;
	}

	// HP 0 이하 시 중지 (사망 판정)
	// 단, 빈사 상태(Downed)일 때는 고통 소리를 계속 재생함
	if (CurrentHP <= KINDA_SMALL_NUMBER)
	{
		if (OwnerSeeker && OwnerSeeker->IsInDyingState())
		{
			// 빈사 상태일 때는 0% HP 기준으로 볼륨 업데이트 후 계속 재생
			UpdateLowHPPainVolume(CurrentHP, MaxHP);
			return;
		}

		StopLowHPPainSound();
		return;
	}

	// RTPC 볼륨/필터 업데이트
	UpdateLowHPPainVolume(CurrentHP, MaxHP);
}

void UGS_SeekerAudioComponent::ForceStopLowHPPainSound()
{
	// 재생 중이 아니면 스킵
	if (!bIsLowHPPainPlaying)
	{
		return;
	}

	// Playing ID로 즉시 중지 (StopEvent 사용 안함)
	if (LowHPPainPlayingID != AK_INVALID_PLAYING_ID)
	{
		if (FAkAudioDevice* AkDevice = FAkAudioDevice::Get())
		{
			AkDevice->StopPlayingID(LowHPPainPlayingID, 100); // 100ms 빠른 페이드아웃
		}
	}

	// 상태 초기화
	bIsLowHPPainPlaying = false;
	LowHPPainPlayingID = AK_INVALID_PLAYING_ID;
	LastLowHPVolumeRatio = -1.0f;
	LastLowHPFilterRatio = -1.0f;
}

// =========================
// 빈사 상태 불꽃 사운드 구현
// =========================

void UGS_SeekerAudioComponent::PlayDyingFlameSpawnSound()
{
	UAkAudioEvent* SpawnSound = AudioConfig.DyingFlameSpawnSound.Get();
	if (SpawnSound && IsValid(OwnerSeeker))
	{
		AkPlayingID PlayingID = UAkGameplayStatics::PostEvent(SpawnSound, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(PlayingID);
	}
}

void UGS_SeekerAudioComponent::PlayDyingFlameLoopSound()
{
	UAkAudioEvent* LoopSound = AudioConfig.DyingFlameLoopSound.Get();
	if (LoopSound && IsValid(OwnerSeeker))
	{
		// 이미 재생 중이면 중복 재생 방지
		if (DyingFlameLoopPlayingID != AK_INVALID_PLAYING_ID)
		{
			return;
		}

		DyingFlameLoopPlayingID = UAkGameplayStatics::PostEvent(LoopSound, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(DyingFlameLoopPlayingID);
	}
}

void UGS_SeekerAudioComponent::StopDyingFlameLoopSound()
{
	if (DyingFlameLoopPlayingID != AK_INVALID_PLAYING_ID)
	{
		if (FAkAudioDevice* AkDevice = FAkAudioDevice::Get())
		{
			AkDevice->StopPlayingID(DyingFlameLoopPlayingID, 200); // 0.2초 페이드아웃
		}
		DyingFlameLoopPlayingID = AK_INVALID_PLAYING_ID;
	}
}

void UGS_SeekerAudioComponent::PlayDyingFlameEndSound()
{
	UAkAudioEvent* EndSound = AudioConfig.DyingFlameEndSound.Get();
	if (EndSound && IsValid(OwnerSeeker))
	{
		AkPlayingID PlayingID = UAkGameplayStatics::PostEvent(EndSound, OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(PlayingID);
	}
}

void UGS_SeekerAudioComponent::PlayDyingTimerWarningSound()
{
	if (!DyingTimerWarningSound)
	{
		return;
	}

	if (OwnerSeeker && OwnerSeeker->IsLocallyControlled())
	{
		UGameplayStatics::PlaySound2D(GetWorld(), DyingTimerWarningSound);
	}
}
// ==========================================
// Blueprint 호환성을 위한 Raw Pointer 오버로드 구현
// ==========================================

void UGS_SeekerAudioComponent::PlayComboSounds(int32 ArrayIndex, const TArray<UAkAudioEvent*>& SwingSounds,
                                               const TArray<UAkAudioEvent*>& VoiceSounds,
                                               const TArray<UAkAudioEvent*>* ExtraSounds,
                                               UAkAudioEvent* StopEvent, float ResetTime)
{
	if (!IsValid(OwnerSeeker))
	{
		return;
	}

	// Swing 사운드 재생
	if (SwingSounds.IsValidIndex(ArrayIndex) && SwingSounds[ArrayIndex])
	{
		AkPlayingID SwingPlayingID = UAkGameplayStatics::PostEvent(SwingSounds[ArrayIndex], OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(SwingPlayingID);
	}

	// Voice 사운드 재생
	if (VoiceSounds.IsValidIndex(ArrayIndex) && VoiceSounds[ArrayIndex])
	{
		AkPlayingID VoicePlayingID = UAkGameplayStatics::PostEvent(VoiceSounds[ArrayIndex], OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(VoicePlayingID);
	}

	// Extra 사운드 재생 (옵션)
	if (ExtraSounds && ExtraSounds->IsValidIndex(ArrayIndex) && (*ExtraSounds)[ArrayIndex])
	{
		AkPlayingID ExtraPlayingID = UAkGameplayStatics::PostEvent((*ExtraSounds)[ArrayIndex], OwnerSeeker, 0, FOnAkPostEventCallback());
		RegisterPlayingID(ExtraPlayingID);
	}

	// Stop 이벤트 설정 (옵션)
	if (StopEvent && ResetTime > 0.0f)
	{
		CurrentStopEvent = StopEvent;
		UWorld* World = GetWorld();
		if (World)
		{
			World->GetTimerManager().SetTimer(AttackSoundResetTimerHandle, this, &UGS_SeekerAudioComponent::ResetAttackSoundSequence, ResetTime, false);
		}
	}
}

void UGS_SeekerAudioComponent::PlayComboAttackSoundByIndex(int32 ComboIndex, const TArray<TObjectPtr<UAkAudioEvent>>& SwingSounds, const TArray<TObjectPtr<UAkAudioEvent>>& VoiceSounds, UAkAudioEvent* StopEvent, float ResetTime)
{
	if (!OwnerSeeker)
	{
		return;
	}

	// 1-based에서 0-based로 변환
	int32 ArrayIndex = ComboIndex - ComboIndexOffset;

	// 유효성 검사 (SwingSounds 기준)
	if (ArrayIndex < 0 || ArrayIndex >= SwingSounds.Num())
	{
		return;
	}

	PlayComboSounds(ArrayIndex, SwingSounds, VoiceSounds, nullptr, StopEvent, ResetTime);
}

void UGS_SeekerAudioComponent::PlayComboAttackSoundByIndexWithExtra(int32 ComboIndex, const TArray<TObjectPtr<UAkAudioEvent>>& SwingSounds, const TArray<TObjectPtr<UAkAudioEvent>>& VoiceSounds, const TArray<TObjectPtr<UAkAudioEvent>>& ExtraSounds, UAkAudioEvent* StopEvent, float ResetTime)
{
	if (!OwnerSeeker)
	{
		return;
	}

	// 1-based에서 0-based로 변환
	int32 ArrayIndex = ComboIndex - ComboIndexOffset;

	// 유효성 검사 (SwingSounds 기준)
	if (ArrayIndex < 0 || ArrayIndex >= SwingSounds.Num())
	{
		return;
	}

	PlayComboSounds(ArrayIndex, SwingSounds, VoiceSounds, &ExtraSounds, StopEvent, ResetTime);
}
