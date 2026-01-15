// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Component/GS_DamageNumberComponent.h"
#include "UI/Damage/GS_DamageNumberWidget.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

UGS_DamageNumberComponent::UGS_DamageNumberComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGS_DamageNumberComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UGS_DamageNumberComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 모든 위젯 정리
	for (UGS_DamageNumberWidget* Widget : ActiveWidgets)
	{
		if (IsValid(Widget))
		{
			Widget->RemoveFromParent();
		}
	}
	ActiveWidgets.Empty();

	for (UGS_DamageNumberWidget* Widget : WidgetPool)
	{
		if (IsValid(Widget))
		{
			Widget->RemoveFromParent();
		}
	}
	WidgetPool.Empty();

	Super::EndPlay(EndPlayReason);
}

void UGS_DamageNumberComponent::ShowDamageNumber(float Damage, EDamageNumberType Type, FVector WorldLocation)
{
	// 서버에서만 RPC 호출
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Multicast_ShowDamageNumber(Damage, Type, WorldLocation);
	}
}

void UGS_DamageNumberComponent::Multicast_ShowDamageNumber_Implementation(float Damage, EDamageNumberType Type, FVector WorldLocation)
{
	// 데디케이티드 서버에서는 UI 표시 안함
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ShowDamageNumberInternal(Damage, Type, WorldLocation);
}

void UGS_DamageNumberComponent::ShowDamageNumberInternal(float Damage, EDamageNumberType Type, FVector WorldLocation)
{
	if (!DamageNumberWidgetClass)
	{
		return;
	}

	// 소유자(공격자)가 로컬 플레이어인 경우에만 해당 클라이언트에서 표시
	// 멀티플레이어에서 각 클라이언트는 자신이 로컬 컨트롤한 캐릭터의 공격에 대해서만 표시
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	// 로컬 플레이어 컨트롤러 가져오기
	APlayerController* LocalPC = nullptr;

	// 소유자가 로컬 플레이어가 조종하는 폰인지 확인
	if (OwnerPawn->IsLocallyControlled())
	{
		LocalPC = Cast<APlayerController>(OwnerPawn->GetController());
	}
	else
	{
		// 로컬 컨트롤이 아니면 표시하지 않음 (다른 플레이어의 공격)
		return;
	}

	if (!LocalPC)
	{
		return;
	}

	// 월드 좌표를 스크린 좌표로 변환
	FVector2D ScreenPosition;
	bool bIsOnScreen = LocalPC->ProjectWorldLocationToScreen(WorldLocation, ScreenPosition, true);
	if (!bIsOnScreen)
	{
		return;
	}

	// 위치에 랜덤 오프셋 추가 (겹침 방지)
	ScreenPosition.X += FMath::RandRange(-RandomOffset, RandomOffset);
	ScreenPosition.Y += FMath::RandRange(-RandomOffset * 0.5f, RandomOffset * 0.5f);

	// 풀에서 위젯 가져오기
	UGS_DamageNumberWidget* Widget = GetPooledWidget(LocalPC);
	if (!Widget)
	{
		return;
	}

	// 데미지 표시 시작
	Widget->ShowDamage(Damage, Type, ScreenPosition, DisplayDuration);

	// 애니메이션 완료 시 풀에 반환
	Widget->OnAnimationComplete.BindUObject(this, &UGS_DamageNumberComponent::ReturnToPool);
}

UGS_DamageNumberWidget* UGS_DamageNumberComponent::GetPooledWidget(APlayerController* PC)
{
	if (!PC)
	{
		return nullptr;
	}

	// 풀에서 사용 가능한 위젯 찾기
	if (WidgetPool.Num() > 0)
	{
		UGS_DamageNumberWidget* Widget = WidgetPool.Pop();
		if (IsValid(Widget))
		{
			Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
			ActiveWidgets.Add(Widget);
			return Widget;
		}
	}

	// 풀이 비어있으면 새 위젯 생성
	if (ActiveWidgets.Num() + WidgetPool.Num() < MaxPoolSize)
	{
		UGS_DamageNumberWidget* NewWidget = CreateWidget<UGS_DamageNumberWidget>(PC, DamageNumberWidgetClass);
		if (NewWidget)
		{
			NewWidget->AddToViewport(100); // HUD 위에 표시
			ActiveWidgets.Add(NewWidget);
			return NewWidget;
		}
	}

	// 풀 한도 초과 - 가장 오래된 활성 위젯 재사용
	if (ActiveWidgets.Num() > 0)
	{
		UGS_DamageNumberWidget* OldestWidget = ActiveWidgets[0];
		ActiveWidgets.RemoveAt(0);
		ActiveWidgets.Add(OldestWidget);
		return OldestWidget;
	}

	return nullptr;
}

void UGS_DamageNumberComponent::ReturnToPool(UGS_DamageNumberWidget* Widget)
{
	if (!IsValid(Widget))
	{
		return;
	}

	// 중복 제거 방지
	int32 RemovedCount = ActiveWidgets.Remove(Widget);
	if (RemovedCount == 0)
	{
		// 이미 풀에 있거나 관리되지 않는 위젯
		return;
	}

	Widget->SetVisibility(ESlateVisibility::Collapsed);
	Widget->OnAnimationComplete.Unbind();

	WidgetPool.Add(Widget);
}
