// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Skill/GS_SkillComp.h"
#include "Character/Player/GS_Player.h"
#include "Character/Skill/GS_SkillBase.h"
#include "Character/Skill/GS_SkillSet.h"
#include "UI/Character/GS_SkillWidget.h"
#include "Net/UnrealNetwork.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/E_Character.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "System/Utility/GS_AssetLoader.h"


UGS_SkillComp::UGS_SkillComp()
{
	PrimaryComponentTick.bCanEverTick = false;
	CurAllowedSkillsMask = DefaultAllowedSkillsMask;
}

void UGS_SkillComp::ApplyCooldownModifier(ESkillSlot Slot, float Ratio)
{
	if (UGS_SkillBase* Skill = SkillMap.FindRef(Slot))
	{
		Skill->Cooltime *= Ratio;
	}

	// 현재 쿨다운이 있는 상태면 남은 시간도 줄여줌
	if (FSkillCooldownState* State = CooldownStates.Find(Slot))
	{
		if (State->bIsOnCooldown)
		{
			// 남은 시간 비례 축소
			State->CooldownRemaining *= Ratio;

			// 타이머 갱신
			float NewRemaining = State->CooldownRemaining;

			GetWorld()->GetTimerManager().ClearTimer(State->CooldownTimer);
			GetWorld()->GetTimerManager().ClearTimer(State->UIUpdateTimer);

			if (NewRemaining > 0.0f)
			{
				GetWorld()->GetTimerManager().SetTimer(
				    State->CooldownTimer,
				    [this, Slot]()
				    { HandleCooldownComplete(Slot); },
				    NewRemaining,
				    false);

				GetWorld()->GetTimerManager().SetTimer(
				    State->UIUpdateTimer,
				    [this, Slot]()
				    { HandleCooldownProgress(Slot); },
				    0.1f,
				    true);
			}

			UpdateReplicatedCooldownStates(); // UI에도 반영
		}
	}
}

void UGS_SkillComp::ResetCooldownModifier(ESkillSlot Slot)
{
	if (!SkillDataTable || !GetOwner())
		return;

	UGS_SkillBase* Skill = GetSkillFromSkillMap(Slot);
	if (!Skill)
		return;

	// 캐릭터 타입을 기반으로 RowName 구하기
	AGS_Character* OwnerCharacter = Cast<AGS_Character>(GetOwner());
	if (!OwnerCharacter)
		return;

	FName RowName = FName(*UEnum::GetValueAsString(OwnerCharacter->GetCharacterType()).RightChop(UEnum::GetValueAsString(OwnerCharacter->GetCharacterType()).Find(TEXT("::")) + 2));

	FString Context;
	const FGS_SkillSet* SkillSet = SkillDataTable->FindRow<FGS_SkillSet>(RowName, Context);
	if (!SkillSet)
		return;

	float OriginalCooltime = 0.f;

	switch (Slot)
	{
	case ESkillSlot::Ready:
		OriginalCooltime = SkillSet->ReadySkill.Cooltime;
		break;
	case ESkillSlot::Moving:
		OriginalCooltime = SkillSet->MovingSkill.Cooltime;
		break;
	case ESkillSlot::Aiming:
		OriginalCooltime = SkillSet->AimingSkill.Cooltime;
		break;
	case ESkillSlot::Ultimate:
		OriginalCooltime = SkillSet->UltimateSkill.Cooltime;
		break;
	case ESkillSlot::Rolling:
		OriginalCooltime = SkillSet->RollingSkill.Cooltime;
		break;
	default:
		break;
	}

	if (OriginalCooltime > 0.f)
	{
		Skill->Cooltime = OriginalCooltime;
	}
}

void UGS_SkillComp::BeginPlay()
{
	Super::BeginPlay();

	SetIsReplicated(true);
	InitSkills();
}

bool UGS_SkillComp::IsSkillAllowed(ESkillSlot CompareSkillType)
{
	uint16 BitFlag = 0;
	BitFlag |= (1 << static_cast<int32>(CompareSkillType));
	return CurAllowedSkillsMask & BitFlag;
}

void UGS_SkillComp::SetCurAllowedSkillsMask(int16 BitMask)
{
	CurAllowedSkillsMask = BitMask;
}

int16 UGS_SkillComp::GetCurAllowedSkillsMask()
{
	return CurAllowedSkillsMask;
}

void UGS_SkillComp::AddAllowedSkill(ESkillSlot Slot)
{
	if (Slot == ESkillSlot::End)
		return;

	int16 BitFlag = (1 << static_cast<int32>(Slot));
	CurAllowedSkillsMask |= BitFlag;
}

void UGS_SkillComp::RemoveAllowedSkill(ESkillSlot Slot)
{
	if (Slot == ESkillSlot::End)
		return;

	int16 BitFlag = (1 << static_cast<int32>(Slot));
	CurAllowedSkillsMask &= ~BitFlag;
}

void UGS_SkillComp::InitSkills()
{
	AGS_Character* OwnerCharacter = Cast<AGS_Character>(GetOwner());
	if (!OwnerCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT(">>> InitSkills: Invalid OwnerCharacter"));
		return;
	}

	if (!SkillDataTable)
	{
		UE_LOG(LogTemp, Error, TEXT(">>> SkillDataTable is null!"));
		return;
	}

	FName RowName = FName(*UEnum::GetValueAsString(OwnerCharacter->GetCharacterType()).RightChop(UEnum::GetValueAsString(OwnerCharacter->GetCharacterType()).Find(TEXT("::")) + 2));
	FString Context;

	FGS_SkillSet* SkillSet = SkillDataTable->FindRow<FGS_SkillSet>(RowName, Context);
	if (SkillSet)
	{
		// 스킬 세팅
		SetSkill(ESkillSlot::Ready, SkillSet->ReadySkill);
		SetSkill(ESkillSlot::Aiming, SkillSet->AimingSkill);
		SetSkill(ESkillSlot::Moving, SkillSet->MovingSkill);
		SetSkill(ESkillSlot::Ultimate, SkillSet->UltimateSkill);
		SetSkill(ESkillSlot::Rolling, SkillSet->RollingSkill);
		SetSkill(ESkillSlot::Combo, SkillSet->ComboSkill);
		SetSkill(ESkillSlot::HealPotion, SkillSet->HealPotionSkill);

		// 스킬 초기화 후 마스크 리셋 (초기 상태에서 스킬 사용 가능하도록)
		ResetAllowedSkillsMask();
	}
}

void UGS_SkillComp::ResetAllowedSkillsMask()
{
	CurAllowedSkillsMask = DefaultAllowedSkillsMask;
}

void UGS_SkillComp::Server_TrySkillAnimationEnd_Implementation(ESkillSlot Slot)
{
	if (SkillMap.Contains(Slot))
	{
		SkillMap[Slot]->OnSkillAnimationEnd();
	}
}

void UGS_SkillComp::TrySkillAnimationEnd(ESkillSlot Slot)
{
	if (GetOwner()->HasAuthority())
	{
		if (SkillMap.Contains(Slot))
		{
			SkillMap[Slot]->OnSkillAnimationEnd();
		}
	}
}

void UGS_SkillComp::SetSkill(ESkillSlot Slot, const FSkillInfo& Info)
{
	if (!Info.SkillClass)
	{
		UE_LOG(LogTemp, Verbose, TEXT(">>> SetSkill: Invalid SkillClass"));
		return;
	}

	UGS_SkillBase* Skill = NewObject<UGS_SkillBase>(this, Info.SkillClass);
	if (!Skill)
	{
		UE_LOG(LogTemp, Error, TEXT(">>> SetSkill: Failed to create skill object"));
		return;
	}

	Skill->InitSkill(Cast<AGS_Player>(GetOwner()), this, Slot);
	Skill->Cooltime = Info.Cooltime;
	Skill->Damage = Info.Damage;

	// Soft Reference 할당 (데이터 테이블에서 이미 SoftPtr 임)
	Skill->SkillAnimMontages = Info.Montages;
	Skill->SkillImage = Info.Image;


	for (const FSkillAllow& Entry : Info.AllowSkillList)
	{
		const int32 Index = static_cast<int32>(Entry.Slot);

		if (Index >= 0 && Index < 8) // uint8 제한
		{
			if (Entry.bAllow)
			{
				Skill->AllowSkillsMask |= (1 << Index);
			}
		}
	}

	Skill->AllowControlValue = Info.AllowControlValue;

	// VFX 정보 설정
	Skill->SkillCastVFX = Info.SkillCastVFX;
	Skill->SkillRangeVFX = Info.SkillRangeVFX;
	Skill->SkillImpactVFX = Info.SkillImpactVFX;
	Skill->SkillEnvImpactVFX = Info.SkillEnvImpactVFX;
	Skill->SkillEndVFX = Info.SkillEndVFX;
	Skill->SkillLoopVFX = Info.SkillLoopVFX;
	Skill->SkillVFXScale = Info.SkillVFXScale;
	Skill->SkillVFXDuration = Info.SkillVFXDuration;

	// VFX 오프셋 정보 설정
	Skill->CastVFXOffset = Info.CastVFXOffset;
	Skill->RangeVFXOffset = Info.RangeVFXOffset;
	Skill->ImpactVFXOffset = Info.ImpactVFXOffset;
	Skill->EndVFXOffset = Info.EndVFXOffset;
	Skill->LoopVFXOffset = Info.LoopVFXOffset;

	SkillMap.Add(Slot, Skill);

	// Init Delegate
	Skill->InitializeDelegate();
}

void UGS_SkillComp::Server_TryActivateSkill_Implementation(ESkillSlot Slot)
{
	if (!bCanUseSkill)
	{
		return;
	}

	if (SkillMap.Contains(Slot))
	{
		if (SkillMap[Slot]->CanActive())
		{
			AGS_Player* OwnerPlayer = Cast<AGS_Player>(GetOwner());
			if (OwnerPlayer)
			{
				if (IsSkillAllowed(Slot))
				{
					SkillsInterrupt();
					SkillMap[Slot]->ActiveSkill();

					// 이동 스킬 사용 시 즉시 네트워크 복제 (위치 동기화 - 워프 현상 방지)
					if (Slot == ESkillSlot::Moving || Slot == ESkillSlot::Rolling)
					{
						GetOwner()->ForceNetUpdate();
					}

					ResetAllowedSkillsMask();
					SetCurAllowedSkillsMask(SkillMap[Slot]->AllowSkillsMask);

					// 스킬 활성화 알림
					if (GetOwner()->GetLocalRole() == ROLE_Authority)
					{
						Client_BroadcastSkillActivation(Slot);
					}
				}
			}
		}
		else
		{
			// 쿨타임 중이거나 사용할 수 없는 상태일 때 알림
			if (GetOwner()->GetLocalRole() == ROLE_Authority)
			{
				Client_BroadcastSkillCooldownBlocked(Slot);
			}
		}
	}
}

void UGS_SkillComp::Client_BroadcastSkillActivation_Implementation(ESkillSlot Slot)
{
	OnSkillActivated.Broadcast(Slot);
}

void UGS_SkillComp::Client_BroadcastHealCountChanged_Implementation(ESkillSlot Slot, int32 CurrentCount, int32 MaxCount)
{
	OnHealCountChanged.Broadcast(Slot, CurrentCount, MaxCount);
}

void UGS_SkillComp::Client_BroadcastSkillCooldownBlocked_Implementation(ESkillSlot Slot)
{
	OnSkillCooldownBlocked.Broadcast(Slot);
}

void UGS_SkillComp::Client_BroadcastSkillCooldownReady_Implementation(ESkillSlot Slot)
{
	OnSkillCooldownReady.Broadcast(Slot);
}

void UGS_SkillComp::Server_TryDeactiveSkill_Implementation(ESkillSlot Slot)
{
	if (SkillMap.Contains(Slot))
	{
		SkillMap[Slot]->DeactiveSkill();
	}
}

void UGS_SkillComp::Server_TrySkillCanceledByDebuff_Implementation(ESkillSlot Slot)
{
	if (SkillMap.Contains(Slot))
	{
		SkillMap[Slot]->OnSkillCanceledByDebuff();
	}
}

void UGS_SkillComp::Server_TrySkillCommand_Implementation(ESkillSlot Slot)
{
	if (SkillMap.Contains(Slot))
	{
		if (UGS_SkillBase* Skill = SkillMap[Slot])
		{
			Skill->OnSkillCommand();
		}
	}
}

void UGS_SkillComp::SetCanUseSkill(bool InCanUseSkill)
{
	bCanUseSkill = InCanUseSkill;
}

void UGS_SkillComp::SetSkillActiveState(ESkillSlot Slot, bool InIsActive)
{
	bool bFound = false;

	// ReplicatedSkillStates 갱신
	for (FSkillRuntimeState& State : ReplicatedSkillStates)
	{
		if (State.Slot == Slot)
		{
			State.bIsActive = InIsActive;
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		ReplicatedSkillStates.Add({Slot, InIsActive});
	}

	// SkillStates는 항상 갱신 (Standalone 대응)
	SkillStates.FindOrAdd(Slot).Slot = Slot;
	SkillStates[Slot].bIsActive = InIsActive;
}

bool UGS_SkillComp::IsSkillActive(ESkillSlot Slot) const
{
	if (const FSkillRuntimeState* State = SkillStates.Find(Slot))
	{
		return State->bIsActive;
	}

	return false;
}

UGS_SkillBase* UGS_SkillComp::GetActiveSkill() const
{
	for (const auto& Pair : SkillStates)
	{
		if (Pair.Value.bIsActive)
		{
			if (UGS_SkillBase* const* SkillPtr = SkillMap.Find(Pair.Key))
			{
				return *SkillPtr;
			}
		}
	}

	return nullptr;
}

void UGS_SkillComp::StartCooldownForSkill(ESkillSlot Slot)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	UGS_SkillBase* Skill = SkillMap.FindRef(Slot);
	if (!Skill)
	{
		return;
	}

	float CooldownTime = Skill->GetCoolTime();
	if (CooldownTime <= 0.0f)
	{
		return;
	}

	FSkillCooldownState& State = CooldownStates.FindOrAdd(Slot);
	State.Slot = Slot;
	State.CooldownRemaining = CooldownTime;
	State.bIsOnCooldown = true;

	// 쿨다운 타이머
	TWeakObjectPtr<UGS_SkillComp> WeakThis = this;
	GetWorld()->GetTimerManager().SetTimer(
	    State.CooldownTimer,
	    [WeakThis, Slot]()
	    {
		    if (WeakThis.IsValid())
		    {
			    WeakThis->HandleCooldownComplete(Slot);
		    }
	    },
	    CooldownTime,
	    false);

	// UI 업데이트 타이머
	GetWorld()->GetTimerManager().SetTimer(
	    State.UIUpdateTimer,
	    [WeakThis, Slot]()
	    {
		    if (WeakThis.IsValid())
		    {
			    WeakThis->HandleCooldownProgress(Slot);
		    }
	    },
	    0.1f,
	    true);

	Skill->SetCoolingDown(true);
	UpdateReplicatedCooldownStates();
}

void UGS_SkillComp::SkillsInterrupt()
{
	AGS_Seeker* Seeker = Cast<AGS_Seeker>(GetOwner());
	if (Seeker == nullptr)
	{
		return;
	}

	if (Seeker->CurrentComboIndex > 0)
	{
		Seeker->CurrentComboIndex = 0;
		Seeker->CanAcceptComboInput = true;
		Seeker->bHasBufferedNextCombo = false;
	}

	for (TPair<ESkillSlot, UGS_SkillBase*> slot : SkillMap)
	{
		//UE_LOG(LogTemp, Warning, TEXT("%s"), *slot.Value->GetName());

		if (Seeker->GetSkillComp()->IsSkillActive(slot.Key))
		{
			slot.Value->InterruptSkill();
		}
	}
}

void UGS_SkillComp::HandleCooldownComplete(ESkillSlot Slot)
{
	FSkillCooldownState* State = CooldownStates.Find(Slot);
	if (!State)
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(State->CooldownTimer);
	GetWorld()->GetTimerManager().ClearTimer(State->UIUpdateTimer);

	State->CooldownRemaining = 0.0f;
	State->bIsOnCooldown = false;

	UpdateReplicatedCooldownStates();

	if (UGS_SkillBase* Skill = SkillMap.FindRef(Slot))
	{
		Skill->SetCoolingDown(false);
	}

	// 스킬 쿨다운 완료 알림을 클라이언트에 전송
	if (GetOwner()->GetLocalRole() == ROLE_Authority)
	{
		Client_BroadcastSkillCooldownReady(Slot);
	}
}

void UGS_SkillComp::HandleCooldownProgress(ESkillSlot Slot)
{
	FSkillCooldownState* State = CooldownStates.Find(Slot);
	if (!State || !State->bIsOnCooldown)
	{
		return;
	}

	float RemainingTime = GetWorld()->GetTimerManager().GetTimerRemaining(State->CooldownTimer);
	RemainingTime = FMath::Max(0.0f, RemainingTime);

	State->CooldownRemaining = RemainingTime;
	UpdateReplicatedCooldownStates();
}

void UGS_SkillComp::UpdateReplicatedCooldownStates()
{
	ReplicatedCooldownStates.Empty();
	for (const auto& Pair : CooldownStates)
	{
		ReplicatedCooldownStates.Add(Pair.Value);
	}
}

void UGS_SkillComp::OnRep_SkillStates()
{
	SkillStates.Empty();
	for (const FSkillRuntimeState& State : ReplicatedSkillStates)
	{
		SkillStates.Add(State.Slot, State);
	}
}

void UGS_SkillComp::OnRep_CooldownStates()
{
	for (const FSkillCooldownState& State : ReplicatedCooldownStates)
	{
		ESkillSlot Slot = State.Slot;

		OnSkillCooldownChanged.Broadcast(Slot, State.CooldownRemaining);

		if (UGS_SkillBase* Skill = SkillMap.FindRef(Slot))
		{
			Skill->SetCoolingDown(State.bIsOnCooldown);
		}
	}
}

void UGS_SkillComp::InitializeSkillWidget(UGS_SkillWidget* InSkillWidget)
{
	if (IsValid(InSkillWidget))
	{
		//client
		ESkillSlot Slot = InSkillWidget->GetSkillSlot();

		InitSkills();

		if (SkillMap.Contains(Slot))
		{
			InSkillWidget->InitSkill(SkillMap[Slot]);

			OnSkillCooldownChanged.AddUObject(InSkillWidget, &UGS_SkillWidget::OnSkillCoolTimeChanged);
			OnHealCountChanged.AddUObject(InSkillWidget, &UGS_SkillWidget::OnHealCountChanged);
			OnSkillActivated.AddDynamic(InSkillWidget, &UGS_SkillWidget::OnSkillActivated);
			OnSkillCooldownBlocked.AddDynamic(InSkillWidget, &UGS_SkillWidget::OnSkillCooldownBlocked);
			OnSkillCooldownReady.AddDynamic(InSkillWidget, &UGS_SkillWidget::OnSkillCooldownReady);
		}
	}
}


UGS_SkillBase* UGS_SkillComp::GetSkillFromSkillMap(ESkillSlot Slot)
{
	if (SkillMap.Contains(Slot))
	{
		return SkillMap[Slot];
	}
	return nullptr;
}

void UGS_SkillComp::Multicast_PlayCastVFX_Implementation(ESkillSlot Slot, FVector Location, FRotator Rotation)
{
	if (UGS_SkillBase* Skill = GetSkillFromSkillMap(Slot))
	{
		Skill->PlayCastVFX(Location, Rotation);
	}
}

void UGS_SkillComp::Multicast_PlayRangeVFX_Implementation(ESkillSlot Slot, FVector Location, float Radius)
{
	if (UGS_SkillBase* Skill = GetSkillFromSkillMap(Slot))
	{
		Skill->PlayRangeVFX(Location, Radius);
	}
}

void UGS_SkillComp::Multicast_PlayImpactVFX_Implementation(ESkillSlot Slot, FVector Location)
{
	if (UGS_SkillBase* Skill = GetSkillFromSkillMap(Slot))
	{
		Skill->PlayImpactVFX(Location);
	}
}

void UGS_SkillComp::Multicast_PlayImpactVFXOnTarget_Implementation(ESkillSlot Slot, AActor* Target)
{
	if (UGS_SkillBase* Skill = GetSkillFromSkillMap(Slot))
	{
		Skill->PlayImpactVFXOnTarget(Target);
	}
}

void UGS_SkillComp::Multicast_PlayEnvImpactVFX_Implementation(ESkillSlot Slot, FVector Location, FRotator Rotation)
{
	if (UGS_SkillBase* Skill = GetSkillFromSkillMap(Slot))
	{
		Skill->PlayEnvImpactVFX(Location, Rotation);
	}
}

void UGS_SkillComp::Multicast_PlayEndVFX_Implementation(ESkillSlot Slot, FVector Location, FRotator Rotation)
{
	if (UGS_SkillBase* Skill = GetSkillFromSkillMap(Slot))
	{
		Skill->PlayEndVFX(Location, Rotation);
	}
}

void UGS_SkillComp::Multicast_PlayLoopVFX_Implementation(ESkillSlot Slot, AActor* AttachTarget)
{
	// 데디케이티드 서버에서는 VFX 재생하지 않음
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 유효성 검사
	if (!AttachTarget || !IsValid(AttachTarget))
	{
		UE_LOG(LogTemp, Error, TEXT("[SkillComp] Multicast_PlayLoopVFX: AttachTarget이 유효하지 않습니다!"));
		return;
	}

	// Skill 객체에서 Loop VFX 가져오기
	UGS_SkillBase* Skill = GetSkillFromSkillMap(Slot);
	if (!Skill || Skill->SkillLoopVFX.IsNull())
	{
		return;
	}

	// === 깜빡임(Flickering) 방지 로직 ===
	// TSoftObjectPtr::Get()은 로드되어 있어도 null을 반환할 수 있으므로 SyncLoadAsset 사용 (이미 로드된 에셋은 즉시 반환함)
	UNiagaraSystem* TargetVFX = UGS_AssetLoader::SyncLoadAsset(Skill->SkillLoopVFX);

	if (UNiagaraComponent** FoundComponent = ActiveLoopVFXComponents.Find(Slot))
	{
		if (*FoundComponent && IsValid(*FoundComponent) && (*FoundComponent)->GetAsset() == TargetVFX)
		{
			// 이미 같은 VFX가 재생 중이므로 종료
			return;
		}
	}

	TWeakObjectPtr<UGS_SkillComp> WeakThis(this);
	TWeakObjectPtr<AActor> WeakTarget(AttachTarget);

	// 이전 비동기 로드 취소
	if (FAsyncLoadHandle* Handle = PendingLoopVFXLoads.Find(Slot))
	{
		Handle->Get()->CancelHandle();
		PendingLoopVFXLoads.Remove(Slot);
	}

	FAsyncLoadHandle NewHandle = UGS_AssetLoader::AsyncLoadAsset<UNiagaraSystem>(
	    Skill->SkillLoopVFX,
	    [WeakThis, WeakTarget, Slot, TargetVFX](UNiagaraSystem* LoadedVFX)
	    {
		    if (!WeakThis.IsValid() || !WeakTarget.IsValid() || !LoadedVFX)
		    {
			    return;
		    }

		    // 비동기 로드 완료 시점에 다시 한 번 체크
		    if (UNiagaraComponent** ActiveComp = WeakThis->ActiveLoopVFXComponents.Find(Slot))
		    {
			    if (*ActiveComp && IsValid(*ActiveComp) && (*ActiveComp)->GetAsset() == LoadedVFX)
			    {
				    return;
			    }
		    }

		    // 기존 Loop VFX 정리
		    WeakThis->Multicast_StopLoopVFX(Slot);

		    UGS_SkillBase* CurrentSkill = WeakThis->GetSkillFromSkillMap(Slot);
		    if (!CurrentSkill)
		    {
			    return;
		    }

		    // 새로운 Loop VFX 생성 (Explicit Destroy를 위해 bAutoDestroy=false 권장)
		    UNiagaraComponent* LoopVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		        LoadedVFX,
		        WeakTarget->GetRootComponent(),
		        NAME_None,
		        CurrentSkill->LoopVFXOffset,
		        FRotator::ZeroRotator,
		        EAttachLocation::KeepRelativeOffset,
		        false // bAutoDestroy -> StopLoopVFX에서 명시적으로 제어
		    );

		    if (LoopVFXComponent)
		    {
			    LoopVFXComponent->Activate();
			    WeakThis->ActiveLoopVFXComponents.Add(Slot, LoopVFXComponent);
		    }

		    // 로드 완료 후 맵에서 제거
		    WeakThis->PendingLoopVFXLoads.Remove(Slot);
	    });

	if (NewHandle.IsValid())
	{
		PendingLoopVFXLoads.Add(Slot, NewHandle);
	}
}

void UGS_SkillComp::Multicast_StopLoopVFX_Implementation(ESkillSlot Slot)
{
	// 데디케이티드 서버에서는 처리하지 않음
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 로딩 중인 것도 취소
	if (FAsyncLoadHandle* Handle = PendingLoopVFXLoads.Find(Slot))
	{
		Handle->Get()->CancelHandle();
		PendingLoopVFXLoads.Remove(Slot);
	}

	// 해당 슬롯의 Loop VFX 컴포넌트 찾기
	if (UNiagaraComponent** FoundComponent = ActiveLoopVFXComponents.Find(Slot))
	{
		if (*FoundComponent && IsValid(*FoundComponent))
		{
			(*FoundComponent)->Deactivate();
			(*FoundComponent)->DestroyComponent();
		}
		ActiveLoopVFXComponents.Remove(Slot);
	}
}

void UGS_SkillComp::Multicast_StopCastVFX_Implementation(ESkillSlot Slot)
{
	if (UGS_SkillBase* Skill = GetSkillFromSkillMap(Slot))
	{
		Skill->Internal_StopCastVFX();
	}
}

void UGS_SkillComp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGS_SkillComp, ReplicatedSkillStates);
	DOREPLIFETIME(UGS_SkillComp, bCanUseSkill);
	DOREPLIFETIME(UGS_SkillComp, ReplicatedCooldownStates);
}

void UGS_SkillComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 1. 타이머 먼저 정리 (Super 호출 전에 World 접근)
	if (GetWorld())
	{
		for (auto& Pair : CooldownStates)
		{
			FSkillCooldownState& State = Pair.Value;
			GetWorld()->GetTimerManager().ClearTimer(State.CooldownTimer);
			GetWorld()->GetTimerManager().ClearTimer(State.UIUpdateTimer);
		}
	}

	// 2. 델리게이트 정리
	OnSkillCooldownChanged.Clear();
	OnHealCountChanged.Clear();
	OnSkillActivated.Clear();
	OnSkillCooldownBlocked.Clear();
	OnSkillCooldownReady.Clear();

	// 3. 마지막에 Super 호출
	Super::EndPlay(EndPlayReason);
}
