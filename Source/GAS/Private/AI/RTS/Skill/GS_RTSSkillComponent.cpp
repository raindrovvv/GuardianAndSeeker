// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/RTS/Skill/GS_RTSSkillComponent.h"
#include "AI/RTS/Skill/GS_RTSSkillBase.h"
#include "AI/RTS/Skill/GS_RTSSkillData.h"
#include "AI/RTS/GS_RTSController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UGS_RTSSkillComponent::UGS_RTSSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 기본 에테르 설정
	MaxEther = 100.f;
	InitialEther = 100.f;
	CurrentEther = 100.f;
	EtherRegenRate = 2.f;  // 초당 2 회복

	// 타겟팅 모드 초기화
	bIsInTargetingMode = false;
	TargetingSkillIndex = -1;

	// 네트워크 복제 설정
	SetIsReplicatedByDefault(true);
}

void UGS_RTSSkillComponent::BeginPlay()
{
	Super::BeginPlay();

	// 초기 에테르 설정
	CurrentEther = InitialEther;

	// 스킬 초기화
	InitializeSkills();

	// 에테르 회복 시작 (서버에서만)
	if (GetOwnerRole() == ROLE_Authority)
	{
		StartEtherRegen();
	}

	// 초기 UI 업데이트
	OnEtherChanged.Broadcast(CurrentEther, MaxEther);
}

void UGS_RTSSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 쿨다운 업데이트
	for (int32 i = 0; i < CooldownRemaining.Num(); ++i)
	{
		if (CooldownRemaining[i] > 0.f)
		{
			CooldownRemaining[i] = FMath::Max(0.f, CooldownRemaining[i] - DeltaTime);
			UGS_RTSSkillBase* Skill = Skills.IsValidIndex(i) ? Skills[i] : nullptr;
			const float MaxCooldown = Skill ? Skill->GetCooldownTime() : 0.f;
			OnSkillCooldownChanged.Broadcast(i, CooldownRemaining[i], MaxCooldown);
		}
	}
}

void UGS_RTSSkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGS_RTSSkillComponent, CurrentEther);
}

void UGS_RTSSkillComponent::InitializeSkills()
{
	const int32 SkillCount = SkillDataAssets.Num();
	Skills.SetNum(SkillCount);
	CooldownTimers.SetNum(SkillCount);
	CooldownRemaining.SetNum(SkillCount);

	for (int32 i = 0; i < SkillCount; ++i)
	{
		CooldownTimers[i] = FTimerHandle();
		CooldownRemaining[i] = 0.f;

		UGS_RTSSkillData* SkillData = SkillDataAssets[i];
		if (!SkillData)
		{
			Skills[i] = nullptr;
			UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkillComponent: Skill data asset is null at index %d"), i);
			continue;
		}

		if (!SkillData->SkillClass)
		{
			Skills[i] = nullptr;
			UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkillComponent: Skill data %s has no SkillClass set"), *SkillData->GetName());
			continue;
		}

		UGS_RTSSkillBase* NewSkill = NewObject<UGS_RTSSkillBase>(this, SkillData->SkillClass);
		if (!NewSkill)
		{
			Skills[i] = nullptr;
			UE_LOG(LogTemp, Warning, TEXT("UGS_RTSSkillComponent: Failed to create skill instance for %s"), *SkillData->GetName());
			continue;
		}

		NewSkill->SetSkillData(SkillData);
		NewSkill->Initialize(this);
		NewSkill->SetSkillSlotIndex(i);
		Skills[i] = NewSkill;
	}

	int32 ValidSkillCount = 0;
	for (UGS_RTSSkillBase* Skill : Skills)
	{
		if (Skill)
		{
			++ValidSkillCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("UGS_RTSSkillComponent: Initialized %d / %d skills"), ValidSkillCount, SkillCount);
}

void UGS_RTSSkillComponent::StartEtherRegen()
{
	if (EtherRegenRate > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			EtherRegenTimer,
			this,
			&UGS_RTSSkillComponent::RegenEther,
			0.5f,  // 0.5초마다 회복
			true
		);
	}
}

void UGS_RTSSkillComponent::RegenEther()
{
	if (CurrentEther < MaxEther)
	{
		float RegenAmount = EtherRegenRate * 0.5f;  // 0.5초 간격이므로 절반
		AddEther(RegenAmount);
	}
}

void UGS_RTSSkillComponent::OnRep_CurrentEther()
{
	OnEtherChanged.Broadcast(CurrentEther, MaxEther);
}

bool UGS_RTSSkillComponent::ConsumeEther(float Amount)
{
	if (Amount <= 0.f)
	{
		return true;
	}

	if (CurrentEther >= Amount)
	{
		CurrentEther -= Amount;
		OnEtherChanged.Broadcast(CurrentEther, MaxEther);
		return true;
	}

	return false;
}

void UGS_RTSSkillComponent::AddEther(float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}

	CurrentEther = FMath::Min(CurrentEther + Amount, MaxEther);
	OnEtherChanged.Broadcast(CurrentEther, MaxEther);
}

void UGS_RTSSkillComponent::SetEther(float Amount)
{
	CurrentEther = FMath::Clamp(Amount, 0.f, MaxEther);
	OnEtherChanged.Broadcast(CurrentEther, MaxEther);
}

bool UGS_RTSSkillComponent::TryActivateSkill(int32 SkillIndex)
{
	if (!CanActivateSkill(SkillIndex))
	{
		return false;
	}

	UGS_RTSSkillBase* Skill = GetSkill(SkillIndex);
	if (!Skill)
	{
		return false;
	}

	// 타겟팅이 필요한 스킬인 경우 타겟팅 모드 진입
	ERTSSkillTargetType TargetType = Skill->GetTargetType();
	if (TargetType == ERTSSkillTargetType::Location || TargetType == ERTSSkillTargetType::Actor)
	{
		EnterSkillTargetingMode(SkillIndex);
		return true;
	}

	// 즉시 발동 스킬
	Server_ActivateSkill(SkillIndex, FVector::ZeroVector);
	return true;
}

bool UGS_RTSSkillComponent::CanActivateSkill(int32 SkillIndex) const
{
	UGS_RTSSkillBase* Skill = GetSkill(SkillIndex);
	if (!Skill)
	{
		return false;
	}

	// 쿨다운 체크
	if (IsSkillOnCooldown(SkillIndex))
	{
		return false;
	}

	// 에테르 체크
	if (CurrentEther < Skill->GetEtherCost())
	{
		return false;
	}

	// 스킬 자체의 활성화 조건 체크
	return Skill->CanActivate(const_cast<UGS_RTSSkillComponent*>(this));
}

UGS_RTSSkillBase* UGS_RTSSkillComponent::GetSkill(int32 SkillIndex) const
{
	if (Skills.IsValidIndex(SkillIndex))
	{
		return Skills[SkillIndex];
	}
	return nullptr;
}

float UGS_RTSSkillComponent::GetSkillCooldownRemaining(int32 SkillIndex) const
{
	if (CooldownRemaining.IsValidIndex(SkillIndex))
	{
		return CooldownRemaining[SkillIndex];
	}
	return 0.f;
}

float UGS_RTSSkillComponent::GetSkillCooldownPercent(int32 SkillIndex) const
{
	UGS_RTSSkillBase* Skill = GetSkill(SkillIndex);
	if (!Skill || Skill->GetCooldownTime() <= 0.f)
	{
		return 0.f;
	}

	return GetSkillCooldownRemaining(SkillIndex) / Skill->GetCooldownTime();
}

bool UGS_RTSSkillComponent::IsSkillOnCooldown(int32 SkillIndex) const
{
	return GetSkillCooldownRemaining(SkillIndex) > 0.f;
}

void UGS_RTSSkillComponent::EnterSkillTargetingMode(int32 SkillIndex)
{
	if (!Skills.IsValidIndex(SkillIndex))
	{
		return;
	}

	bIsInTargetingMode = true;
	TargetingSkillIndex = SkillIndex;
	OnSkillActivationStateChanged.Broadcast(SkillIndex, true);

	UE_LOG(LogTemp, Log, TEXT("Entered skill targeting mode for skill %d"), SkillIndex);
}

void UGS_RTSSkillComponent::ExitSkillTargetingMode()
{
	if (bIsInTargetingMode)
	{
		OnSkillActivationStateChanged.Broadcast(TargetingSkillIndex, false);
		bIsInTargetingMode = false;
		TargetingSkillIndex = -1;

		UE_LOG(LogTemp, Log, TEXT("Exited skill targeting mode"));
	}
}

void UGS_RTSSkillComponent::ExecuteSkillAtLocation(const FVector& TargetLocation)
{
	if (!bIsInTargetingMode || !Skills.IsValidIndex(TargetingSkillIndex))
	{
		return;
	}

	int32 SkillIndexToExecute = TargetingSkillIndex;
	ExitSkillTargetingMode();

	if (CanActivateSkill(SkillIndexToExecute))
	{
		Server_ActivateSkill(SkillIndexToExecute, TargetLocation);
	}
}

void UGS_RTSSkillComponent::Server_ActivateSkill_Implementation(int32 SkillIndex, const FVector& TargetLocation)
{
	UGS_RTSSkillBase* Skill = GetSkill(SkillIndex);
	if (!Skill)
	{
		return;
	}

	// 에테르 소비
	if (!ConsumeEther(Skill->GetEtherCost()))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to consume ether for skill %d"), SkillIndex);
		return;
	}

	// 스킬 발동
	Skill->ActivateSkill(this, TargetLocation);

	// 쿨다운 시작
	StartSkillCooldown(SkillIndex);

	// 멀티캐스트로 모든 클라이언트에 알림
	Multicast_OnSkillActivated(SkillIndex);
}

void UGS_RTSSkillComponent::Multicast_OnSkillActivated_Implementation(int32 SkillIndex)
{
	UE_LOG(LogTemp, Log, TEXT("Skill %d activated"), SkillIndex);
}

void UGS_RTSSkillComponent::StartSkillCooldown(int32 SkillIndex)
{
	UGS_RTSSkillBase* Skill = GetSkill(SkillIndex);
	if (!Skill || !CooldownRemaining.IsValidIndex(SkillIndex))
	{
		return;
	}

	CooldownRemaining[SkillIndex] = Skill->GetCooldownTime();
	OnSkillCooldownChanged.Broadcast(SkillIndex, CooldownRemaining[SkillIndex], Skill->GetCooldownTime());
}

void UGS_RTSSkillComponent::OnSkillCooldownFinished(int32 SkillIndex)
{
	if (CooldownRemaining.IsValidIndex(SkillIndex))
	{
		CooldownRemaining[SkillIndex] = 0.f;
		
		UGS_RTSSkillBase* Skill = GetSkill(SkillIndex);
		if (Skill)
		{
			OnSkillCooldownChanged.Broadcast(SkillIndex, 0.f, Skill->GetCooldownTime());
		}
	}
}

void UGS_RTSSkillComponent::DebugPrintSkillStatus() const
{
	UE_LOG(LogTemp, Log, TEXT("=== RTS Skill Component Status ==="));
	UE_LOG(LogTemp, Log, TEXT("Ether: %.1f / %.1f (Regen: %.1f/s)"), CurrentEther, MaxEther, EtherRegenRate);
	
	for (int32 i = 0; i < Skills.Num(); ++i)
	{
		UGS_RTSSkillBase* Skill = Skills[i];
		if (Skill)
		{
			UE_LOG(LogTemp, Log, TEXT("Skill %d: %s - Cost: %.0f, CD: %.1f/%.1f"), 
				i, *Skill->GetSkillName().ToString(), 
				Skill->GetEtherCost(),
				GetSkillCooldownRemaining(i),
				Skill->GetCooldownTime());
		}
	}
}
