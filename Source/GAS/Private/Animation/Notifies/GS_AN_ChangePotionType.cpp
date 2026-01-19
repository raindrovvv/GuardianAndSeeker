// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Animation/Notifies/GS_AN_ChangePotionType.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Props/Item/SeekerItem/GS_HP_Potion.h"
#include "Components/SkeletalMeshComponent.h"

UGS_AN_ChangePotionType::UGS_AN_ChangePotionType()
{
}

void UGS_AN_ChangePotionType::Notify(USkeletalMeshComponent* MeshComp,
									 UAnimSequenceBase* Animation,
									 const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	AGS_Seeker* Seeker = Cast<AGS_Seeker>(MeshComp->GetOwner());
	if (!Seeker)
	{
		return;
	}

	// Locate the potion item in the seeker's inventory
	AGS_HP_Potion* Potion = Cast<AGS_HP_Potion>(Seeker->GetItem(EItemType::HP_Potion));
	if (Potion)
	{
		// Apply the visual mesh change
		Potion->ApplyMeshVariant(TargetMeshVariantName);

		// Ensure it stays correctly attached to the character's hand/socket during the animation
		Potion->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetIncludingScale, FName("Potion"));

		// Disable collision with players while being handled
		if (UStaticMeshComponent* VisualMesh = Potion->GetVisualMesh())
		{
			VisualMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		}
	}
}
