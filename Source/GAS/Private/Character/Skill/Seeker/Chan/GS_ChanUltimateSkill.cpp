// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Skill/Seeker/Chan/GS_ChanUltimateSkill.h"
#include "Character/Player/Seeker/GS_Chan.h"
#include "Character/Skill/GS_SkillSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Player/Guardian/GS_Guardian.h"
#include "Engine/StaticMeshActor.h"
#include "Character/Debuff/EDebuffType.h"
#include "Character/Component/GS_DebuffComp.h"
#include "Character/GS_TpsController.h"
#include "AIController.h"
#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Sound/GS_SeekerAudioComponent.h"


UGS_ChanUltimateSkill::UGS_ChanUltimateSkill()
{
	CurrentSkillType = ESkillSlot::Ultimate;
}

void UGS_ChanUltimateSkill::ActiveSkill()
{
	Super::ActiveSkill();


	// 쿨타임 측정 시작
	StartCoolDown();

	// 구조물 충돌 확인 변수 초기화
	bInStructureCrash = false;
	bIsCharging = false; // 돌진은 StartCharge에서 시작

	// 무적 설정
	OwnerCharacter->SetInvincible(true);

	// 소유자 캐싱
	CachedChanOwner = Cast<AGS_Chan>(OwnerCharacter);

	if (CachedChanOwner.IsValid())
	{
		if (UAnimInstance* AnimInstance = CachedChanOwner->GetMesh()->GetAnimInstance())
		{
			AnimInstance->OnMontageEnded.AddUniqueDynamic(this, &UGS_ChanUltimateSkill::OnMontageEnded);
		}

		// 궁극기 사운드 재생 (멀티캐스트)
		if (CachedChanOwner->HasAuthority())
		{
			if (UGS_SeekerAudioComponent* AudioComp = CachedChanOwner->SeekerAudioComponent)
			{
				AudioComp->RequestSkillAudio(CurrentSkillType, 0);
			}
		}

		// 입력 제한 설정
		//CachedChanOwner->SetSkillInputControl(false, false, false);
		CachedChanOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::FullBody);
		CachedChanOwner->SetMoveControlValue(false, false);
	}

	// 돌진 시작 (약간 딜레이)
	GetWorld()->GetTimerManager().SetTimer(ChargeDelayTimerHandle, this, &UGS_ChanUltimateSkill::StartCharge, 0.5f, false);
}

void UGS_ChanUltimateSkill::OnSkillCanceledByDebuff()
{
	Super::OnSkillCanceledByDebuff();
}

void UGS_ChanUltimateSkill::OnSkillAnimationEnd()
{
	DeactiveSkill();
}

void UGS_ChanUltimateSkill::InterruptSkill()
{
	DeactiveSkill();
	Super::InterruptSkill();
}

void UGS_ChanUltimateSkill::HandleUltimateCollision(AActor* HitActor, UPrimitiveComponent* HitComp, const FHitResult& HitResult)
{
	// 돌진 중이 아니면 무시 (돌진 시작 전에 벽과 겹칠 수 있음)
	if (!bIsCharging)
	{
		return;
	}

	if (!OwnerCharacter || !OwnerCharacter->HasAuthority() || !CachedChanOwner.IsValid())
	{
		return;
	}

	if (AGS_Guardian* Guardian = Cast<AGS_Guardian>(HitActor)) // 가디언일 경우
	{
		ApplyEffectToGuardian(Guardian);

		// 가디언 충돌 사운드 재생 (멀티캐스트)
		if (CachedChanOwner.IsValid() && CachedChanOwner->SeekerAudioComponent)
		{
			CachedChanOwner->SeekerAudioComponent->RequestSkillAudio(CurrentSkillType, 6); // 6 = 가디언 충돌 (2 + 4)
		}

		// Impact VFX 재생 (Multicast 사용)
		if (OwningComp)
		{
			OwningComp->Multicast_PlayImpactVFXOnTarget(CurrentSkillType, Guardian);
		}

		EndCharge();
	}
	else if (AGS_Monster* Monster = Cast<AGS_Monster>(HitActor)) // 몬스터일 경우
	{
		if (!HitActors.Contains(Monster))
		{
			HitActors.Add(Monster);
			ApplyEffectToDungeonMonster(Monster);

			// 몬스터 충돌 사운드 재생 (멀티캐스트)
			if (CachedChanOwner.IsValid() && CachedChanOwner->SeekerAudioComponent)
			{
				CachedChanOwner->SeekerAudioComponent->RequestSkillAudio(CurrentSkillType, 5); // 5 = 몬스터 충돌 (1 + 4)
			}

			// Impact VFX 재생 (Multicast 사용)
			if (OwningComp)
			{
				OwningComp->Multicast_PlayImpactVFXOnTarget(CurrentSkillType, Monster);
			}
		}
	}

	// 벽 충돌 체크
	if (HitComp && HitComp->ComponentHasTag("Wall"))
	{
		// ---------------------------------------------------------------------------
		// [강화된 방향 판정 로직]
		// ---------------------------------------------------------------------------
		FVector OwnerLocation = OwnerCharacter->GetActorLocation();
		FVector OwnerForward = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();

		// 충돌 지점 결정 (Sweep 결과가 있으면 사용, 없으면 컴포넌트 중심)
		FVector TargetPoint = HitResult.ImpactPoint;
		bool bValidNormal = !HitResult.ImpactNormal.IsNearlyZero();

		// Overlap 이벤트의 경우 ImpactPoint가 (0,0,0)일 수 있음.
		// 이 경우 캐릭터 앞쪽 일정 거리를 가상의 충돌 지점으로 사용하거나 컴포넌트 위치 사용.
		if (TargetPoint.IsNearlyZero())
		{
			// 벽 컴포넌트의 위치가 캐릭터보다 앞쪽에 있는지 확인
			FVector ToWall = (HitComp->GetComponentLocation() - OwnerLocation).GetSafeNormal2D();
			float WallDot = FVector::DotProduct(OwnerForward, ToWall);

			// 전방 90도 이내에 벽의 중심이 있거나,
			// 혹은 그냥 벽에 닿았으므로 아주 보수적으로 전방인지만 체크
			if (WallDot > 0.3f)
			{
				TargetPoint = OwnerLocation + OwnerForward * 50.0f;
			}
			else
			{
				TargetPoint = HitComp->GetComponentLocation();
			}
		}

		FVector DirectionToImpact = (TargetPoint - OwnerLocation).GetSafeNormal2D();
		float ForwardDot = FVector::DotProduct(OwnerForward, DirectionToImpact);

		bool bIsFrontalCrash = false;
		if (bValidNormal)
		{
			float NormalDot = FVector::DotProduct(OwnerForward, -HitResult.ImpactNormal.GetSafeNormal2D());
			if (NormalDot > 0.1f && ForwardDot > 0.4f) // 임계값 소폭 완화
			{
				bIsFrontalCrash = true;
			}
		}
		else
		{
			// 법선 정보가 없는 경우 (Overlap 등)
			FVector LocalImpactPos = OwnerCharacter->GetActorTransform().InverseTransformPosition(TargetPoint);
			if (LocalImpactPos.X > 10.0f && ForwardDot > 0.3f)
			{
				bIsFrontalCrash = true;
			}
		}

		if (bIsFrontalCrash)
		{
			bInStructureCrash = true;

			// 벽 충돌 사운드 재생 (멀티캐스트)
			if (CachedChanOwner.IsValid() && CachedChanOwner->SeekerAudioComponent)
			{
				CachedChanOwner->SeekerAudioComponent->RequestSkillAudio(CurrentSkillType, 4); // 4 = 벽 충돌 (0 + 4)
			}

			// Env Impact VFX 재생 (Multicast 사용)
			if (OwningComp)
			{
				FVector SkillLocation = OwnerLocation + (OwnerForward * 50.0f);
				FRotator SkillRotation = OwnerCharacter->GetActorRotation();
				OwningComp->Multicast_PlayEnvImpactVFX(CurrentSkillType, SkillLocation, SkillRotation);
			}

			// 대시 종료
			EndCharge();
		}
	}
}

void UGS_ChanUltimateSkill::ApplyEffectToDungeonMonster(AGS_Monster* Target)
{
	if (!Target || !CachedChanOwner.IsValid())
	{
		return;
	}

	// 경직 디버프
	if (UGS_DebuffComp* DebuffComp = Target->FindComponentByClass<UGS_DebuffComp>())
	{
		Target->GetDebuffComp()->ApplyDebuff(EDebuffType::Stun, OwnerCharacter);
	}

	// 현재 프레임의 실시간 방향 계산
	FVector CurrentForward = CachedChanOwner.IsValid() ? CachedChanOwner->GetActorForwardVector().GetSafeNormal() : FVector::ForwardVector;
	FVector PlayerToMonster = Target->GetActorLocation() - (CachedChanOwner.IsValid() ? CachedChanOwner->GetActorLocation() : FVector::ZeroVector);
	FVector RightVector = FVector::CrossProduct(CurrentForward, FVector::UpVector).GetSafeNormal();

	// Dot 비교로 방향 판별
	float DotProduct = FVector::DotProduct(PlayerToMonster, RightVector);
	FVector KnockbackDirection = (DotProduct > 0) ? RightVector : -RightVector;

	FVector KnockbackVelocity = KnockbackDirection * KnockbackForce;
	KnockbackVelocity.Z = 300.0f;

	// 몬스터 넉백 적용
	if (Target->GetCharacterMovement())
	{
		Target->LaunchCharacter(KnockbackVelocity, true, true);
	}

	//UGameplayStatics::ApplyDamage(Target, Damage, OwnerCharacter->GetController(), OwnerCharacter, UDamageType::StaticClass());
}

void UGS_ChanUltimateSkill::ApplyEffectToGuardian(AGS_Guardian* Target)
{
	if (!Target || !CachedChanOwner.IsValid())
		return;

	// 가디언용 넉백 (더 약한 힘)
	FVector KnockbackDirection = (Target->GetActorLocation() - (CachedChanOwner.IsValid() ? CachedChanOwner->GetActorLocation() : FVector::ZeroVector)).GetSafeNormal();
	FVector KnockbackVelocity = KnockbackDirection * GuardianKnockbackForce;

	if (Target->GetCharacterMovement())
	{
		Target->LaunchCharacter(KnockbackVelocity, true, false);
	}
}

void UGS_ChanUltimateSkill::DeactiveSkill()
{
	if (!GetIsActive())
		return;

	// 상태 플래그 초기화
	bIsCharging = false;
	bInStructureCrash = false;

	if (CachedChanOwner.IsValid())
	{
		// 넉백 Collision 설정
		CachedChanOwner->UltimateCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// 무적 해제
		CachedChanOwner->SetInvincible(false);

		// 입력 제어 복구
		CachedChanOwner->SetMoveControlValue(true, true);
		CachedChanOwner->SetLookControlValue(true, true);

		// 애니메이션 델리게이트 해제
		if (UAnimInstance* AnimInstance = CachedChanOwner->GetMesh()->GetAnimInstance())
		{
			AnimInstance->OnMontageEnded.RemoveDynamic(this, &UGS_ChanUltimateSkill::OnMontageEnded);
		}

		// 몽타주 종료 (슬롯 초기화)
		CachedChanOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::None);
	}

	if (OwnerCharacter)
	{
		// 속도 조절 및 이동 중지
		OwnerCharacter->Server_SetCharacterSpeed(1.0f);
		if (OwnerCharacter->GetCharacterMovement())
		{
			OwnerCharacter->GetCharacterMovement()->StopMovementImmediately();
		}

		// 자동 이동 종료 (컨트롤러 제어)
		if (AGS_TpsController* Controller = Cast<AGS_TpsController>(OwnerCharacter->GetController()))
		{
			Controller->StopAutoMoveForward();
		}
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ChargeTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(ChargeUpdateTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(ChargeDelayTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(ForwardTraceTimerHandle);
	}
	// 충돌 이력 초기화
	HitActors.Empty();

	// 스킬 종료 사운드 재생 (멀티캐스트)
	if (OwnerCharacter->HasAuthority() && CachedChanOwner.IsValid())
	{
		if (UGS_SeekerAudioComponent* AudioComp = CachedChanOwner->SeekerAudioComponent)
		{
			AudioComp->RequestSkillAudio(CurrentSkillType, 1);
		}
	}

	// =======================
	// VFX 정리 및 종료 VFX 재생
	// =======================
	if (OwningComp)
	{
		FVector SkillLocation = OwnerCharacter->GetActorLocation();
		FRotator SkillRotation = OwnerCharacter->GetActorRotation();
		OwningComp->Multicast_PlayEndVFX(CurrentSkillType, SkillLocation, SkillRotation);
	}

	Super::DeactiveSkill();
}

void UGS_ChanUltimateSkill::StartCharge()
{
	// 스킬이 이미 종료되었거나 유효하지 않으면 중단
	if (!GetIsActive() || !OwnerCharacter || !CachedChanOwner.IsValid())
	{
		return;
	}

	// 넉백 Collision 켜기
	if (CachedChanOwner.IsValid())
	{
		CachedChanOwner->UltimateCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	// 만약 Collision을 켜자마자 즉시 충돌하여 스킬이 종료되었다면 여기서 중단 (예: 벽 앞에서 사용)
	if (!GetIsActive())
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
	    ChargeTimerHandle,
	    this,
	    &UGS_ChanUltimateSkill::EndCharge,
	    1.5f,
	    false);

	// 속도 조절
	OwnerCharacter->Server_SetCharacterSpeed(3.0f);

	// 자동 이동 시작
	AGS_TpsController* Controller = Cast<AGS_TpsController>(OwnerCharacter->GetController());
	Controller->SetMoveControlValue(true, true);
	Controller->StartAutoMoveForward();

	if (CachedChanOwner.IsValid())
	{
		// 애니메이션 설정
		CachedChanOwner->Multicast_SetMontageSlot(ESeekerMontageSlot::FullBody);

		if (UAnimMontage* LoadedMontage = GetCachedMontage(0))
		{
			CachedChanOwner->Multicast_PlaySkillMontage(LoadedMontage);
		}

		// =======================
		// VFX 재생 - 컴포넌트 RPC 사용
		// =======================
		if (OwningComp)
		{
			FVector SkillLocation = OwnerCharacter->GetActorLocation();
			//FRotator SkillRotation = OwnerCharacter->GetActorRotation();
			FRotator SkillRotation = FRotator(0.f, 0.f, 0.f);

			// 스킬 시전 VFX 재생
			OwningComp->Multicast_PlayCastVFX(CurrentSkillType, SkillLocation, SkillRotation);
		}
	}

	// 돌진 상태 설정 및 전방 장애물 감지 타이머 시작
	// (벽 바로 앞에서 시작해도 준비 동작이 끝날 때까지 유예)
	bIsCharging = true;
	GetWorld()->GetTimerManager().SetTimer(
	    ForwardTraceTimerHandle,
	    this,
	    &UGS_ChanUltimateSkill::CheckForwardObstacle,
	    0.016f, // 60 FPS
	    true,
	    0.05f); // 시작 후 0.05초 유예 시간 (대폭 단축)
}

void UGS_ChanUltimateSkill::EndCharge()
{
	// 돌진 상태 즉시 해제 (추가 감지 방지)
	bIsCharging = false;

	// 1. 즉시 멈춤 처리 (움직임, 충돌만)
	if (CachedChanOwner.IsValid())
	{
		CachedChanOwner->UltimateCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (OwnerCharacter)
	{
		OwnerCharacter->Server_SetCharacterSpeed(1.0f);
		if (OwnerCharacter->GetCharacterMovement())
		{
			OwnerCharacter->GetCharacterMovement()->StopMovementImmediately();
		}

		if (AGS_TpsController* Controller = Cast<AGS_TpsController>(OwnerCharacter->GetController()))
		{
			Controller->StopAutoMoveForward();
		}
	}

	// 타이머 정리
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ChargeTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(ChargeUpdateTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(ChargeDelayTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(ForwardTraceTimerHandle);
	}

	// 2. 애니메이션 선택 및 재생
	UAnimMontage* FinishMontage = nullptr;
	if (bInStructureCrash)
	{
		FinishMontage = GetCachedMontage(2);
	}
	else
	{
		FinishMontage = GetCachedMontage(1);
	}

	if (FinishMontage && CachedChanOwner.IsValid())
	{
		CachedChanOwner->Multicast_PlaySkillMontage(FinishMontage);
		// DeactiveSkill()은 OnMontageEnded -> OnSkillAnimationEnd()에 의해 호출됨
	}
	else
	{
		// 재생할 애니메이션이 없으면 즉시 종료 처리
		DeactiveSkill();
	}
}

void UGS_ChanUltimateSkill::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!OwnerCharacter || !GetIsActive())
		return;

	UAnimMontage* LoadedMontage1 = GetCachedMontage(1);
	UAnimMontage* LoadedMontage2 = GetCachedMontage(2);

	// 종료 애니메이션(일반 종료 또는 충돌 종료)이 끝났을 때만 Deactive 호출
	if ((LoadedMontage1 && Montage == LoadedMontage1) || (LoadedMontage2 && Montage == LoadedMontage2))
	{
		OnSkillAnimationEnd();
	}
}

void UGS_ChanUltimateSkill::CheckForwardObstacle()
{
	if (!bIsCharging || !OwnerCharacter || !CachedChanOwner.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
		return;

	// === 능동적 전방 스윗 트레이스 ===
	// 바닥 충돌을 피하기 위해 가슴 높이 정도에서 발사 (Z + 40)
	FVector Start = OwnerCharacter->GetActorLocation() + FVector(0, 0, 40.0f);
	FVector Forward = OwnerCharacter->GetActorForwardVector();
	FVector End = Start + (Forward * 120.0f); // 감지 거리 소폭 단축 (120cm)

	// 바닥에 닿지 않도록 트레이스 부피 축소
	float TraceRadius = 30.0f;
	float TraceHalfHeight = 40.0f;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerCharacter);
	// 캐릭터가 들고 있는 무기 등이 있다면 추가로 무시 (Attached Actors)
	TArray<AActor*> AttachedActors;
	OwnerCharacter->GetAttachedActors(AttachedActors);
	QueryParams.AddIgnoredActors(AttachedActors);
	QueryParams.bTraceComplex = false;

	// ECC_WorldStatic, ECC_WorldDynamic 대상
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult HitResult;
	bool bHit = World->SweepSingleByObjectType(
	    HitResult,
	    Start,
	    End,
	    FQuat::Identity,
	    ObjectQueryParams,
	    FCollisionShape::MakeCapsule(TraceRadius, TraceHalfHeight),
	    QueryParams);

	if (bHit && HitResult.GetComponent())
	{
		// 명확하게 "Wall" 태그가 있는 경우에만 장애물로 인지
		if (HitResult.GetComponent()->ComponentHasTag("Wall"))
		{
			// 돌진 방향과 충돌 법선 비교
			FVector ImpactNormal2D = HitResult.ImpactNormal.GetSafeNormal2D();
			FVector Forward2D = Forward.GetSafeNormal2D();
			float NormalDot = FVector::DotProduct(Forward2D, -ImpactNormal2D);

			// 전방 충돌 판정 (이미 겹쳐 있거나 정면 방향인 경우)
			if (HitResult.bStartPenetrating || NormalDot > 0.1f)
			{
				// 첫 번째 충돌만 처리
				bInStructureCrash = true;

				// 벽 충돌 사운드 재생 (멀티캐스트)
				if (CachedChanOwner.IsValid() && CachedChanOwner->SeekerAudioComponent)
				{
					CachedChanOwner->SeekerAudioComponent->RequestSkillAudio(CurrentSkillType, 4);
				}

				// Env Impact VFX 재생
				if (OwningComp)
				{
					FVector SkillLocation = HitResult.ImpactPoint;
					FRotator SkillRotation = OwnerCharacter->GetActorRotation();
					OwningComp->Multicast_PlayEnvImpactVFX(CurrentSkillType, SkillLocation, SkillRotation);
				}

				EndCharge();
			}
		}
	}
}
