#include "Props/Trap/NonTriggerTrap/GS_NonTrigTrapBase.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Components/CapsuleComponent.h"
#include "Rendering/GS_RenderingConstants.h"

AGS_NonTrigTrapBase::AGS_NonTrigTrapBase()
{
	// 부모 클래스(GS_TrapBase)의 ActivateSphereComp 사용
	// GS_TrapBase에서 이미 생성됨
}

void AGS_NonTrigTrapBase::BeginPlay()
{
	Super::BeginPlay();

	// 논트리거 함정용 콜리전 활성화
	if (ActivateSphereComp)
	{
		//ActivateSphereComp->OnComponentBeginOverlap.AddDynamic(this, &AGS_NonTrigTrapBase::OnActivSCompBeginOverlap);
		ActivateSphereComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	// 추가적인 논트리거 함정 초기화 로직이 필요한 경우 여기에 추가
}

void AGS_NonTrigTrapBase::OnActivSCompBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                                   bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this)
	{
		AGS_Seeker* Seeker = Cast<AGS_Seeker>(OtherActor);
		if (Seeker)
		{
			// Seeker의 경우 오직 CapsuleComponent와의 충돌만 인정 (CombatTrigger 등 감지 방지)
			if (OtherComp != Cast<UPrimitiveComponent>(Seeker->GetCapsuleComponent()))
			{
				return;
			}

			// 재발동 시에도 사운드가 들리도록 함정 활성화 사운드 재생
			PlayActivationSound();

			if (!bIsActivated)
			{
				bIsActivated = true;

				if (!HasAuthority())
				{
					Server_ActivateTrap(OtherActor);
				}
				else
				{
					ActivateTrap(OtherActor);
				}

				if (!GetWorld()->GetTimerManager().IsTimerActive(CheckOverlapTimerHandle))
				{
					StartDeactivateTrapCheck();
				}
			}
		}
	}
}

// ===================
// Non-Trigger Trap Specific Functions Implementation
// ===================

void AGS_NonTrigTrapBase::ActivateNonTriggerTrap(AActor* TargetActor)
{
	// GS_TrapBase의 활성화 사운드 재생 (RTS 모드 지원됨)
	PlayActivationSound();

	// 논트리거 함정 전용 로직
	if (!bIsActivated)
	{
		bIsActivated = true;

		if (!HasAuthority())
		{
			Server_ActivateTrap(TargetActor);
		}
		else
		{
			ActivateTrap(TargetActor);
		}

		if (!GetWorld()->GetTimerManager().IsTimerActive(CheckOverlapTimerHandle))
		{
			StartDeactivateTrapCheck();
		}
	}
}

void AGS_NonTrigTrapBase::DeactivateNonTriggerTrap()
{
	// GS_TrapBase의 비활성화 사운드 재생 (RTS 모드 지원됨)
	PlayDeactivationSound();

	// 논트리거 함정 전용 로직
	if (bIsActivated)
	{
		bIsActivated = false;
		DeActivateTrap();
		GetWorld()->GetTimerManager().ClearTimer(CheckOverlapTimerHandle);
	}
}

// ===================
// Trap Motion Functions Implementation
// ===================

bool AGS_NonTrigTrapBase::CanStartMotion() const
{
	//UE_LOG(LogTemp, Warning, TEXT("[CanStartMotion] bIsActivated: %s"), bIsActivated ? TEXT("true") : TEXT("false"));
	return bIsActivated;
}

bool AGS_NonTrigTrapBase::CanStopMotion() const
{
	// false가 되면 제거
	return !bIsActivated;
}

float AGS_NonTrigTrapBase::CalculateSignificance(const FTransform& Viewpoint)
{
	// 논트리거 함정(영구 활성화 함정 등)은 활성화 상태와 관계없이
	// 항상 거리 기반으로 적응형 중요도를 계산하여 컬링이 작동하게 함
	float DistSq = FVector::DistSquared(GetActorLocation(), Viewpoint.GetLocation());
	const float FinalCullDistance = GS_Rendering::CalculateCullDistance(this, GetTrapCullDistance());

	// 컬링 거리의 1.1배를 기준으로 0.1~1.0 사이 점수 계산
	float MaxDistSq = FMath::Square(FinalCullDistance * 1.1f);
	return FMath::Clamp(1.0f - (DistSq / MaxDistSq), 0.1f, 1.0f);
}