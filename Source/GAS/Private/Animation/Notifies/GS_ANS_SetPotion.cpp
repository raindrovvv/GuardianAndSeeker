// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/GS_ANS_SetPotion.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Props/Item/SeekerItem/GS_HP_Potion.h"
#include "Engine/World.h"

UGS_ANS_SetPotion::UGS_ANS_SetPotion()
{
}

void UGS_ANS_SetPotion::NotifyBegin(USkeletalMeshComponent* MeshComp,
									UAnimSequenceBase* Animation,
									float TotalDuration,
									const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	AGS_Seeker* Seeker = Cast<AGS_Seeker>(MeshComp->GetOwner());
	if (!Seeker)
	{
		return;
	}

	UWorld* World = Seeker->GetWorld();
	if (!World)
	{
		return;
	}

	// Spawn parameters for the potion actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Seeker;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Spawn a new HP potion instance
	AGS_HP_Potion* Potion = World->SpawnActor<AGS_HP_Potion>(AGS_HP_Potion::StaticClass(), SpawnParams);
	if (Potion)
	{
		// Initialize the potion with seeker's item data and default visual
		Potion->AssignItemData(Seeker->GetItemData(EItemType::HP_Potion));
		Potion->ApplyMeshVariant(FName(TEXT("HP_Potion_Full")));

		// Track the potion globally/per-character if needed
		Seeker->Items.Add(EItemType::HP_Potion, Potion);

		// Attach to the character's potion socket
		Potion->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetIncludingScale, FName("Potion"));
	}
}

void UGS_ANS_SetPotion::NotifyEnd(USkeletalMeshComponent* MeshComp,
								  UAnimSequenceBase* Animation,
								  const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	AGS_Seeker* Seeker = Cast<AGS_Seeker>(MeshComp->GetOwner());
	if (!Seeker)
	{
		return;
	}

	// Retrieve the active potion to release it
	AGS_HP_Potion* Potion = Cast<AGS_HP_Potion>(Seeker->GetItem(EItemType::HP_Potion));
	if (Potion)
	{
		// Trigger the drop/destruction sequence
		Potion->ReleaseFromHolder();

		// Ensure physics are enabled for a realistic drop effect
		if (UStaticMeshComponent* VisualMesh = Potion->GetVisualMesh())
		{
			VisualMesh->SetSimulatePhysics(true);
			VisualMesh->SetEnableGravity(true);
			VisualMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}
}
