// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Projectile/Seeker/GS_SeekerMerciArrowSmoke.h"
#include "Character/Skill/Seeker/Merci/GS_SmokeFieldSkill.h"

void AGS_SeekerMerciArrowSmoke::OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Overlap 시 ImpactPoint가 0인 경우 보정
	FHitResult FixedHitResult = SweepResult;
	if (FixedHitResult.ImpactPoint.IsZero())
	{
		FixedHitResult.ImpactPoint = GetActorLocation();
		FixedHitResult.ImpactNormal = -GetActorForwardVector();
	}

	Super::OnBeginOverlap(OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, FixedHitResult);

	ETargetType TargetType = DetermineTargetType(OtherActor);

	if (TargetType == ETargetType::Guardian || TargetType == ETargetType::DungeonMonster)
	{
		SpawnSmokeArea(FixedHitResult.ImpactPoint);
		StickWithVisualOnly(FixedHitResult);
	}
	else if (TargetType == ETargetType::Structure)
	{
		SpawnSmokeArea(FixedHitResult.ImpactPoint);
	}
}

void AGS_SeekerMerciArrowSmoke::SpawnSmokeArea(FVector SpawnLocation)
{
	if (SmokeAreaClass)
	{
		GetWorld()->SpawnActor<AGS_SmokeFieldSkill>(SmokeAreaClass, SpawnLocation, FRotator::ZeroRotator);
	}
}
