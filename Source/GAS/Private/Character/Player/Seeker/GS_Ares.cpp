// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Player/Seeker/GS_Ares.h"
#include "Sound/GS_SeekerAudioComponent.h"
#include "Character/Component/Seeker/GS_AresSkillInputHandlerComp.h"
#include "Character/Component/GS_StatComp.h"

/*#include "Animation/Character/GS_SeekerAnimInstance.h"
#include "Character/GS_TpsController.h"
#include "Character/Component/Seeker/GS_AresSkillInputHandlerComp.h"*/
#include "Character/Skill/GS_SkillComp.h"
#include "Character/Skill/Seeker/Ares/GS_AresMovingSkill.h"
#include "Components/CapsuleComponent.h"


// Sets default values
AGS_Ares::AGS_Ares()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	CharacterType = ECharacterType::Ares;
	SkillInputHandlerComponent = CreateDefaultSubobject<UGS_AresSkillInputHandlerComp>(TEXT("SkillInputHandlerComp"));

	// 사운드 배열들은 GS_SeekerAudioComponent에서 관리됨

	// KeyManual에서 쓰일 캐릭터 타입 저장
	ManualRowName = FName("Ares");

	// 타격 보정 설정 (아레스: 긴 사거리, 좁은 유도각)
	MagnetismDistance = 500.0f;
	MagnetismAngle = 45.0f;
}

// Called when the game starts or when spawned
void AGS_Ares::BeginPlay()
{
	Super::BeginPlay();

	SetReplicateMovement(true);
	GetMesh()->SetIsReplicated(true);

	// Moving 스킬 객체를 가져와서 카메라 설정값 전달
	if (SkillComp)
	{
		UGS_AresMovingSkill* MovingSkill = Cast<UGS_AresMovingSkill>(SkillComp->GetSkillFromSkillMap(ESkillSlot::Moving));
		if (MovingSkill)
		{
			MovingSkill->SetCameraSettings(
			    MovingSkill_ZoomOutDistance,
			    MovingSkill_CameraZoomCurve,
			    MovingSkill_EnableMotionBlur,
			    MovingSkill_MotionBlurPeakAmount,
			    MovingSkill_MotionBlurCurve,
			    MovingSkill_MotionBlurExponent);
		}
	}
}

// Called every frame
void AGS_Ares::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AGS_Ares::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

/*void AGS_Ares::OnComboAttack()
{
	Super::OnComboAttack();
}*/

void AGS_Ares::ServerAttackMontage()
{
	Super::ServerAttackMontage();
}

void AGS_Ares::MulticastPlayComboSection_Implementation(int32 ComboIndex)
{
	Super::MulticastPlayComboSection_Implementation(ComboIndex);

	// SeekerAudioComponent를 통해 아레스 전용 콤보 공격 사운드 재생 (1-based 인덱스 전달)
	if (SeekerAudioComponent)
	{
		// GS_SeekerAudioComponent의 AresComboXXX 프로퍼티들을 사용하여 사운드 재생
		SeekerAudioComponent->PlayAresComboAttackSoundWithExtra(ComboIndex + 1);
	}
}

void AGS_Ares::Multicast_OnAttackHit_Implementation(int32 ComboIndex)
{
	// 조작감 개선: 콤보 인덱스별 차별화된 타격 정지(Hit-stop) 적용
	// 멀티플레이 유의: 대검의 무게감은 유지하되 끊김 현상을 줄이기 위해 시간 조정 (0.11 -> 0.07)
	float BaseDuration = 0.07f;
	float FinalDuration = BaseDuration;

	if (ComboIndex >= 4)
	{
		FinalDuration = 0.11f; // 대검 피니셔: 묵직하지만 빠른 복구 유도
	}
	else
	{
		// 콤보 진행에 따른 점진적 강화 (최대 1.2배)
		float Scale = 1.0f + (FMath::Min(2, FMath::Max(0, ComboIndex - 1)) * 0.1f);
		FinalDuration = BaseDuration * Scale;
	}

	Multicast_ApplyHitStop(FinalDuration, 0.0f, true);

	// 공격 성공 시 공격자에게 카메라 쉐이크 적용 (Ares 전용)
	if (HasAuthority())
	{
		if (APlayerController* AttackerPC = Cast<APlayerController>(GetController()))
		{
			// 4번째 공격(마지막 공격)은 더 강한 쉐이크 적용
			if (ComboIndex == 4)
			{
				// 강한 공격 성공 쉐이크 (마지막 콤보)
				FGS_CameraShakeInfo StrongAttackShake = AttackSuccessShake;
				StrongAttackShake.Intensity *= 1.6f; // Ares 마지막 공격 강도
				Client_PlayAttackSuccessShakeWithInfo(AttackerPC, StrongAttackShake);
			}
			else
			{
				// 일반 공격 성공 쉐이크
				Client_PlayAttackSuccessShake(AttackerPC);
			}
		}
	}
}

float AGS_Ares::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	// Call parent implementation
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	return ActualDamage;
}

void AGS_Ares::Multicast_RestoreDashCameraZoom_Implementation()
{
	// 로컬 클라이언트에서만 카메라 복원 실행
	if (!IsLocallyControlled())
	{
		return;
	}

	if (SkillComp)
	{
		UGS_AresMovingSkill* MovingSkill = Cast<UGS_AresMovingSkill>(SkillComp->GetSkillFromSkillMap(ESkillSlot::Moving));
		if (MovingSkill)
		{
			MovingSkill->RestoreCameraZoom(true);
		}
	}
}
