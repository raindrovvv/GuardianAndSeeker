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

void UGS_DamageNumberComponent::ShowDamageNumber(float Damage, EDamageNumberType Type, FVector WorldLocation, AActor* TargetActor)
{
	// 서버에서만 RPC 호출
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Multicast_ShowDamageNumber(Damage, Type, WorldLocation, TargetActor);
	}
}

void UGS_DamageNumberComponent::Multicast_ShowDamageNumber_Implementation(float Damage, EDamageNumberType Type, FVector WorldLocation, AActor* TargetActor)
{
	// 데디케이티드 서버에서는 UI 표시 안함
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ShowDamageNumberInternal(Damage, Type, WorldLocation, TargetActor);
}

void UGS_DamageNumberComponent::ShowDamageNumberInternal(float Damage, EDamageNumberType Type, FVector WorldLocation, AActor* TargetActor)
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

	// ====== 데미지 누적 시스템 ======
	// 동일 타겟에 대해 활성화된 위젯이 있으면 데미지 누적
	if (bEnableAccumulation && TargetActor != nullptr)
	{
		UGS_DamageNumberWidget* ExistingWidget = FindActiveWidgetForTarget(TargetActor);
		if (ExistingWidget && ExistingWidget->AddDamage(Damage))
		{
			// 누적 성공 - 새 위젯 생성하지 않음
			return;
		}
	}

	// ====== 거리 기반 스케일 계산 ======
	float DistanceScale = 1.0f;
	if (bEnableDistanceScaling && LocalPC->GetPawn())
	{
		const float Distance = FVector::Dist(WorldLocation, LocalPC->GetPawn()->GetActorLocation());
		// 거리에 따른 스케일 보간 (Near -> Max, Far -> Min)
		DistanceScale = FMath::GetMappedRangeValueClamped(
		    FVector2D(NearDistance, FarDistance),
		    FVector2D(MaxDistanceScale, MinDistanceScale),
		    Distance);
	}

	// 위치에 랜덤 오프셋 추가 (겹침 방지)
	ScreenPosition.X += FMath::RandRange(-RandomOffset, RandomOffset);
	ScreenPosition.Y += FMath::RandRange(-RandomOffset * 0.5f, RandomOffset * 0.5f);

	// ====== 스마트 겹침 방지 ======
	if (bEnableOverlapPrevention)
	{
		// 거리 제곱을 루프 밖에서 미리 계산 (최적화)
		const float OverlapRadiusSq = FMath::Square(OverlapDetectionRadius);
		const int32 MaxOverlapIterations = 5; // 무한 루프 방지

		for (int32 Iteration = 0; Iteration < MaxOverlapIterations; ++Iteration)
		{
			bool bFoundOverlap = false;

			for (UGS_DamageNumberWidget* ExistingWidget : ActiveWidgets)
			{
				if (!IsValid(ExistingWidget) || !ExistingWidget->IsActive())
				{
					continue;
				}

				const FVector2D ExistingPos = ExistingWidget->GetCurrentScreenPosition();
				const float DistanceSq = FVector2D::DistSquared(ScreenPosition, ExistingPos);

				if (DistanceSq < OverlapRadiusSq)
				{
					// 겹침 감지 - 위로 밀어냄
					ScreenPosition.Y -= OverlapYOffset;
					bFoundOverlap = true;
					break;
				}
			}

			if (!bFoundOverlap)
			{
				break;
			}
		}
	}

	// 풀에서 위젯 가져오기
	UGS_DamageNumberWidget* Widget = GetPooledWidget(LocalPC);
	if (!Widget)
	{
		return;
	}

	// 타겟 액터 설정 (누적 시스템용)
	Widget->SetTargetActor(TargetActor);

	// 데미지 표시 시작 (거리 스케일 적용)
	Widget->ShowDamage(Damage, Type, ScreenPosition, DisplayDuration, DistanceScale);

	// 애니메이션 완료 시 풀에 반환 (기존 바인딩 해제 후 재바인딩)
	Widget->OnAnimationComplete.Unbind();
	Widget->OnAnimationComplete.BindUObject(this, &UGS_DamageNumberComponent::ReturnToPool);
}

UGS_DamageNumberWidget* UGS_DamageNumberComponent::FindActiveWidgetForTarget(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return nullptr;
	}

	for (UGS_DamageNumberWidget* Widget : ActiveWidgets)
	{
		if (IsValid(Widget) && Widget->IsActive() && Widget->GetTargetActor() == TargetActor)
		{
			return Widget;
		}
	}

	return nullptr;
}

UGS_DamageNumberWidget* UGS_DamageNumberComponent::GetPooledWidget(APlayerController* PC)
{
	if (!PC)
	{
		return nullptr;
	}

	// 풀에서 사용 가능한 위젯 찾기 (무효한 위젯은 건너뛰기)
	while (WidgetPool.Num() > 0)
	{
		UGS_DamageNumberWidget* Widget = WidgetPool.Pop();
		if (IsValid(Widget))
		{
			Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
			ActiveWidgets.Add(Widget);
			return Widget;
		}
		// Invalid한 위젯은 버리고 다음 시도
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
	for (int32 i = 0; i < ActiveWidgets.Num(); ++i)
	{
		if (IsValid(ActiveWidgets[i]))
		{
			UGS_DamageNumberWidget* OldestWidget = ActiveWidgets[i];
			ActiveWidgets.RemoveAt(i);
			ActiveWidgets.Add(OldestWidget);
			return OldestWidget;
		}
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
