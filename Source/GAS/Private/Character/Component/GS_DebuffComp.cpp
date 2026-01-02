// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Component/GS_DebuffComp.h"
#include "Character/Debuff/DebuffData.h"
#include "Character/GS_Character.h"
#include "Character/Debuff/GS_DebuffBase.h"
#include "Net/UnrealNetwork.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Player/Guardian/GS_Guardian.h"
#include "Character/Component/GS_VFXComponent.h"
#include "Character/Component/GS_DrakharVFXComponent.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Skill/GS_SkillComp.h"


UGS_DebuffComp::UGS_DebuffComp()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void UGS_DebuffComp::BeginPlay()
{
	Super::BeginPlay();

	// VFX 컴포넌트 캐싱
	if (GetOwner())
	{
		CachedDrakharVFXComp = GetOwner()->FindComponentByClass<UGS_DrakharVFXComponent>();
		CachedVFXComp = GetOwner()->FindComponentByClass<UGS_VFXComponent>();
	}
}

void UGS_DebuffComp::ApplyDebuff(EDebuffType Type, AActor* Attacker)
{
	if (!GetOwner()->HasAuthority())
	{
		Server_ApplyDebuff(Type, Attacker);
		return;
	}

	// 궁극기 사용 중이거나 무적 상태일 때는 디버프 무시 (슈퍼아머 효과)
	if (AGS_Character* OwnerChar = Cast<AGS_Character>(GetOwner()))
	{
		// 1. 무적 상태 확인
		if (OwnerChar->IsInvincible())
		{
			return;
		}

		// 2. 시커의 경우 궁극기 사용 중인지 확인
		if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(OwnerChar))
		{
			if (Seeker->GetSkillComp() && Seeker->GetSkillComp()->IsSkillActive(ESkillSlot::Ultimate))
			{
				return;
			}
		}
	}

	// 해당 디버프 타입의 데이터 가져오기
	const FDebuffData* Row = GetDebuffData(Type);
	if (!Row || !Row->DebuffClass)
		return;

	// 이미 적용중인 디버프라면
	UGS_DebuffBase* Existing = GetActiveDebuff(Type);

	if (Existing)
	{
		Existing->StartTime = GetWorld()->GetTimeSeconds(); // 시작 시간 재저장
		RefreshDebuffTimer(Existing, Row->Duration);
		UpdateReplicatedDebuffList(); // 복제 정보 갱신


		// ===============================
		// VFX 트리거 (기존 디버프 갱신 시에도)
		// ===============================
		TriggerDebuffVFX(Type);

		return;
	}

	// 적용중이 아닌 디버프라면 풀에서 꺼내거나 생성
	UGS_DebuffBase* NewDebuff = GetOrCreateDebuffObject(Type, Row->DebuffClass);
	if (!NewDebuff)
		return;

	NewDebuff->Initialize(Cast<AGS_Character>(GetOwner()), Attacker, Row->Duration, Row->Priority, Row->Damage, Row->DamageInterval, Type);
	NewDebuff->StartTime = GetWorld()->GetTimeSeconds();

	// 우선순위와 관련 없다면
	if (Row->bIsConcurrent)
	{
		CreateAndApplyConcurrentDebuff(NewDebuff);
	}
	else // 우선순위가 관련있다면
	{
		AddDebuffToQueue(NewDebuff);
	}
	UpdateReplicatedDebuffList(); // 복제 정보 갱신

	// ===============================
	// VFX 트리거 (새로운 디버프 적용 시)
	// ===============================
	TriggerDebuffVFX(Type);
}

void UGS_DebuffComp::RemoveDebuff(EDebuffType Type)
{
	if (!GetOwner()->HasAuthority())
	{
		Server_RemoveDebuff(Type);
		return;
	}

	UGS_DebuffBase* Debuff = GetActiveDebuff(Type);
	if (!Debuff)
	{
		return;
	}

	if (FTimerHandle* Handle = DebuffTimers.Find(Debuff))
	{
		GetWorld()->GetTimerManager().ClearTimer(*Handle);
		DebuffTimers.Remove(Debuff);
	}

	// 디버프 제거 전 만료 VFX 재생
	TriggerDebuffExpireVFX(Type);

	if (ConcurrentDebuffs.Contains(Debuff))
	{
		ConcurrentDebuffs.Remove(Debuff);
	}
	else if (DebuffQueue.Contains(Debuff))
	{
		DebuffQueue.Remove(Debuff);
	}
	else if (Debuff == CurrentDebuff)
	{
		CurrentDebuff = nullptr;
		ApplyNextDebuff();
	}

	Debuff->OnExpire();
	// 디버프 VFX 제거
	RemoveDebuffVFX(Type);

	ReturnDebuffToPool(Debuff); // 풀에 반환

	UpdateReplicatedDebuffList();
}

bool UGS_DebuffComp::IsDebuffActive(EDebuffType Type)
{
	return GetActiveDebuff(Type) != nullptr;
}

void UGS_DebuffComp::OnRep_DebuffList()
{
	OnDebuffListUpdated.Broadcast(ReplicatedDebuffs);
}

void UGS_DebuffComp::ClearAllDebuffs()
{
	if (!GetOwner()->HasAuthority())
	{
		Server_ClearAllDebuffs();
		return;
	}

	// 타이머 먼저 제거
	for (auto& Elem : DebuffTimers)
	{
		GetWorld()->GetTimerManager().ClearTimer(Elem.Value);
	}

	// 이후 안전하게 OnExpire 호출 및 VFX 제거
	for (auto& Elem : DebuffTimers)
	{
		if (Elem.Key && IsValid(Elem.Key))
		{
			// 디버프 VFX 제거 (만료 VFX는 재생하지 않음 - 사망 시)
			RemoveDebuffVFX(Elem.Key->GetDebuffType());

			Elem.Key->OnExpire();
			ReturnDebuffToPool(Elem.Key); // 풀에 반환
		}
	}
	DebuffTimers.Empty();

	// 모든 디버프 컨테이너 초기화
	ConcurrentDebuffs.Empty();
	DebuffQueue.Empty();
	CurrentDebuff = nullptr;

	// 복제 목록 갱신
	UpdateReplicatedDebuffList();
}

void UGS_DebuffComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnDebuffListUpdated.Clear();
	Super::EndPlay(EndPlayReason);
	ClearAllDebuffs();
}

const FDebuffData* UGS_DebuffComp::GetDebuffData(EDebuffType Type) const
{
	if (!DebuffDataTable)
		return nullptr;

	// 디버프 데이터 Row 반환
	FName RowName = *UEnum::GetValueAsString(Type).RightChop(13);
	return DebuffDataTable->FindRow<FDebuffData>(RowName, TEXT(""));
}

UGS_DebuffBase* UGS_DebuffComp::GetActiveDebuff(EDebuffType Type) const
{
	// ConcurrentDebuffs 내에 존재하는지 확인
	for (UGS_DebuffBase* Debuff : ConcurrentDebuffs)
	{
		if (Debuff && Debuff->GetDebuffType() == Type)
			return Debuff;
	}

	// CurrentDebuff로 존재하는지 확인
	if (CurrentDebuff && CurrentDebuff->GetDebuffType() == Type)
	{
		return CurrentDebuff;
	}

	// DebuffQueue 내에 존재하는지 확인
	for (UGS_DebuffBase* Debuff : DebuffQueue)
	{
		if (Debuff && Debuff->GetDebuffType() == Type)
			return Debuff;
	}

	// 존재하지 않음
	return nullptr;
}

void UGS_DebuffComp::RefreshDebuffTimer(UGS_DebuffBase* Debuff, float Duration)
{
	if (FTimerHandle* FoundHandle = DebuffTimers.Find(Debuff))
	{
		GetWorld()->GetTimerManager().ClearTimer(*FoundHandle);

		TWeakObjectPtr<UGS_DebuffBase> WeakDebuff(Debuff);

		GetWorld()->GetTimerManager().SetTimer(*FoundHandle, [this, WeakDebuff]()
		                                       {
			                                       if (!IsValid(WeakDebuff.Get()))
				                                       return;
			                                       UGS_DebuffBase* ValidDebuff = WeakDebuff.Get();

			                                       // 디버프 만료 VFX 재생
			                                       TriggerDebuffExpireVFX(ValidDebuff->GetDebuffType());

			                                       // 디버프 VFX 제거
			                                       RemoveDebuffVFX(ValidDebuff->GetDebuffType());


			                                       DebuffTimers.Remove(ValidDebuff);
			                                       if (ConcurrentDebuffs.Contains(ValidDebuff))
			                                       {
				                                       ConcurrentDebuffs.Remove(ValidDebuff);
			                                       }
			                                       else if (DebuffQueue.Contains(ValidDebuff))
			                                       {
				                                       DebuffQueue.Remove(ValidDebuff);
			                                       }
			                                       else if (ValidDebuff == CurrentDebuff)
			                                       {
				                                       CurrentDebuff = nullptr;
				                                       ApplyNextDebuff();
			                                       }
			                                       UpdateReplicatedDebuffList();

			                                       ValidDebuff->OnExpire();
			                                       ReturnDebuffToPool(ValidDebuff); // 풀에 반환
		                                       },
		                                       Duration, false);
	}
}

void UGS_DebuffComp::CreateAndApplyConcurrentDebuff(UGS_DebuffBase* Debuff)
{
	ConcurrentDebuffs.Add(Debuff);
	Debuff->OnApply();

	FTimerHandle Handle;
	TWeakObjectPtr<UGS_DebuffBase> WeakDebuff(Debuff);

	GetWorld()->GetTimerManager().SetTimer(Handle, [this, WeakDebuff]()
	                                       {
		                                       if (!IsValid(WeakDebuff.Get()))
		                                       {
			                                       return;
		                                       }
		                                       UGS_DebuffBase* ValidDebuff = WeakDebuff.Get();

		                                       // 디버프 만료 VFX 재생 (Concurrent 디버프용)
		                                       TriggerDebuffExpireVFX(ValidDebuff->GetDebuffType());

		                                       // 디버프 VFX 제거
		                                       RemoveDebuffVFX(ValidDebuff->GetDebuffType());

		                                       ConcurrentDebuffs.Remove(ValidDebuff);
		                                       DebuffTimers.Remove(ValidDebuff);
		                                       UpdateReplicatedDebuffList();
		                                       ValidDebuff->OnExpire();
		                                       ReturnDebuffToPool(ValidDebuff); // 풀에 반환
	                                       },
	                                       Debuff->GetDuration(), false);

	DebuffTimers.Add(Debuff, Handle);
}

void UGS_DebuffComp::AddDebuffToQueue(UGS_DebuffBase* Debuff)
{
	// 현재 디버프가 없으면 단순 큐 처리
	if (CurrentDebuff)
	{
		if (Debuff->GetPriority() > CurrentDebuff->GetPriority())
		{

			// 현재 디버프의 타이머 제거
			if (FTimerHandle* FoundHandle = DebuffTimers.Find(CurrentDebuff))
			{
				GetWorld()->GetTimerManager().ClearTimer(*FoundHandle);
				DebuffTimers.Remove(CurrentDebuff);
			}

			CurrentDebuff->OnExpire();

			// 현재 디버프의 남은 시간 저장
			float RemainingTime = CurrentDebuff->GetRemainingTime(GetWorld()->GetTimeSeconds());

			// 큐로 재삽입
			DebuffQueue.Add(CurrentDebuff);

			// 타이머 재설정
			FTimerHandle NewHandle;
			TWeakObjectPtr<UGS_DebuffBase> WeakDebuff(CurrentDebuff);
			GetWorld()->GetTimerManager().SetTimer(NewHandle, [this, WeakDebuff]()
			                                       {
				                                       if (!IsValid(WeakDebuff.Get()))
					                                       return;
				                                       UGS_DebuffBase* ValidDebuff = WeakDebuff.Get();
				                                       DebuffQueue.Remove(ValidDebuff);
				                                       DebuffTimers.Remove(ValidDebuff);
				                                       UpdateReplicatedDebuffList();
				                                       ReturnDebuffToPool(ValidDebuff); // 풀에 반환
			                                       },
			                                       RemainingTime, false);

			DebuffTimers.Add(CurrentDebuff, NewHandle);
			CurrentDebuff = nullptr;
			UpdateReplicatedDebuffList();
		}
	}

	DebuffQueue.Add(Debuff);

	// 우선순위 정렬
	DebuffQueue.Sort([](const UGS_DebuffBase& A, const UGS_DebuffBase& B)
	                 { return A.GetPriority() > B.GetPriority(); });

	// 디버프 만료 타이머 설정
	FTimerHandle Handle;
	TWeakObjectPtr<UGS_DebuffBase> WeakDebuff(Debuff);
	GetWorld()->GetTimerManager().SetTimer(Handle, [this, WeakDebuff]()
	                                       {
		                                       if (!IsValid(WeakDebuff.Get()))
			                                       return;
		                                       UGS_DebuffBase* ValidDebuff = WeakDebuff.Get();

		                                       // 디버프 만료 VFX 재생 (Queue 디버프용)
		                                       TriggerDebuffExpireVFX(ValidDebuff->GetDebuffType());

		                                       // 디버프 VFX 제거
		                                       RemoveDebuffVFX(ValidDebuff->GetDebuffType());

		                                       DebuffQueue.Remove(ValidDebuff);
		                                       DebuffTimers.Remove(ValidDebuff);
		                                       UpdateReplicatedDebuffList();
		                                       ReturnDebuffToPool(ValidDebuff); // 풀에 반환
	                                       },
	                                       Debuff->GetDuration(), false);

	DebuffTimers.Add(Debuff, Handle);

	// 현재 디버프가 없는 경우에만 다음 디버프 적용
	if (!CurrentDebuff)
	{
		ApplyNextDebuff();
	}
}

void UGS_DebuffComp::ApplyNextDebuff()
{
	// 디버프 큐에 없으면 리턴
	if (DebuffQueue.Num() == 0)
		return;

	// 현재 디버프 업데이트
	CurrentDebuff = DebuffQueue[0];
	DebuffQueue.RemoveAt(0);

	// 현재 디버프 효과 실행
	CurrentDebuff->OnApply();

	// 기존 타이머가 있으면 제거
	if (FTimerHandle* FoundHandle = DebuffTimers.Find(CurrentDebuff))
	{
		GetWorld()->GetTimerManager().ClearTimer(*FoundHandle);
	}

	// 새 타이머 핸들 생성
	FTimerHandle NewHandle;
	TWeakObjectPtr<UGS_DebuffBase> WeakDebuff(CurrentDebuff);
	// 디버프 남은 시간 받기
	float Remaining = CurrentDebuff->GetRemainingTime(GetWorld()->GetTimeSeconds());

	// 남은 시간으로 다시 타이머 세팅
	GetWorld()->GetTimerManager().SetTimer(NewHandle, [this, WeakDebuff]()
	                                       {
			if (!IsValid(WeakDebuff.Get())) return;
			UGS_DebuffBase* ValidDebuff = WeakDebuff.Get();
			
			// 디버프 만료 VFX 재생 (Current 디버프용)
			TriggerDebuffExpireVFX(ValidDebuff->GetDebuffType());

			// 디버프 VFX 제거
			RemoveDebuffVFX(ValidDebuff->GetDebuffType());
			
			DebuffTimers.Remove(ValidDebuff);
			if (CurrentDebuff == ValidDebuff)
			{
				CurrentDebuff = nullptr;
			}
			UpdateReplicatedDebuffList();
			ValidDebuff->OnExpire();
			ReturnDebuffToPool(ValidDebuff); // 풀에 반환
			ApplyNextDebuff(); }, Remaining, false);

	DebuffTimers.Add(CurrentDebuff, NewHandle);
}

void UGS_DebuffComp::UpdateReplicatedDebuffList()
{
	ReplicatedDebuffs.Empty();
	float Now = GetWorld()->GetTimeSeconds();

	// Concurrent 디버프 추가
	for (UGS_DebuffBase* Debuff : ConcurrentDebuffs)
	{
		if (!Debuff)
			continue;
		FDebuffRepInfo Info;
		Info.Type = Debuff->GetDebuffType();
		Info.RemainingTime = Debuff->GetRemainingTime(Now);
		ReplicatedDebuffs.Add(Info);
	}

	// CurrentDebuff 디버프도 추가
	if (CurrentDebuff)
	{
		FDebuffRepInfo Info;
		Info.Type = CurrentDebuff->GetDebuffType();
		Info.RemainingTime = CurrentDebuff->GetRemainingTime(Now);
		ReplicatedDebuffs.Add(Info);
	}

	// DebuffQueue의 디버프 추가
	for (UGS_DebuffBase* Debuff : DebuffQueue)
	{
		if (!Debuff)
			continue;
		FDebuffRepInfo Info;
		Info.Type = Debuff->GetDebuffType();
		Info.RemainingTime = Debuff->GetRemainingTime(Now);
		ReplicatedDebuffs.Add(Info);
	}
}

void UGS_DebuffComp::Server_ApplyDebuff_Implementation(EDebuffType Type, AActor* Attacker)
{
	ApplyDebuff(Type, Attacker);
}

void UGS_DebuffComp::Server_RemoveDebuff_Implementation(EDebuffType Type)
{
	RemoveDebuff(Type);
}

void UGS_DebuffComp::Server_ClearAllDebuffs_Implementation()
{
	ClearAllDebuffs();
}

void UGS_DebuffComp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGS_DebuffComp, ReplicatedDebuffs);
}

void UGS_DebuffComp::TriggerDebuffVFX(EDebuffType Type)
{
	// 서버에서만 실행
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// 캐싱된 컴포넌트 사용
	if (CachedDrakharVFXComp)
	{
		CachedDrakharVFXComp->PlayDebuffVFX(Type);
	}
	else if (CachedVFXComp)
	{
		CachedVFXComp->PlayDebuffVFX(Type);
	}
}

void UGS_DebuffComp::RemoveDebuffVFX(EDebuffType Type)
{
	if (!GetOwner()->HasAuthority())
		return;

	if (CachedDrakharVFXComp)
	{
		CachedDrakharVFXComp->RemoveDebuffVFX(Type);
	}
	else if (CachedVFXComp)
	{
		CachedVFXComp->RemoveDebuffVFX(Type);
	}
}

void UGS_DebuffComp::TriggerDebuffExpireVFX(EDebuffType Type)
{
	// 서버에서만 실행
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (CachedDrakharVFXComp)
	{
		CachedDrakharVFXComp->PlayDebuffExpireVFX(Type);
	}
	else if (CachedVFXComp)
	{
		CachedVFXComp->PlayDebuffExpireVFX(Type);
	}
}

UGS_DebuffBase* UGS_DebuffComp::GetOrCreateDebuffObject(EDebuffType Type, TSubclassOf<UGS_DebuffBase> DebuffClass)
{
	if (!DebuffClass)
		return nullptr;

	// 풀에서 해당 타입의 객체가 있는지 확인
	if (UGS_DebuffBase** PooledObject = DebuffPool.Find(Type))
	{
		if (*PooledObject && IsValid(*PooledObject))
		{
			UGS_DebuffBase* Debuff = *PooledObject;
			DebuffPool.Remove(Type);
			return Debuff;
		}
	}

	// 없으면 새로 생성
	return NewObject<UGS_DebuffBase>(this, DebuffClass);
}

void UGS_DebuffComp::ReturnDebuffToPool(UGS_DebuffBase* Debuff)
{
	if (!Debuff || !IsValid(Debuff))
		return;

	// 이미 동일한 타입의 객체가 풀에 있으면 무시하거나 교체 (시스템상 한 타입당 하나가 보통)
	DebuffPool.Add(Debuff->GetDebuffType(), Debuff);
}
