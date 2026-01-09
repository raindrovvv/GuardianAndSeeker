#include "Character/Skill/GS_SkillBase.h"
#include "Character/Player/Guardian/GS_Guardian.h"
#include "Character/Skill/GS_SkillComp.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Sound/GS_SeekerAudioComponent.h"
#include "System/Utility/GS_AssetLoader.h"

UTexture2D* UGS_SkillBase::GetSkillImage()
{
	// Soft Reference를 동기 로드 (UI는 즉시 표시되어야 하므로)
	return UGS_AssetLoader::SyncLoadAsset(SkillImage);
}

float UGS_SkillBase::GetCoolTime()
{
	return Cooltime;
}

void UGS_SkillBase::InitSkill(AGS_Player* InOwner, UGS_SkillComp* InOwningComp, ESkillSlot InSlot)
{
	OwnerCharacter = InOwner;
	OwningComp = InOwningComp;
	CurrentSkillType = InSlot;

	// VFX + 몽타주 에셋 프리로드 (비동기)
	PreloadSkillAssets();
}

void UGS_SkillBase::ActiveSkill()
{
	if (!CanActive())
	{
		return;
	}

	SetIsActive(true);

	return;
}

void UGS_SkillBase::OnSkillCanceledByDebuff()
{
	StopCastVFX();
}

void UGS_SkillBase::OnSkillAnimationEnd()
{
	AGS_Player* OwnerPlayer = Cast<AGS_Player>(OwnerCharacter);
	if (OwnerPlayer)
	{
		OwnerPlayer->GetSkillComp()->ResetAllowedSkillsMask();
	}
}

void UGS_SkillBase::ExecuteSkillEffect()
{
}

void UGS_SkillBase::DeactiveSkill()
{
	UE_LOG(LogTemp, Warning, TEXT("DeactiveSkill!!!!!!!!!!!!!!"));
	StopCastVFX();
	SetIsActive(false);

	// 시커 캐릭터라면 스킬 종료 시 상태를 복구함 (이동/회전 제어권 및 달리기 상태)
	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(OwnerCharacter))
	{
		Seeker->StateReset();
		Seeker->SetSeekerGait(EGait::Run);
	}
	else if (AGS_Player* OwnerPlayer = Cast<AGS_Player>(OwnerCharacter))
	{
		// 시커가 아닌 플레이어(가디언 등)는 스킬 마스크만 리셋
		if (OwnerPlayer->GetSkillComp())
		{
			OwnerPlayer->GetSkillComp()->ResetAllowedSkillsMask();
		}
	}
}

void UGS_SkillBase::OnSkillCommand()
{
}

bool UGS_SkillBase::CanActive() const
{
	return OwnerCharacter && !bIsCoolingDown;
}

bool UGS_SkillBase::GetIsActive() const
{
	return bIsActive;
}

void UGS_SkillBase::InterruptSkill()
{
	StopCastVFX();
	SetIsActive(false);

	// Seeker의 경우 StateReset()이 이동/회전 제어, CanChangeSeekerGait 등을 복구함
	// SetSeekerGait(Run)은 DeactiveSkill()에서 호출되므로 여기서는 StateReset만 호출
	if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(OwnerCharacter))
	{
		Seeker->StateReset();
		Seeker->SetSeekerGait(EGait::Run);
	}
}

void UGS_SkillBase::SetIsActive(bool bInIsActive)
{
	// 스킬 내 bIsActive 업데이트
	bIsActive = bInIsActive;

	// SkillComp 내 스킬 상태 업데이트
	AGS_Player* OwnerPlayer = Cast<AGS_Player>(OwnerCharacter);
	if (OwnerPlayer)
	{
		OwnerPlayer->GetSkillComp()->SetSkillActiveState(CurrentSkillType, bInIsActive);
	}
}

void UGS_SkillBase::StartCoolDown()
{
	if (OwningComp)
	{
		OwningComp->StartCooldownForSkill(CurrentSkillType);
	}
}


void UGS_SkillBase::PreloadSkillAssets()
{
	TArray<FSoftObjectPath> AssetsToLoad;

	if (!SkillCastVFX.IsNull())
	{
		AssetsToLoad.Add(SkillCastVFX.ToSoftObjectPath());
	}
	if (!SkillRangeVFX.IsNull())
	{
		AssetsToLoad.Add(SkillRangeVFX.ToSoftObjectPath());
	}
	if (!SkillImpactVFX.IsNull())
	{
		AssetsToLoad.Add(SkillImpactVFX.ToSoftObjectPath());
	}
	if (!SkillEnvImpactVFX.IsNull())
	{
		AssetsToLoad.Add(SkillEnvImpactVFX.ToSoftObjectPath());
	}
	if (!SkillEndVFX.IsNull())
	{
		AssetsToLoad.Add(SkillEndVFX.ToSoftObjectPath());
	}
	if (!SkillLoopVFX.IsNull())
	{
		AssetsToLoad.Add(SkillLoopVFX.ToSoftObjectPath());
	}

	for (const TSoftObjectPtr<UAnimMontage>& MontagePtr : SkillAnimMontages)
	{
		if (!MontagePtr.IsNull())
		{
			AssetsToLoad.Add(MontagePtr.ToSoftObjectPath());
		}
	}

	// 데이터 테이블에서 스킬 오디오 정보 가져와 프리로드 리스트에 추가
	const FSkillInfo* Info = GetCurrentSkillInfo();
	if (Info)
	{
		if (!Info->SkillStartSound.IsNull())
			AssetsToLoad.Add(Info->SkillStartSound.ToSoftObjectPath());
		if (!Info->SkillEndSound.IsNull())
			AssetsToLoad.Add(Info->SkillEndSound.ToSoftObjectPath());
		if (!Info->SkillLoopSound.IsNull())
			AssetsToLoad.Add(Info->SkillLoopSound.ToSoftObjectPath());
		if (!Info->SkillLoopStopSound.IsNull())
			AssetsToLoad.Add(Info->SkillLoopStopSound.ToSoftObjectPath());
		if (!Info->RTSSkillStartSound.IsNull())
			AssetsToLoad.Add(Info->RTSSkillStartSound.ToSoftObjectPath());
		if (!Info->RTSSkillEndSound.IsNull())
			AssetsToLoad.Add(Info->RTSSkillEndSound.ToSoftObjectPath());
		if (!Info->RTSSkillLoopSound.IsNull())
			AssetsToLoad.Add(Info->RTSSkillLoopSound.ToSoftObjectPath());
		if (!Info->RTSSkillLoopStopSound.IsNull())
			AssetsToLoad.Add(Info->RTSSkillLoopStopSound.ToSoftObjectPath());
		if (!Info->WallCollisionSound.IsNull())
			AssetsToLoad.Add(Info->WallCollisionSound.ToSoftObjectPath());
		if (!Info->MonsterCollisionSound.IsNull())
			AssetsToLoad.Add(Info->MonsterCollisionSound.ToSoftObjectPath());
		if (!Info->GuardianCollisionSound.IsNull())
			AssetsToLoad.Add(Info->GuardianCollisionSound.ToSoftObjectPath());
		if (!Info->RTSWallCollisionSound.IsNull())
			AssetsToLoad.Add(Info->RTSWallCollisionSound.ToSoftObjectPath());
		if (!Info->RTSMonsterCollisionSound.IsNull())
			AssetsToLoad.Add(Info->RTSMonsterCollisionSound.ToSoftObjectPath());
		if (!Info->RTSGuardianCollisionSound.IsNull())
			AssetsToLoad.Add(Info->RTSGuardianCollisionSound.ToSoftObjectPath());
	}

	if (AssetsToLoad.Num() > 0)
	{
		// TWeakObjectPtr로 캡처하여 UObject 파괴 후 람다 호출 시 안전성 확보
		TWeakObjectPtr<UGS_SkillBase> WeakThis(this);

		UGS_AssetLoader::AsyncLoadMultipleAssets(AssetsToLoad, [WeakThis]()
		                                         {
			if (UGS_SkillBase* Strong = WeakThis.Get())
			{
				// 로드 완료 후 캐싱
				Strong->CachedCastVFX = Strong->SkillCastVFX.Get();
				Strong->CachedRangeVFX = Strong->SkillRangeVFX.Get();
				Strong->CachedImpactVFX = Strong->SkillImpactVFX.Get();
				Strong->CachedEnvImpactVFX = Strong->SkillEnvImpactVFX.Get();
				Strong->CachedEndVFX = Strong->SkillEndVFX.Get();
				Strong->CachedLoopVFX = Strong->SkillLoopVFX.Get();

				Strong->CachedAnimMontages.SetNum(Strong->SkillAnimMontages.Num());
				for (int32 i = 0; i < Strong->SkillAnimMontages.Num(); ++i)
				{
					Strong->CachedAnimMontages[i] = Strong->SkillAnimMontages[i].Get();
				}

				// 오디오 캐싱
				if (const FSkillInfo* InfoPtr = Strong->GetCurrentSkillInfo())
				{
					Strong->CachedSkillStartSound = InfoPtr->SkillStartSound.Get();
					Strong->CachedSkillEndSound = InfoPtr->SkillEndSound.Get();
					Strong->CachedSkillLoopSound = InfoPtr->SkillLoopSound.Get();
					Strong->CachedSkillLoopStopSound = InfoPtr->SkillLoopStopSound.Get();
					Strong->CachedRTSSkillStartSound = InfoPtr->RTSSkillStartSound.Get();
					Strong->CachedRTSSkillEndSound = InfoPtr->RTSSkillEndSound.Get();
					Strong->CachedRTSSkillLoopSound = InfoPtr->RTSSkillLoopSound.Get();
					Strong->CachedRTSSkillLoopStopSound = InfoPtr->RTSSkillLoopStopSound.Get();
					Strong->CachedWallCollisionSound = InfoPtr->WallCollisionSound.Get();
					Strong->CachedMonsterCollisionSound = InfoPtr->MonsterCollisionSound.Get();
					Strong->CachedGuardianCollisionSound = InfoPtr->GuardianCollisionSound.Get();
					Strong->CachedRTSWallCollisionSound = InfoPtr->RTSWallCollisionSound.Get();
					Strong->CachedRTSMonsterCollisionSound = InfoPtr->RTSMonsterCollisionSound.Get();
					Strong->CachedRTSGuardianCollisionSound = InfoPtr->RTSGuardianCollisionSound.Get();
				}
			} });
	}
}

UAnimMontage* UGS_SkillBase::GetCachedMontage(int32 Index)
{
	// 인덱스 유효성 검사
	if (!SkillAnimMontages.IsValidIndex(Index))
	{
		return nullptr;
	}

	// 캐시된 몽타주가 있으면 반환
	if (CachedAnimMontages.IsValidIndex(Index) && CachedAnimMontages[Index])
	{
		return CachedAnimMontages[Index];
	}

	// 캐시되지 않았으면 LoadSynchronous로 폴백 (안전장치)
	if (!SkillAnimMontages[Index].IsNull())
	{
		UAnimMontage* LoadedMontage = UGS_AssetLoader::SyncLoadAsset(SkillAnimMontages[Index]);

		// 폴백으로 로드한 몽타주도 캐싱 (다음번 사용을 위해)
		if (LoadedMontage && CachedAnimMontages.IsValidIndex(Index))
		{
			CachedAnimMontages[Index] = LoadedMontage;
		}

		return LoadedMontage;
	}

	return nullptr;
}

void UGS_SkillBase::PlayCastVFX(FVector Location, FRotator Rotation)
{
	// 캐싱된 VFX 사용 (프리로드되지 않은 경우 즉시 로드)
	UNiagaraSystem* VFXToUse = UGS_AssetLoader::SyncLoadAsset(SkillCastVFX);
	if (!VFXToUse)
	{
		return;
	}

	if (OwnerCharacter)
	{
		// === 깜빡임(Flickering) 방지 로직 ===
		// 이미 해당 에셋이 재생 중이라면 중복 실행하지 않음
		if (ActiveCastVFXComponent && IsValid(ActiveCastVFXComponent) && ActiveCastVFXComponent->GetAsset() == VFXToUse)
		{
			return;
		}

		// 기존 Cast VFX가 있으면 먼저 로컬에서 정리 (이미 멀티캐스트 내부이므로 추가 RPC 불필요)
		Internal_StopCastVFX();

		UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		    VFXToUse,
		    OwnerCharacter->GetRootComponent(),
		    NAME_None,
		    CastVFXOffset, // 데이터 테이블에서 설정된 오프셋 사용
		    Rotation,
		    EAttachLocation::KeepRelativeOffset,
		    true);

		if (NiagaraComp)
		{
			// Cast VFX 컴포넌트 추적 저장
			ActiveCastVFXComponent = NiagaraComp;

			FVector Forward = OwnerCharacter->GetActorForwardVector();
			NiagaraComp->SetVectorParameter(FName("User.ForwardVector"), Forward);

			NiagaraComp->SetVectorParameter(FName("User.FixedVector"), FVector(1, 0, 0));
		}
	}
}

void UGS_SkillBase::PlayRangeVFX(FVector Location, float Radius)
{
	// 캐싱된 VFX 사용 (프리로드되지 않은 경우 즉시 로드)
	UNiagaraSystem* VFXToUse = CachedRangeVFX.Get();
	if (!VFXToUse)
	{
		VFXToUse = UGS_AssetLoader::SyncLoadAsset(SkillRangeVFX);
		CachedRangeVFX = VFXToUse; // 캐싱
	}

	if (VFXToUse && OwnerCharacter)
	{
		UNiagaraComponent* SpawnedVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
		    VFXToUse,
		    OwnerCharacter->GetRootComponent(),
		    NAME_None,
		    RangeVFXOffset, // 데이터 테이블에서 설정된 오프셋 사용
		    FRotator::ZeroRotator,
		    EAttachLocation::KeepRelativeOffset,
		    true);

		if (SpawnedVFX)
		{
			SpawnedVFX->SetWorldScale3D(SkillVFXScale);
			SpawnedVFX->SetFloatParameter(FName("Radius"), Radius);
			if (SkillVFXDuration > 0.0f)
			{
				SpawnedVFX->SetFloatParameter(FName("Duration"), SkillVFXDuration);
			}
		}
	}
}

void UGS_SkillBase::PlayImpactVFX(FVector Location)
{
	// 캐싱된 VFX 사용 (프리로드되지 않은 경우 즉시 로드)
	UNiagaraSystem* VFXToUse = CachedImpactVFX.Get();
	if (!VFXToUse)
	{
		VFXToUse = UGS_AssetLoader::SyncLoadAsset(SkillImpactVFX);
		CachedImpactVFX = VFXToUse; // 캐싱
	}

	if (VFXToUse && OwnerCharacter)
	{
		// 이 함수는 타겟 정보를 받지 않으므로, 월드 위치에 생성.
		// 타겟에 부착하려면 별도 함수 필요.
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		    GetWorld(),
		    VFXToUse,
		    Location,
		    FRotator::ZeroRotator,
		    SkillVFXScale,
		    true, true, ENCPoolMethod::None);
	}
}

// 타겟에 직접 Impact VFX를 부착하는 새 함수
void UGS_SkillBase::PlayImpactVFXOnTarget(AActor* Target)
{
	if (!Target || SkillImpactVFX.IsNull())
		return;

	TWeakObjectPtr<AActor> WeakTarget(Target);
	TWeakObjectPtr<UGS_SkillBase> WeakThis(this);

	UGS_AssetLoader::AsyncLoadAsset<UNiagaraSystem>(
	    SkillImpactVFX,
	    [WeakThis, WeakTarget](UNiagaraSystem* LoadedVFX)
	    {
		    if (WeakThis.IsValid() && WeakTarget.IsValid() && LoadedVFX)
		    {
			    // 캐싱
			    WeakThis->CachedImpactVFX = LoadedVFX;

			    UNiagaraFunctionLibrary::SpawnSystemAttached(
			        LoadedVFX,
			        WeakTarget->GetRootComponent(),
			        NAME_None,
			        WeakThis->ImpactVFXOffset,
			        FRotator::ZeroRotator,
			        EAttachLocation::KeepRelativeOffset,
			        true);
		    }
	    });
}

void UGS_SkillBase::PlayEnvImpactVFX(FVector Location, FRotator Rotation)
{
	if (SkillEnvImpactVFX.IsNull() || !GetWorld())
		return;

	TWeakObjectPtr<UGS_SkillBase> WeakThis(this);
	UGS_AssetLoader::AsyncLoadAsset<UNiagaraSystem>(
	    SkillEnvImpactVFX,
	    [WeakThis, Location, Rotation](UNiagaraSystem* LoadedVFX)
	    {
		    if (WeakThis.IsValid() && LoadedVFX)
		    {
			    // 캐싱
			    WeakThis->CachedEnvImpactVFX = LoadedVFX;

			    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			        WeakThis->GetWorld(),
			        LoadedVFX,
			        Location + Rotation.RotateVector(WeakThis->EnvImpactVFXOffset),
			        Rotation);
		    }
	    });
}

void UGS_SkillBase::PlayEndVFX(FVector Location, FRotator Rotation)
{
	// 캐싱된 VFX 사용 (프리로드되지 않은 경우 즉시 로드)
	UNiagaraSystem* VFXToUse = CachedEndVFX.Get();
	if (!VFXToUse)
	{
		VFXToUse = UGS_AssetLoader::SyncLoadAsset(SkillEndVFX);
		CachedEndVFX = VFXToUse; // 캐싱
	}

	if (VFXToUse && OwnerCharacter)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
		    VFXToUse,
		    OwnerCharacter->GetRootComponent(),
		    NAME_None,
		    EndVFXOffset, // 데이터 테이블에서 설정된 오프셋 사용
		    Rotation,
		    EAttachLocation::KeepRelativeOffset,
		    true);
	}

	// End VFX 재생 시 Cast VFX 정리
	StopCastVFX();
}

void UGS_SkillBase::StopCastVFX()
{
	// 서버에서 중지 요청 시 모든 클라이언트에도 전파
	if (OwnerCharacter && OwnerCharacter->HasAuthority() && OwningComp)
	{
		OwningComp->Multicast_StopCastVFX(CurrentSkillType);
		return;
	}

	Internal_StopCastVFX();
}

void UGS_SkillBase::Internal_StopCastVFX()
{
	if (ActiveCastVFXComponent && IsValid(ActiveCastVFXComponent))
	{
		// VFX 비활성화 및 제거
		ActiveCastVFXComponent->Deactivate();
		ActiveCastVFXComponent->DestroyComponent();
		ActiveCastVFXComponent = nullptr;
	}
}

const FSkillInfo* UGS_SkillBase::GetCurrentSkillInfo() const
{
	if (!OwningComp || !OwnerCharacter)
	{
		return nullptr;
	}

	UDataTable* SkillDataTable = OwningComp->GetSkillDataTable();
	if (!SkillDataTable)
	{
		return nullptr;
	}

	// 캐릭터 타입을 기반으로 RowName 구하기
	FName RowName = FName(*UEnum::GetValueAsString(OwnerCharacter->GetCharacterType()).RightChop(UEnum::GetValueAsString(OwnerCharacter->GetCharacterType()).Find(TEXT("::")) + 2));

	FString Context;
	const FGS_SkillSet* SkillSet = SkillDataTable->FindRow<FGS_SkillSet>(RowName, Context);
	if (!SkillSet)
	{
		return nullptr;
	}

	// 현재 스킬 슬롯에 따라 적절한 스킬 정보 반환
	switch (CurrentSkillType)
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

void UGS_SkillBase::PlaySkillStartSound() const
{
	if (!OwnerCharacter)
	{
		return;
	}

	// 시커인 경우 SeekerAudioComponent 사용
	if (AGS_Seeker* OwnerSeeker = Cast<AGS_Seeker>(OwnerCharacter))
	{
		if (UGS_SeekerAudioComponent* AudioComp = OwnerSeeker->SeekerAudioComponent)
		{
			AudioComp->RequestSkillAudio(CurrentSkillType, 0); // 0 = 스킬 시작
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No SeekerAudioComponent found for character: %s"), *OwnerCharacter->GetName());
	}
}

void UGS_SkillBase::PlaySkillEndSound() const
{
	if (!OwnerCharacter)
	{
		return;
	}

	// 시커인 경우 SeekerAudioComponent 사용
	if (AGS_Seeker* OwnerSeeker = Cast<AGS_Seeker>(OwnerCharacter))
	{
		if (UGS_SeekerAudioComponent* AudioComp = OwnerSeeker->SeekerAudioComponent)
		{
			AudioComp->RequestSkillAudio(CurrentSkillType, 1); // 1 = 스킬 종료
		}
	}
	// 다른 캐릭터 타입은 각자의 오디오 컴포넌트 사용
}

void UGS_SkillBase::BeginDestroy()
{
	Super::BeginDestroy();
	if (IsValid(GetWorld()) && bIsActive)
	{
		DeactiveSkill();
		OnSkillAnimationEnd();
	}
}

void UGS_SkillBase::InitializeDelegate()
{
	return;
}
