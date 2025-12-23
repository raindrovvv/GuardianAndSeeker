// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Skill/Seeker/Merci/GS_MerciUltimateSkill.h"
#include "Character/Player/Seeker/GS_Merci.h"
#include "DrawDebugHelpers.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Player/Guardian/GS_Guardian.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/GS_SeekerAudioComponent.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"

UGS_MerciUltimateSkill::UGS_MerciUltimateSkill()
{
	CurrentSkillType = ESkillSlot::Ultimate;
	AllowSkillsMask = -9;
}

void UGS_MerciUltimateSkill::ActiveSkill()
{
	Super::ActiveSkill();

	// 쿨타임 측정 시작
	StartCoolDown();

	// 소유자 캐싱 (매번 Cast를 피하기 위해)
	CachedMerciOwner = Cast<AGS_Merci>(OwnerCharacter);

	if(CachedMerciOwner.IsValid())
	{
		const FSkillInfo* SkillInfo = GetCurrentSkillInfo();
		
		// 스킬 시작 사운드 재생 (멀티캐스트)
		if (CachedMerciOwner->HasAuthority())
		{
			if (UGS_SeekerAudioComponent* AudioComp = CachedMerciOwner->SeekerAudioComponent)
			{
				AudioComp->RequestSkillAudio(CurrentSkillType, 0);
			}
		}

		CachedMerciOwner->SetAutoAimTarget(nullptr);
		CachedMerciOwner->Client_StartZoom();

		// 타이머 설정
		CachedMerciOwner->GetWorldTimerManager().SetTimer(AutoAimingHandle, this, &UGS_MerciUltimateSkill::DeactiveSkill, AutoAimingStateTime, false);
		CachedMerciOwner->GetWorldTimerManager().SetTimer(AutoAimTickHandle, this, &UGS_MerciUltimateSkill::TickAutoAimTarget, AutoAimTickInterval, true);

		// 입력 제한 설정
		CachedMerciOwner->SetSkillInputControl(true, true, false, false);
	}

	// 몬스터 리스트 업데이트
	UpdateMonsterList();

	// 자동 조준 시작
	AutoAimingStart();
}

void UGS_MerciUltimateSkill::OnSkillAnimationEnd()
{
}

void UGS_MerciUltimateSkill::InterruptSkill()
{
	Super::InterruptSkill();
	// 궁극기는 InterruptSkill로 중단되지 않음 (AutoAimingHandle 타이머가 끝날 때까지 유지)
	// DeactiveSkill은 타이머 종료 시에만 호출됨
}

void UGS_MerciUltimateSkill::AutoAimingStart()
{
	// 타겟 찾기
	AActor* Target = FindCloseTarget();

	// 타겟 설정
	if (Target && CachedMerciOwner.IsValid() && CachedMerciOwner->HasAuthority())
	{
		CachedMerciOwner->SetAutoAimTarget(Target);
	}
}

AActor* UGS_MerciUltimateSkill::FindCloseTarget()
{
	if (!CachedMerciOwner.IsValid())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	
	AController* Controller = CachedMerciOwner->GetController();
	if (!Controller) 
	{
		return nullptr;
	}

	FVector CamLoc;
	FRotator CamRot;
	Controller->GetPlayerViewPoint(CamLoc, CamRot);
	FVector ViewDir = CamRot.Vector();

	float CloseDot = 0.8f; // 최소 허용 Dot
	AActor* BestTarget = nullptr;

	TArray<AActor*> HostileActors;
	UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>();
	if (!Registry)
	{
		return nullptr;
	}
	Registry->GetAllHostileActors(HostileActors);

	for (AActor* Target : HostileActors)
	{
		if (!IsValid(Target)) continue;

		// 죽은 타겟은 건너뛰기
		if (AGS_Character* CharacterTarget = Cast<AGS_Character>(Target))
		{
			if (CharacterTarget->IsDead()) continue;
		}

		FVector ToTarget = (Target->GetActorLocation() - CamLoc).GetSafeNormal();
		float Dot = FVector::DotProduct(ViewDir, ToTarget);

		if (Dot > CloseDot)
		{
			// LineTrace로 시야 확인
			FHitResult Hit;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(CachedMerciOwner.Get());

			bool bHit = World->LineTraceSingleByChannel(
				Hit,
				CamLoc,
				Target->GetActorLocation(),
				ECC_Visibility,
				Params
			);

			// 벽에 가려져 있다면 무시
			if (bHit && Hit.GetActor() != Target)
			{
				continue;
			}

			// 타겟 선택
			CloseDot = Dot;
			BestTarget = Target;
		}
	}

	return BestTarget;
}

void UGS_MerciUltimateSkill::TickAutoAimTarget()
{
	if (!CachedMerciOwner.IsValid()) 
	{
		return;
	}

	if (!GetIsActive())
	{
		return;
	}

	// 가까운 타겟 찾기
	AActor* NewTarget = FindCloseTarget();

	if (CachedMerciOwner->HasAuthority())
	{
		// 타겟 설정
		CachedMerciOwner->SetAutoAimTarget(NewTarget);

		if (NewTarget != CurrentTarget)
		{
			// 플레이어를 통해 클라이언트로 전송
			CachedMerciOwner->Client_UpdateTargetUI(NewTarget, CurrentTarget);
			CurrentTarget = NewTarget;
		}
	}
}

void UGS_MerciUltimateSkill::UpdateMonsterList()
{
	// RegistrySubsystem을 사용하므로 더 이상 개별적으로 리스트를 수집할 필요가 없음
}

void UGS_MerciUltimateSkill::DeactiveSkill()
{
	if (CachedMerciOwner.IsValid())
	{
		// 현재 표시된 타겟 UI 정리
		if (CurrentTarget && CachedMerciOwner->HasAuthority())
		{
			CachedMerciOwner->Client_UpdateTargetUI(nullptr, CurrentTarget);
			CurrentTarget = nullptr;
		}

		// 줌 아웃
		CachedMerciOwner->Client_StopZoom(0.f);

		// 타이머 정리
		CachedMerciOwner->GetWorldTimerManager().ClearTimer(AutoAimTickHandle);
		CachedMerciOwner->GetWorldTimerManager().ClearTimer(AutoAimingHandle);

		// 스킬 Input 수정
		CachedMerciOwner->SetSkillInputControl(true, true, true);

		// 스킬 종료 사운드 재생 (멀티캐스트)
		if (CachedMerciOwner->HasAuthority())
		{
			if (UGS_SeekerAudioComponent* AudioComp = CachedMerciOwner->SeekerAudioComponent)
			{
				AudioComp->RequestSkillAudio(CurrentSkillType, 1);
			}
		}
	}

	// 스킬 상태 업데이트
	Super::DeactiveSkill();
}
