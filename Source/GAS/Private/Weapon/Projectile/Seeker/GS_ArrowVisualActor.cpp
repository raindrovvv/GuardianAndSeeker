// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Projectile/Seeker/GS_ArrowVisualActor.h"
#include "Net/UnrealNetwork.h"
#include "Component/GS_VisualPoolComp.h"


AGS_ArrowVisualActor::AGS_ArrowVisualActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false); // 화살은 부착 후 움직이지 않으므로 위치 복제 불필요

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ArrowMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArrowMesh"));
	ArrowMesh->SetIsReplicated(true);
	ArrowMesh->SetupAttachment(Root);

	ArrowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArrowMesh->SetSimulatePhysics(false);
}

void AGS_ArrowVisualActor::OnRep_SkeletalMesh()
{
	if (ArrowMesh && CurrentMesh)
	{
		ArrowMesh->SetSkeletalMesh(CurrentMesh);
	}
}

void AGS_ArrowVisualActor::OnRep_Active()
{
	SetActorHiddenInGame(!bActive);
}

void AGS_ArrowVisualActor::SetArrowMesh(USkeletalMesh* Mesh)
{
	if (ArrowMesh && Mesh)
	{
		ArrowMesh->SetSkeletalMesh(Mesh);
		CurrentMesh = Mesh;
	}
}

void AGS_ArrowVisualActor::SetAttachedTargetActor(AActor* Target)
{
	AttachedTargetActor = Target;
	if (AttachedTargetActor)
	{
		AttachedTargetActor->OnDestroyed.AddDynamic(this, &AGS_ArrowVisualActor::OnAttachedTargetDestroyed);
	}
}

void AGS_ArrowVisualActor::Activate(const FVector& Location, const FRotator& Rotation)
{
	SetActorLocationAndRotation(Location, Rotation);
	SetActorScale3D(FVector::OneVector); // 재사용 시 스케일 초기화 (부착 대상 영향 방지)
	bActive = true;
	OnRep_Active();

	// 생명주기 타이머 설정
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LifeTimeTimerHandle);
		World->GetTimerManager().SetTimer(
		    LifeTimeTimerHandle,
		    this,
		    &AGS_ArrowVisualActor::OnLifeTimeExpired,
		    LifeTime,
		    false);
	}
}

void AGS_ArrowVisualActor::Deactivate()
{
	// 타이머 클리어
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LifeTimeTimerHandle);
	}

	if (AttachedTargetActor)
	{
		AttachedTargetActor->OnDestroyed.RemoveDynamic(this, &AGS_ArrowVisualActor::OnAttachedTargetDestroyed);
		AttachedTargetActor = nullptr;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bActive = false;
	OnRep_Active();
}

void AGS_ArrowVisualActor::OnAttachedTargetDestroyed(AActor* DestroyedActor)
{
	// 풀링 시스템이므로 Destroy가 아니라 ReturnToPool/Deactivate를 사용해야 합니다.
	if (OwningPool)
	{
		OwningPool->ReturnToPool(this);
	}
	else
	{
		Destroy();
	}
}

void AGS_ArrowVisualActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGS_ArrowVisualActor, CurrentMesh);
	DOREPLIFETIME(AGS_ArrowVisualActor, bActive);
}

void AGS_ArrowVisualActor::OnLifeTimeExpired()
{
	if (OwningPool)
	{
		OwningPool->ReturnToPool(this);
	}
	else
	{
		Destroy();
	}
}
