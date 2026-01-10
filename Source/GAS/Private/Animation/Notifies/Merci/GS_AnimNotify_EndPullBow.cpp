// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/Notifies/Merci/GS_AnimNotify_EndPullBow.h"
#include "Character/Player/Seeker/GS_Merci.h"

void UGS_AnimNotify_EndPullBow::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	UE_LOG(LogTemp, Error, TEXT("========== [AnimNotify_EndPullBow] FIRED! =========="));

	if (!MeshComp)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimNotify_EndPullBow] MeshComp is NULL!"));
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_EndPullBow] Owner: %s"), Owner ? *Owner->GetName() : TEXT("NULL"));

	if (AGS_Merci* MerciCharacter = Cast<AGS_Merci>(Owner))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_EndPullBow] Merci cast success - HasAuthority=%d"), MerciCharacter->HasAuthority());
		// 🔴 CRITICAL: Always call on client where animation plays
		// OnDrawMontageEnded will handle server RPC internally
		MerciCharacter->OnDrawMontageEnded();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimNotify_EndPullBow] Failed to cast to Merci!"));
	}
}
