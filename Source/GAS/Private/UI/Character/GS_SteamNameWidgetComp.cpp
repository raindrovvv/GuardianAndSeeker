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

	// 서버에서는 UI 처리 불필요
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 로컬 플레이어 컨트롤러 가져오기
	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!LocalPC || !LocalPC->PlayerCameraManager)
	{
		return;
	}

	// 본인의 네임태그는 항상 숨김
	if (LocalPC->GetPawn() == GetOwner())
	{
		if (IsVisible())
		{
			SetVisibility(false);
		}
		return;
	}

	// 다른 플레이어: 거리 기반 컬링
	FVector CameraLocation = LocalPC->PlayerCameraManager->GetCameraLocation();
	float DistSq = FVector::DistSquared(CameraLocation, GetComponentLocation());

	const float CullDistSq = FMath::Square(GS_Rendering::STEAM_NAME_WIDGET_CULL_DISTANCE);
	bool bInRange = (DistSq < CullDistSq);

	if (IsVisible() != bInRange)
	{
		SetVisibility(bInRange);
	}
}
