// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Projectile/Seeker/GS_SeekerMerciArrow.h"
#include "Weapon/Projectile/Seeker/GS_ArrowVisualActor.h"
#include "Weapon/Projectile/Component/GS_ArrowFXComponent.h"
#include "Component/GS_VisualPoolComp.h"
#include "Components/SphereComponent.h"
#include "Character/Player/Guardian/GS_Guardian.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/Seeker/GS_Merci.h"
#include "Character/Skill/Seeker/GS_FieldSkillActor.h"
#include "AkGameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ResourceSystem/Aether/GS_AetherExtractor.h"


AGS_SeekerMerciArrow::AGS_SeekerMerciArrow()
{
	// 화살 FX 컴포넌트 생성 (VFX + Sound)
	ArrowFXComponent = CreateDefaultSubobject<UGS_ArrowFXComponent>(TEXT("ArrowFXComponent"));

	if (HasAuthority())
	{
		// 화살 스폰 직후
		this->SetActorEnableCollision(false);
	}
}

void AGS_SeekerMerciArrow::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		if (CollisionComponent)
		{
			CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AGS_SeekerMerciArrow::OnBeginOverlap);

			// Overlap 설정 강화
			CollisionComponent->SetGenerateOverlapEvents(true);
			CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		this->SetActorEnableCollision(true);

		AActor* IgnoredActor = GetInstigator();
		if (IgnoredActor && CollisionComponent)
		{
			CollisionComponent->IgnoreActorWhenMoving(IgnoredActor, true);
		}
	}
}

void AGS_SeekerMerciArrow::StickWithVisualOnly(const FHitResult& Hit)
{
	if (!ProjectileMesh || bAlreadyStuck)
	{
		return;
	}
	bAlreadyStuck = true;

	// 화살이 박힐 정확한 지점 계산
	FVector StickLocation = Hit.ImpactPoint;
	FVector StickNormal = Hit.ImpactNormal;

	// Overlap 시 ImpactPoint가 0인 경우가 많으므로 보정
	if (StickLocation.IsZero())
	{
		FVector ArrowLoc = GetActorLocation();
		FVector PrevLoc = ArrowLoc - (GetVelocity() * GetWorld()->GetDeltaSeconds() * 2.0f);

		FHitResult SurfaceHit;
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(ArrowStickTrace), true, this);

		// [최적화] ECC_Visibility 대신 WorldStatic 채널 사용 (벽 감지 정확도 향상)
		if (GetWorld()->LineTraceSingleByChannel(SurfaceHit, PrevLoc, ArrowLoc + (GetActorForwardVector() * 100.f), ECC_WorldStatic, TraceParams))
		{
			StickLocation = SurfaceHit.ImpactPoint;
			StickNormal = SurfaceHit.ImpactNormal;
		}
		else
		{
			StickLocation = ArrowLoc;
			StickNormal = -GetActorForwardVector();
		}
	}

	// 화살의 회전값 결정
	FRotator StickRotation = GetActorRotation();

	// [수정] StickNormal 방향으로 '빼야' 벽 안쪽으로 파고듭니다.
	// 너무 많이 파고들면 사라지므로 10~15유닛이 적당합니다.
	StickLocation -= (StickNormal * 12.0f);

	AGS_Merci* Merci = Cast<AGS_Merci>(GetOwner());
	if (!Merci)
		Merci = Cast<AGS_Merci>(GetInstigator());

	if (Merci && Merci->VisualPool)
	{
		if (AGS_ArrowVisualActor* VisualArrow = Merci->VisualPool->GetActorFromPool(StickLocation, StickRotation))
		{
			if (USkeletalMesh* MeshAsset = ProjectileMesh->GetSkeletalMeshAsset())
			{
				VisualArrow->SetArrowMesh(MeshAsset);
			}
			VisualArrow->SetAttachedTargetActor(Hit.GetActor());

			if (Hit.Component.IsValid())
			{
				VisualArrow->AttachToComponent(Hit.Component.Get(), FAttachmentTransformRules::KeepWorldTransform, Hit.BoneName);
			}
		}
	}

	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->StopMovementImmediately();
		ProjectileMovementComponent->Deactivate();
	}
	SetActorEnableCollision(false);
	Destroy();
}

void AGS_SeekerMerciArrow::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Overlap과 통합 처리
	OnBeginOverlap(HitComp, OtherActor, OtherComp, 0, true, Hit);
}

void AGS_SeekerMerciArrow::Multicast_InitHomingTarget_Implementation(AActor* Target)
{
	if (!ProjectileMovementComponent)
	{
		return;
	}

	if (Target && IsValid(Target))
	{
		UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Target->GetRootComponent());
		if (!RootPrim)
		{
			// 루트 컴포넌트가 Primitive가 아니면 SkeletalMesh라도 시도
			RootPrim = Cast<UPrimitiveComponent>(Target->GetComponentByClass(UMeshComponent::StaticClass()));
		}

		if (RootPrim)
		{
			ProjectileMovementComponent->bRotationFollowsVelocity = true;
			ProjectileMovementComponent->HomingTargetComponent = RootPrim;
			ProjectileMovementComponent->bIsHomingProjectile = true;

			// 유도 성능 설정
			ProjectileMovementComponent->InitialSpeed = 3000.f;
			ProjectileMovementComponent->MaxSpeed = 4000.f;
			ProjectileMovementComponent->HomingAccelerationMagnitude = 30000.f; // 유도성 강화
			ProjectileMovementComponent->ProjectileGravityScale = 0.0f;

			// 초기 속도 방향 설정 (서버/클라이언트 동기화)
			ProjectileMovementComponent->Velocity = GetActorForwardVector() * ProjectileMovementComponent->InitialSpeed;

			HomingTarget = Target;
			if (AGS_Character* TargetCharacter = Cast<AGS_Character>(Target))
			{
				TargetCharacter->OnDeathDelegate.AddUniqueDynamic(this, &AGS_SeekerMerciArrow::OnTargetDied);
			}

			UE_LOG(LogTemp, Warning, TEXT("HomingTarget successfully set to %s"), *Target->GetName());
			return;
		}
	}

	// 타겟이 없거나 유효하지 않으면 일반 화살로 동작
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->HomingTargetComponent = nullptr;
	ProjectileMovementComponent->bIsHomingProjectile = false;
	ProjectileMovementComponent->InitialSpeed = 5000.f;
	ProjectileMovementComponent->MaxSpeed = 5000.0f;
	ProjectileMovementComponent->ProjectileGravityScale = 1.0f;
	ProjectileMovementComponent->Velocity = GetActorForwardVector() * ProjectileMovementComponent->InitialSpeed;

	HomingTarget = nullptr;
	UE_LOG(LogTemp, Warning, TEXT("HomingTarget invalid or null — switching to normal arrow"));
}

// === 개선된 FindClosestBoneName 함수 ===
FName AGS_SeekerMerciArrow::FindClosestBoneName(USkeletalMeshComponent* MeshComp, const FVector& WorldLocation)
{
	if (!MeshComp || !MeshComp->GetSkeletalMeshAsset())
	{
		return NAME_None;
	}

	const FReferenceSkeleton& RefSkeleton = MeshComp->GetSkeletalMeshAsset()->GetRefSkeleton();
	const TArray<FMeshBoneInfo>& BoneInfos = RefSkeleton.GetRefBoneInfo();

	if (BoneInfos.Num() == 0)
	{
		return NAME_None;
	}

	FName ClosestBone = NAME_None;
	float ClosestDistance = FLT_MAX;

	// === 주요 본들만 우선 검사 (성능 최적화) ===
	TArray<FName> PriorityBones = {
	    TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
	    TEXT("head"), TEXT("neck_01"),
	    TEXT("upperarm_l"), TEXT("upperarm_r"),
	    TEXT("lowerarm_l"), TEXT("lowerarm_r"),
	    TEXT("thigh_l"), TEXT("thigh_r"),
	    TEXT("calf_l"), TEXT("calf_r"),
	    TEXT("chest"), TEXT("pelvis")};

	// 1. 우선순위 본들부터 검사
	for (const FName& PriorityBone : PriorityBones)
	{
		if (MeshComp->GetBoneIndex(PriorityBone) != INDEX_NONE)
		{
			FVector BoneWorldLocation = MeshComp->GetBoneLocation(PriorityBone, EBoneSpaces::WorldSpace);
			float Distance = FVector::Dist(WorldLocation, BoneWorldLocation);

			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestBone = PriorityBone;
			}
		}
	}

	// 2. 우선순위 본에서 찾지 못했거나 거리가 너무 멀면 전체 본 검사
	if (ClosestBone == NAME_None || ClosestDistance > 50.0f)
	{
		for (int32 BoneIndex = 0; BoneIndex < BoneInfos.Num(); ++BoneIndex)
		{
			FName BoneName = BoneInfos[BoneIndex].Name;
			FVector BoneWorldLocation = MeshComp->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace);
			float Distance = FVector::Dist(WorldLocation, BoneWorldLocation);

			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestBone = BoneName;
			}
		}
	}

	return ClosestBone;
}

void AGS_SeekerMerciArrow::OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || HitActors.Contains(OtherActor))
	{
		return;
	}

	HitActors.Add(OtherActor);
	UE_LOG(LogTemp, Log, TEXT("Arrow hit actor: %s"), *OtherActor->GetName());

	// 맞은 대상 구분
	ETargetType TargetType = DetermineTargetType(OtherActor);

	// 이펙트와 사운드 처리 (가상함수로 만들어 자식에서 오버라이드 가능)
	ProcessHitEffects(TargetType, SweepResult);

	// 서버에서만 데미지 및 로직 처리
	if (HasAuthority())
	{
		// 데미지 처리 (자식 클래스에서 구현)
		ProcessDamageLogic(TargetType, SweepResult, OtherActor);
	}
	bool bShouldContinueMovement = false;

	if (HasAuthority())
	{
		// HandleTargetTypeGeneric에서 관통 여부를 반환값으로 받음
		bShouldContinueMovement = HandleTargetTypeGeneric(TargetType, SweepResult);

		if (!IsValid(this))
		{
			UE_LOG(LogTemp, Warning, TEXT("Arrow marked for destruction, stopping processing"));
			return;
		}
	}

	// 관통하는 경우 이동 중지하지 않고 함수 종료
	if (bShouldContinueMovement)
	{
		UE_LOG(LogTemp, Warning, TEXT("Arrow penetrating, continuing movement"));
		return;
	}

	// === 여기까지 왔다는 것은 관통하지 않는다는 뜻 ===
	bAlreadyHit = true;

	// 이동 중지 (관통하지 않는 경우에만 실행됨)
	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->StopMovementImmediately();
		ProjectileMovementComponent->Deactivate();
	}

	// 충돌 비활성화
	SetActorEnableCollision(false);

	// 박힘 처리를 위한 SkeletalMesh 검사 및 처리
	ProcessStickLogic(OtherActor, TargetType, SweepResult);
}

ETargetType AGS_SeekerMerciArrow::DetermineTargetType(AActor* OtherActor) const
{
	if (Cast<AGS_Monster>(OtherActor))
	{
		return ETargetType::DungeonMonster;
	}
	else if (Cast<AGS_Guardian>(OtherActor))
	{
		return ETargetType::Guardian;
	}
	else if (Cast<AGS_AetherExtractor>(OtherActor))
	{
		return ETargetType::AetherExtractor;
	}
	else if (Cast<AGS_Seeker>(OtherActor))
	{
		return ETargetType::Seeker;
	}
	else if (Cast<AGS_SeekerMerciArrow>(OtherActor) || Cast<AGS_FieldSkillActor>(OtherActor))
	{
		return ETargetType::Skill;
	}
	else
	{
		return ETargetType::Structure;
	}
}

bool AGS_SeekerMerciArrow::HandleTargetTypeGeneric(ETargetType TargetType, const FHitResult& SweepResult)
{
	switch (TargetType)
	{
	case ETargetType::Skill:
		break;
	case ETargetType::Structure:
		StickWithVisualOnly(SweepResult);
		break;
	case ETargetType::Seeker:
		break;
	default:
		break;
	}

	return false; // 기본적으로는 이동 중지 (박힘)
}

void AGS_SeekerMerciArrow::ProcessHitEffects(ETargetType TargetType, const FHitResult& SweepResult)
{
	//Crosshair Hit Anim
	if (TargetType == ETargetType::Guardian || TargetType == ETargetType::DungeonMonster)
	{
		if (AGS_Merci* MerciPlayer = Cast<AGS_Merci>(GetOwner()))
		{
			MerciPlayer->Client_ShowCrosshairHitFeedback();
			MerciPlayer->Client_PlayHitFeedbackSound();
		}
	}

	// 히트 사운드 & VFX 재생 (컴포넌트로 위임)
	if (ArrowFXComponent)
	{
		ArrowFXComponent->PlayHitSound(TargetType, SweepResult);
		ArrowFXComponent->PlayHitVFX(TargetType, SweepResult);
	}
}

void AGS_SeekerMerciArrow::ProcessDamageLogic(ETargetType TargetType, const FHitResult& SweepResult, AActor* HitActor)
{
}

void AGS_SeekerMerciArrow::ProcessStickLogic(AActor* HitActor, ETargetType TargetType, const FHitResult& SweepResult)
{
	UE_LOG(LogTemp, Warning, TEXT("=== ProcessStickLogic Debug ==="));
	UE_LOG(LogTemp, Warning, TEXT("Arrow Location: %s"), *GetActorLocation().ToString());
	UE_LOG(LogTemp, Warning, TEXT("Arrow Velocity: %s"), *GetVelocity().ToString());
	UE_LOG(LogTemp, Warning, TEXT("SweepResult Valid: %s"), SweepResult.bBlockingHit ? TEXT("True") : TEXT("False"));
	if (SweepResult.bBlockingHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("SweepResult Impact: %s"), *SweepResult.ImpactPoint.ToString());
		UE_LOG(LogTemp, Warning, TEXT("SweepResult Normal: %s"), *SweepResult.ImpactNormal.ToString());
	}

	// 캐릭터의 SkeletalMeshComponent 찾기
	USkeletalMeshComponent* TargetMesh = Cast<USkeletalMeshComponent>(HitActor->GetComponentByClass(USkeletalMeshComponent::StaticClass()));

	// 로그로 어떤 컴포넌트가 들어오는지 확인
	UE_LOG(LogTemp, Warning, TEXT("OtherComp Type: %s"), SweepResult.Component.IsValid() ? *SweepResult.Component->GetClass()->GetName() : TEXT("NULL"));

	if (TargetMesh)
	{
		FHitResult MeshHit;
		FVector ArrowLocation = GetActorLocation();

		// === 화살 방향 결정 (개선된 방법) ===
		FVector ArrowDirection;

		// 1. SweepResult의 ImpactNormal 활용 (가장 신뢰할 수 있음)
		if (SweepResult.bBlockingHit && !SweepResult.ImpactNormal.IsZero())
		{
			ArrowDirection = -SweepResult.ImpactNormal;
		}
		// 2. 현재 속도 사용
		else if (!GetVelocity().IsZero())
		{
			ArrowDirection = GetVelocity().GetSafeNormal();
		}
		// 3. 폴백: 화살의 Forward 벡터
		else
		{
			ArrowDirection = GetActorForwardVector();
		}

		// === Trace 설정 (더 넓은 범위로) ===
		float TraceDistance = 200.f; // 더 긴 거리

		// 화살 위치 기준으로 앞뒤로 충분히 검사
		FVector TraceStart = ArrowLocation - (ArrowDirection * 100.f);
		FVector TraceEnd = ArrowLocation + (ArrowDirection * 100.f);

		// === LineTrace 시도 ===
		FCollisionQueryParams ComponentTraceParams;
		ComponentTraceParams.bTraceComplex = true;
		ComponentTraceParams.bReturnPhysicalMaterial = false;
		ComponentTraceParams.AddIgnoredActor(this);
		ComponentTraceParams.AddIgnoredActor(GetOwner());
		ComponentTraceParams.AddIgnoredActor(GetInstigator());

		bool bHit = TargetMesh->LineTraceComponent(
		    MeshHit,
		    TraceStart,
		    TraceEnd,
		    ComponentTraceParams);

		// === Component Trace 실패 시 World Trace로 폴백 ===
		if (!bHit)
		{
			FCollisionQueryParams WorldTraceParams = ComponentTraceParams;
			// Capsule이나 다른 collision component 무시
			if (SweepResult.Component.IsValid())
			{
				WorldTraceParams.AddIgnoredComponent(SweepResult.Component.Get());
			}

			bHit = GetWorld()->LineTraceSingleByChannel(
			    MeshHit,
			    TraceStart,
			    TraceEnd,
			    ECC_Pawn, // 또는 커스텀 채널
			    WorldTraceParams);

			// Hit한 Component가 TargetMesh가 아니면 무효
			if (bHit && MeshHit.Component.Get() != TargetMesh)
			{
				bHit = false;
			}
		}

		// === Hit 결과 처리 ===
		if (bHit && MeshHit.Component.Get() == TargetMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("Arrow hit mesh successfully! Bone: %s, Impact: %s"),
			       *MeshHit.BoneName.ToString(),
			       *MeshHit.ImpactPoint.ToString());
#if WITH_EDITOR
			/*FVector CapsuleHitLocation = SweepResult.ImpactPoint;
			FVector MeshHitLocation = MeshHit.ImpactPoint;
			Client_DrawDebugLineTest(CapsuleHitLocation, MeshHitLocation, FColor::Yellow);
			Client_DrawDebugSphereTest(CapsuleHitLocation, FColor::Red);
			Client_DrawDebugSphereTest(MeshHitLocation, FColor::Green);*/
#endif

			StickWithVisualOnly(MeshHit);
		}
		else
		{
			// === 폴백: 수동으로 정확한 Hit 결과 생성 ===
			FHitResult FallbackHit = CreateFallbackHitResult(TargetMesh, HitActor, ArrowLocation, ArrowDirection, SweepResult);

			UE_LOG(LogTemp, Warning, TEXT("Using fallback hit result. Bone: %s, Impact: %s"),
			       *FallbackHit.BoneName.ToString(),
			       *FallbackHit.ImpactPoint.ToString());
			StickWithVisualOnly(FallbackHit);
		}
	}
	else
	{
		// SkeletalMesh가 없는 경우 기본 처리
		UE_LOG(LogTemp, Warning, TEXT("No SkeletalMesh found, using sweep result"));
		StickWithVisualOnly(SweepResult);
	}
}

// === 헬퍼 함수: 폴백 Hit 결과 생성 ===
FHitResult AGS_SeekerMerciArrow::CreateFallbackHitResult(USkeletalMeshComponent* TargetMesh, AActor* HitActor,
                                                         const FVector& ArrowLocation, const FVector& ArrowDirection,
                                                         const FHitResult& OriginalSweepResult)
{
	FHitResult FallbackHit;

	// === 기본 정보 설정 ===
	FallbackHit.Component = TargetMesh;
	FallbackHit.HitObjectHandle = FActorInstanceHandle(HitActor);
	FallbackHit.bBlockingHit = true;

	// === Impact 위치 결정 ===
	FVector ImpactPoint;
	FVector ImpactNormal;

	if (OriginalSweepResult.bBlockingHit)
	{
		// SweepResult가 유효하면 그 정보 사용
		ImpactPoint = OriginalSweepResult.ImpactPoint;
		ImpactNormal = OriginalSweepResult.ImpactNormal;
	}
	else
	{
		// SweepResult가 없으면 화살 위치 기준으로 계산
		ImpactPoint = ArrowLocation;
		ImpactNormal = -ArrowDirection;

		// TargetMesh의 바운딩 박스를 이용해 더 정확한 위치 추정
		FBox MeshBounds = TargetMesh->Bounds.GetBox();
		if (MeshBounds.IsValid)
		{
			// 화살 위치를 바운딩 박스 표면으로 클램프
			ImpactPoint = MeshBounds.GetClosestPointTo(ArrowLocation);
		}
	}

	FallbackHit.ImpactPoint = ImpactPoint;
	FallbackHit.ImpactNormal = ImpactNormal;
	FallbackHit.Distance = FVector::Dist(ArrowLocation, ImpactPoint);

	// === 가장 가까운 Bone 찾기 ===
	FallbackHit.BoneName = FindClosestBoneName(TargetMesh, ImpactPoint);

	return FallbackHit;
}

void AGS_SeekerMerciArrow::Client_DrawDebugLineTest_Implementation(FVector Start, FVector End, FColor Color)
{
	DrawDebugLine(GetWorld(), Start, End, Color, false, 3.0f, 0, 2.0f);
}

void AGS_SeekerMerciArrow::Client_DrawDebugSphereTest_Implementation(FVector Location, FColor Color)
{
	DrawDebugSphere(GetWorld(), Location, 5.0f, 12, Color, false, 3.0f);
}

void AGS_SeekerMerciArrow::OnTargetDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor == HomingTarget)
	{
		Destroy(); // 화살 제거
	}
}

void AGS_SeekerMerciArrow::OnTargetDied()
{
	Destroy();
}