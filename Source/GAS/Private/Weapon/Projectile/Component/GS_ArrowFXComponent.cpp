// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/Projectile/Component/GS_ArrowFXComponent.h"
#include "Engine/World.h"
#include "Character/GS_Character.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Actor.h"
#include "AkGameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Rendering/GS_RenderingConstants.h"


UGS_ArrowFXComponent::UGS_ArrowFXComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 기본값 설정
	NormalArrowTrailVFX = nullptr;
	AxeArrowTrailVFX = nullptr;
	ChildArrowTrailVFX = nullptr;
	ActiveTrailVFXComponent = nullptr;
	OwnerActor = nullptr;

	NormalHitPawnVFX = nullptr;
	AxeHitPawnVFX = nullptr;
	ChildHitPawnVFX = nullptr;

	HitStructureVFX = nullptr;
	HitPawnSoundEvent = nullptr;
	HitStructureSoundEvent = nullptr;

	// 네트워크 복제 설정
	SetIsReplicatedByDefault(true);
}

void UGS_ArrowFXComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGS_ArrowFXComponent, CurrentArrowType);
}

void UGS_ArrowFXComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerActor = GetOwner();

	// if (OwnerActor && OwnerActor->HasAuthority())
	// {
	// 	Multicast_StartArrowTrailVFX(EArrowType::Normal);
	// }
}

void UGS_ArrowFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// VFX 정리
	if (ActiveTrailVFXComponent)
	{
		ActiveTrailVFXComponent->DestroyComponent();
		ActiveTrailVFXComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UGS_ArrowFXComponent::StartArrowTrailVFX(EArrowType ArrowType)
{
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		Multicast_StartArrowTrailVFX(ArrowType);
	}
}

void UGS_ArrowFXComponent::StopArrowTrailVFX()
{
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		Multicast_StopArrowTrailVFX();
	}
}

void UGS_ArrowFXComponent::PlayHitVFX(ETargetType TargetType, const FHitResult& SweepResult, EArrowType ArrowType)
{
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		Multicast_PlayHitVFX(TargetType, SweepResult, ArrowType);
	}
}

void UGS_ArrowFXComponent::PlayHitSound(ETargetType TargetType, const FHitResult& SweepResult, EArrowType ArrowType, AActor* HitActor)
{
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		// RPC 스로틀링: 짧은 시간 내에 여러 번의 RPC가 발생하는 것을 방지
		if (UWorld* World = GetWorld())
		{
			float Now = World->GetTimeSeconds();
			if (Now - LastHitSoundRPCTime < HitSoundThrottle)
			{
				return;
			}
			LastHitSoundRPCTime = Now;
		}

		Multicast_PlayHitSound(TargetType, SweepResult, ArrowType, HitActor);
	}
}

void UGS_ArrowFXComponent::SetArrowType(EArrowType ArrowType)
{
	// 기존 VFX 중지하고 새 타입으로 시작
	StopArrowTrailVFX();
	StartArrowTrailVFX(ArrowType);
}

void UGS_ArrowFXComponent::Multicast_StartArrowTrailVFX_Implementation(EArrowType ArrowType)
{
	if (!OwnerActor)
	{
		return;
	}

	// VFX 거리 기반 컬링 (트레일 VFX)
	if (GetWorld())
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
			{
				const FVector CameraLoc = CameraManager->GetCameraLocation();
				float DistSq = FVector::DistSquared(CameraLoc, GetOwner()->GetActorLocation());

				// 전역 렌더링 상수 사용
				const float MaxDistSq = FMath::Square(GS_Rendering::VFX_DISABLE_DISTANCE);
				if (DistSq > MaxDistSq)
				{
					return;
				}
			}
		}
	}

	// 화살 타입에 따른 VFX 선택
	UNiagaraSystem* SelectedVFXSystem = nullptr;
	switch (ArrowType)
	{
	case EArrowType::Normal:
		SelectedVFXSystem = NormalArrowTrailVFX;
		break;
	case EArrowType::Axe:
		SelectedVFXSystem = AxeArrowTrailVFX;
		break;
	case EArrowType::Child:
		SelectedVFXSystem = ChildArrowTrailVFX;
		break;
	default:
		SelectedVFXSystem = NormalArrowTrailVFX; // 기본값
		break;
	}

	if (!SelectedVFXSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("No VFX System assigned for arrow type: %d"), static_cast<int32>(ArrowType));
		return;
	}

	// 현재 화살 타입 저장 (Hit VFX 등에서 사용)
	CurrentArrowType = ArrowType;

	// 이미 활성화된 VFX가 있으면 중지
	if (ActiveTrailVFXComponent)
	{
		ActiveTrailVFXComponent->DestroyComponent();
		ActiveTrailVFXComponent = nullptr;
	}

	// 새 나이아가라 컴포넌트 생성
	ActiveTrailVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
	    SelectedVFXSystem,
	    OwnerActor->GetRootComponent(),
	    NAME_None, // 소켓 이름 (없으면 루트에 부착)
	    TrailVFXLocationOffset,
	    TrailVFXRotationOffset,
	    TrailVFXScale,
	    EAttachLocation::KeepRelativeOffset,
	    true, // Auto Destroy
	    ENCPoolMethod::None,
	    true, // Auto Activate
	    true // Pre Cull Check
	);

	UE_LOG(LogTemp, Log, TEXT("Arrow Trail VFX Started for type: %d"), static_cast<int32>(ArrowType));
}

void UGS_ArrowFXComponent::Multicast_StopArrowTrailVFX_Implementation()
{
	if (ActiveTrailVFXComponent)
	{
		ActiveTrailVFXComponent->Deactivate();
		ActiveTrailVFXComponent->DestroyComponent();
		ActiveTrailVFXComponent = nullptr;

		UE_LOG(LogTemp, Log, TEXT("Arrow Trail VFX Stopped"));
	}
}

void UGS_ArrowFXComponent::Multicast_PlayHitVFX_Implementation(ETargetType TargetType, const FHitResult& SweepResult, EArrowType ArrowType)
{
	// VFX 거리 기반 컬링
	if (!GetWorld())
		return;

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FVector CameraLoc;
		FRotator CameraRot;
		PC->GetPlayerViewPoint(CameraLoc, CameraRot);
		float DistSq = FVector::DistSquared(CameraLoc, SweepResult.ImpactPoint);

		// 전역 렌더링 상수 사용
		const float MaxDistSq = FMath::Square(GS_Rendering::VFX_DISABLE_DISTANCE);
		if (DistSq > MaxDistSq)
		{
			return;
		}
	}

	UNiagaraSystem* VFXToPlay = nullptr;

	switch (TargetType)
	{
	case ETargetType::Guardian:
	case ETargetType::DungeonMonster:
		// 전달받은 화살 타입에 따라 적 캐릭터 적중 VFX 선택
		switch (ArrowType)
		{
		case EArrowType::Normal:
			VFXToPlay = NormalHitPawnVFX;
			break;
		case EArrowType::Axe:
			VFXToPlay = AxeHitPawnVFX;
			break;
		case EArrowType::Child:
			VFXToPlay = ChildHitPawnVFX;
			break;
		default:
			VFXToPlay = NormalHitPawnVFX;
			break;
		}
		break;
	case ETargetType::Structure:
		// 벽이나 구조물에 맞았을 때 VFX
		VFXToPlay = HitStructureVFX;
		break;
	case ETargetType::Seeker:
	case ETargetType::Skill:
		// 아군 시커나 스킬에 맞았을 때는 VFX 없음
		break;
	default:
		break;
	}

	// VFX 재생
	if (VFXToPlay && GetWorld())
	{
		// 히트 포인트에서 VFX 재생
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		    GetWorld(),
		    VFXToPlay,
		    SweepResult.ImpactPoint,
		    SweepResult.ImpactNormal.Rotation(), // 히트 표면의 법선 방향으로 VFX 회전
		    FVector(1.0f), // Scale
		    true, // Auto Destroy
		    true // Auto Activate
		);

		UE_LOG(LogTemp, Log, TEXT("Arrow Hit VFX Played at %s"), *SweepResult.ImpactPoint.ToString());
	}
}

void UGS_ArrowFXComponent::Multicast_PlayHitSound_Implementation(ETargetType TargetType, const FHitResult& SweepResult, EArrowType ArrowType, AActor* HitActor)
{
	UAkAudioEvent* SoundEventToPlay = nullptr;

	switch (TargetType)
	{
	case ETargetType::Guardian:
	case ETargetType::DungeonMonster:
		// 적 캐릭터(가디언, 던전몬스터)에 맞았을 때 Wwise 이벤트
		SoundEventToPlay = HitPawnSoundEvent;
		break;
	case ETargetType::Structure:
		// 벽이나 구조물에 맞았을 때 Wwise 이벤트
		SoundEventToPlay = HitStructureSoundEvent;
		break;
	case ETargetType::Seeker:
	case ETargetType::Skill:
		// 아군 시커나 스킬에 맞았을 때는 사운드 없음
		break;
	default:
		break;
	}

	// Wwise 사운드 재생 (글로벌 팝핑 방지를 위해 대상 액터 기반으로 재생)
	// 사운드 재생
	if (SoundEventToPlay && GetWorld())
	{
		// [최적화] 사운드 거리 기반 컬링
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FVector CameraLoc;
			FRotator CameraRot;
			PC->GetPlayerViewPoint(CameraLoc, CameraRot);
			float DistSq = FVector::DistSquared(CameraLoc, SweepResult.ImpactPoint);

			// 투사체 히트 사운드는 가시성 거리까지 허용 (전역 상수 60m 사용)
			if (DistSq > FMath::Square(GS_Rendering::VFX_DISABLE_DISTANCE))
			{
				return;
			}
		}

		// 우선순위: 1. 직접 맞은 액터, 2. 화살 소유자 (폴백)
		AActor* SoundOwner = HitActor ? HitActor : OwnerActor;

		if (SoundOwner)
		{
			UAkGameplayStatics::PostEvent(SoundEventToPlay, SoundOwner, 0, FOnAkPostEventCallback(), false);
		}
		else
		{
			// 폴백: 액터가 전혀 없으면 위치 기반 재생
			UAkGameplayStatics::PostEventAtLocation(SoundEventToPlay, SweepResult.ImpactPoint, FRotator::ZeroRotator, GetWorld());
		}
	}
}