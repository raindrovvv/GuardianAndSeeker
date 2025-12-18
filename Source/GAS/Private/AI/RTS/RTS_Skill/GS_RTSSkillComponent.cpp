// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/RTS/RTS_Skill/GS_RTSSkillComponent.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillBase.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillData.h"
#include "AI/RTS/GS_RTSController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UGS_RTSSkillComponent::UGS_RTSSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 기본 에테르 설정
	MaxAether = 100.f;
	InitialAether = 100.f;
	CurrentAether = 100.f;
	AetherRegenRate = 2.f;  // 초당 2 회복

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
	CurrentAether = InitialAether;

	// 스킬 초기화
	InitializeSkills();

	// 에테르 회복 시작 (서버에서만)
	if (GetOwnerRole() == ROLE_Authority)
	{
		StartAetherRegen();
	}

	// 초기 UI 업데이트
	OnAetherChanged.Broadcast(CurrentAether, MaxAether);
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

	DOREPLIFETIME(UGS_RTSSkillComponent, CurrentAether);
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
}

void UGS_RTSSkillComponent::StartAetherRegen()
{
	if (AetherRegenRate > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			AetherRegenTimer,
			this,
			&UGS_RTSSkillComponent::RegenAether,
			0.5f,  // 0.5초마다 회복
			true
		);
	}
}

void UGS_RTSSkillComponent::RegenAether()
{
	if (CurrentAether < MaxAether)
	{
		float RegenAmount = AetherRegenRate * 0.5f;  // 0.5초 간격이므로 절반
		AddAether(RegenAmount);
	}
}

void UGS_RTSSkillComponent::OnRep_CurrentAether()
{
	OnAetherChanged.Broadcast(CurrentAether, MaxAether);
}

bool UGS_RTSSkillComponent::ConsumeAether(float Amount)
{
	if (Amount <= 0.f)
	{
		return true;
	}

	if (CurrentAether >= Amount)
	{
		CurrentAether -= Amount;
		OnAetherChanged.Broadcast(CurrentAether, MaxAether);
		return true;
	}

	return false;
}

void UGS_RTSSkillComponent::AddAether(float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}

	CurrentAether = FMath::Min(CurrentAether + Amount, MaxAether);
	OnAetherChanged.Broadcast(CurrentAether, MaxAether);
}

void UGS_RTSSkillComponent::SetAether(float Amount)
{
	CurrentAether = FMath::Clamp(Amount, 0.f, MaxAether);
	OnAetherChanged.Broadcast(CurrentAether, MaxAether);
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
	if (CurrentAether < Skill->GetAetherCost())
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
}

void UGS_RTSSkillComponent::ExitSkillTargetingMode()
{
	if (bIsInTargetingMode)
	{
		OnSkillActivationStateChanged.Broadcast(TargetingSkillIndex, false);
		bIsInTargetingMode = false;
		TargetingSkillIndex = -1;
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
	if (!ConsumeAether(Skill->GetAetherCost()))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to consume aether for skill %d"), SkillIndex);
		return;
	}

	// 스킬 발동
	FVector ActualLocation = Skill->ActivateSkill(this, TargetLocation);

	// 쿨다운 시작
	StartSkillCooldown(SkillIndex);

	// 멀티캐스트로 모든 클라이언트에 알림
	Multicast_OnSkillActivated(SkillIndex, ActualLocation);
}

void UGS_RTSSkillComponent::Multicast_OnSkillActivated_Implementation(int32 SkillIndex, const FVector& TargetLocation)
{
	// 스킬 활성화 알림 및 VFX 재생
	if (UGS_RTSSkillBase* Skill = GetSkill(SkillIndex))
	{
		Skill->PlayCastEffects(TargetLocation);
	}
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
	for (int32 i = 0; i < Skills.Num(); ++i)
	{
		UGS_RTSSkillBase* Skill = Skills[i];
		if (Skill)
		{
			UE_LOG(LogTemp, Log, TEXT("Skill %d: %s - Cost: %.0f, CD: %.1f/%.1f"), 
				i, *Skill->GetSkillName().ToString(), 
				Skill->GetAetherCost(),
				GetSkillCooldownRemaining(i),
				Skill->GetCooldownTime());
		}
	}
}
