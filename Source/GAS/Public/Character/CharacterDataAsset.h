// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "CharacterDataAsset.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class GAS_API UCharacterDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	// Soft Reference로 메모리 최적화 - 필요할 때만 로드
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI")
	TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI")
	FText CharacterName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI")
	FText TypeName;
};
