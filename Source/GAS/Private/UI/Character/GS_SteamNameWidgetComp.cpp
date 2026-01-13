#include "UI/Character/GS_SteamNameWidgetComp.h"

#include "UI/Character/GS_SteamNameWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/GS_RenderingConstants.h"

UGS_SteamNameWidgetComp::UGS_SteamNameWidgetComp()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UGS_SteamNameWidgetComp::BeginPlay()
{
	Super::BeginPlay();

	if (GetWidget())
	{
		if (GetWidget()->IsA<UGS_SteamNameWidget>())
		{
			UGS_SteamNameWidget* NameWidget = Cast<UGS_SteamNameWidget>(GetWidget());
			if (IsValid(NameWidget))
			{
				NameWidget->SetOwningActor(GetOwner());
			}
		}
	}
}

void UGS_SteamNameWidgetComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
