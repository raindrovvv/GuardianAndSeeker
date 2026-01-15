// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Player/Monster/GS_SmallClaw.h"
#include "Character/Component/GS_StatComp.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "Character/F_GS_DamageEvent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "VFX/GS_VFX_FunctionLibrary.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Sound/GS_AudioMixingComponent.h"
#include "AkAudioEvent.h"

AGS_SmallClaw::AGS_SmallClaw()
{
	BiteCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BiteCollision"));
	BiteCollision->SetupAttachment(GetMesh(), TEXT("head"));
	BiteCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BiteCollision->OnComponentBeginOverlap.AddDynamic(this, &AGS_SmallClaw::OnAttackBiteboxOverlap);
}

void AGS_SmallClaw::BeginPlay()
{
	Super::BeginPlay();

	// === BiteCollision에 방어 가능 태그 추가 ===
	if (BiteCollision)
	{
		BiteCollision->ComponentTags.AddUnique(FName("DEFENSIBLE_ATTACK"));
	}

	// SmallClaw 전용 몬스터 오디오 설정 (컴포넌트 사용)
	if (MonsterAudioComponent)
	{
		// 작은 몬스터 특성: 가까운 거리에서 경계, 짧은 최대 거리
		MonsterAudioComponent->AudioConfig.AlertDistance = 600.0f;
		MonsterAudioComponent->AudioConfig.MaxAudioDistance = 2000.0f;

		// 사운드 재생 간격 (작은 몬스터이므로 자주 울음)
		MonsterAudioComponent->IdleSoundInterval = 4.0f;
		MonsterAudioComponent->CombatSoundInterval = 2.5f;
	}
}

void AGS_SmallClaw::SetBiteCollision(bool bEnable)
{
	if (BiteCollision)
	{
		BiteCollision->SetCollisionEnabled(bEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void AGS_SmallClaw::OnAttackBiteboxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                           UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)

{
	if (!HasAuthority())
	{
		return;
	}

	AGS_Character* Damaged = Cast<AGS_Character>(OtherActor);
	AGS_Character* Attacker = this;
	if (!Damaged || !Attacker || !Damaged->IsEnemy(Attacker))
	{
		return;
	}

	if (AGS_Character* DamagedCharacter = Cast<AGS_Character>(OtherActor))
	{
		if (!DamagedCharacter->IsEnemy(Cast<AGS_Character>(this)))
		{
			return;
		}

		bool bIsCritical = false;
		float Damage = DamagedCharacter->GetStatComp()->CalculateDamage(this, DamagedCharacter, bIsCritical);
		FGS_DamageEvent DamageEvent;
		DamageEvent.HitReactType = EHitReactType::DamageOnly;
		DamageEvent.bIsCritical = bIsCritical;

		float ActualDamage = OtherActor->TakeDamage(Damage, DamageEvent, GetController(), this);

		// 실제로 데미지가 적용된 경우에만 혈흔 이펙트 및 레이어드 사운드 재생
		if (ActualDamage > 0.0f)
		{
			// 혈흔 이펙트 재생 - 깨물기 지점에서 재생
			FVector HitLocation = SweepResult.bBlockingHit ? FVector(SweepResult.ImpactPoint) : DamagedCharacter->GetActorLocation();
			FVector HitNormal = SweepResult.bBlockingHit ? FVector(SweepResult.ImpactNormal) : FVector::UpVector;

			Multicast_PlayBloodEffect(HitLocation, HitNormal);

			// [추가] 레이어드 사운드 재생
			Multicast_PlayLayeredHitSound(SweepResult, OtherActor);
		}

		BiteCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AGS_SmallClaw::Multicast_PlayBloodEffect_Implementation(FVector HitLocation, FVector HitNormal)
{
	if (ShouldPlayVFXAtLocation(HitLocation, 3000.0f))
	{
		UGS_VFX_FunctionLibrary::PlayBloodEffect(this, BloodEffectSystem, HitLocation, FRotationMatrix::MakeFromZ(HitNormal).Rotator(), 0.8f);
	}
}

void AGS_SmallClaw::PlayLayeredHitSound(const FHitResult& HitResult, AActor* HitActor)
{
	if (!GetWorld() || IsRunningDedicatedServer() || !AudioMixingComponent)
	{
		return;
	}

	// HitActor의 재질 타입에 따라 적절한 사운드 선택
	UAkAudioEvent* ImpactEvent = ImpactFleshSoundEvent;

	if (AGS_Character* TargetChar = Cast<AGS_Character>(HitActor))
	{
		EImpactMaterialType MatType = TargetChar->GetImpactMaterialType();
		switch (MatType)
		{
		case EImpactMaterialType::Armor:
			ImpactEvent = ImpactArmorSoundEvent;
			break;
		case EImpactMaterialType::Flesh:
		default:
			ImpactEvent = ImpactFleshSoundEvent;
			break;
		}

		// 해당 재질 이벤트가 없으면 Flesh로 폴백
		if (!ImpactEvent)
		{
			ImpactEvent = ImpactFleshSoundEvent;
		}
	}

	AudioMixingComponent->PostLayeredSound(ImpactEvent, ReverbSoundEvent, HitResult.ImpactPoint, HitActor);
}

void AGS_SmallClaw::Multicast_PlayLayeredHitSound_Implementation(const FHitResult& HitResult, AActor* HitActor)
{
	PlayLayeredHitSound(HitResult, HitActor);
}

float AGS_SmallClaw::GetOptimalCullDistance() const
{
	return GS_Rendering::MONSTER_SMALL_CULL_DISTANCE;
}
