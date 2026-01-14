#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "GS_SteamNameWidgetComp.generated.h"


UCLASS()
class GAS_API UGS_SteamNameWidgetComp : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UGS_SteamNameWidgetComp();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
