// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Component/GS_DebuffIndicatorComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/Character/GS_DebuffIndicatorWidget.h"
#include "Character/Component/GS_DebuffComp.h"
#include "Character/GS_Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UGS_DebuffIndicatorComponent::UGS_DebuffIndicatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // 타이머 기반 업데이트 사용
	SetIsReplicatedByDefault(false); // 클라이언트 로컬 전용
}

void UGS_DebuffIndicatorComponent::BeginPlay()
{
	Super::BeginPlay();

	// Dedicated Server에서는 시각적 컴포넌트 불필요
	if (!FApp::CanEverRender())
	{
		return;
	}

	// DebuffComp 캐싱
	if (GetOwner())
	{
		CachedDebuffComp = GetOwner()->FindComponentByClass<UGS_DebuffComp>();
	}

	if (!CachedDebuffComp.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebuffIndicator] DebuffComp not found on %s"),
		       GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
		return;
	}

	// 위젯 클래스가 설정되어 있는지 확인
	if (!DebuffIndicatorWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebuffIndicator] DebuffIndicatorWidgetClass not set on %s"),
		       GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
		return;
	}

	// 위젯 컴포넌트 초기화
	InitializeWidgetComponent();

	// DebuffComp 델리게이트 바인딩
	CachedDebuffComp->OnDebuffListUpdated.AddUObject(this, &UGS_DebuffIndicatorComponent::OnDebuffListUpdated);

	// 거리 기반 가시성 업데이트 타이머 (로드 밸런싱을 위한 랜덤 초기 딜레이)
	if (GetWorld())
	{
		float RandomVariance = FMath::RandRange(0.0f, VISIBILITY_UPDATE_INTERVAL);
		GetWorld()->GetTimerManager().SetTimer(
		    VisibilityUpdateTimerHandle,
		    this,
		    &UGS_DebuffIndicatorComponent::UpdateVisibilityByDistance,
		    VISIBILITY_UPDATE_INTERVAL,
		    true, // 반복
		    RandomVariance // 초기 딜레이
		);
	}

	bIsInitialized = true;
}

void UGS_DebuffIndicatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 타이머 정리
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(VisibilityUpdateTimerHandle);
	}

	// 델리게이트 언바인딩
	if (CachedDebuffComp.IsValid())
	{
		CachedDebuffComp->OnDebuffListUpdated.RemoveAll(this);
	}

	// 위젯 컴포넌트 정리 (DefaultSubobject가 아니므로 수동 정리)
	if (DebuffWidgetComponent && !DebuffWidgetComponent->IsBeingDestroyed())
	{
		DebuffWidgetComponent->SetWidget(nullptr);
		DebuffWidgetComponent->DestroyComponent();
		DebuffWidgetComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UGS_DebuffIndicatorComponent::InitializeWidgetComponent()
{
	if (!GetOwner())
	{
		return;
	}

	// 위젯 컴포넌트 생성
	DebuffWidgetComponent = NewObject<UWidgetComponent>(GetOwner(), TEXT("DebuffIndicatorWidgetComp"));
	if (!DebuffWidgetComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[DebuffIndicator] Failed to create WidgetComponent"));
		return;
	}

	// 루트 컴포넌트에 부착 (RegisterComponent 전에 수행해야 함)
	USceneComponent* RootComp = GetOwner()->GetRootComponent();
	if (RootComp)
	{
		DebuffWidgetComponent->SetupAttachment(RootComp);
	}

	// 컴포넌트 등록
	DebuffWidgetComponent->RegisterComponent();

	// 위치 설정 (머리 위)
	DebuffWidgetComponent->SetRelativeLocation(WidgetLocationOffset);

	// Screen Space 설정 (항상 카메라를 향함)
	DebuffWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	DebuffWidgetComponent->SetDrawAtDesiredSize(true);

	// 콜리전 비활성화
	DebuffWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 위젯 클래스 설정
	DebuffWidgetComponent->SetWidgetClass(DebuffIndicatorWidgetClass);

	// 위젯 인스턴스 캐싱 (위젯은 SetWidgetClass 호출 후 자동 생성됨)
	// 주의: GetWidget()은 위젯이 아직 생성되지 않았을 수 있음
	// NativeConstruct 이후에만 유효하므로 타이머로 지연 캐싱
	if (GetWorld())
	{
		// 람다에서 안전한 캡처를 위해 TWeakObjectPtr 사용
		TWeakObjectPtr<UGS_DebuffIndicatorComponent> WeakThis(this);
		TWeakObjectPtr<UWidgetComponent> WeakWidgetComp(DebuffWidgetComponent);

		GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis, WeakWidgetComp]()
		                                                  {
			if (WeakThis.IsValid() && WeakWidgetComp.IsValid())
			{
				WeakThis->DebuffIndicatorWidget = Cast<UGS_DebuffIndicatorWidget>(WeakWidgetComp->GetWidget());
			} });
	}

	// 초기에는 숨김
	SetIndicatorVisible(false);
}

void UGS_DebuffIndicatorComponent::SetIndicatorVisible(bool bVisible)
{
	bIsVisible = bVisible;

	if (DebuffWidgetComponent)
	{
		DebuffWidgetComponent->SetVisibility(bVisible);
	}

	if (!bVisible && DebuffIndicatorWidget)
	{
		DebuffIndicatorWidget->HideAllIcons();
	}
}

void UGS_DebuffIndicatorComponent::OnDebuffListUpdated(const TArray<FDebuffRepInfo>& DebuffList)
{
	if (!bIsInitialized || !bIsVisible)
	{
		return;
	}

	// 디버프가 없으면 숨김
	if (DebuffList.Num() == 0)
	{
		if (DebuffIndicatorWidget)
		{
			DebuffIndicatorWidget->HideAllIcons();
		}
		return;
	}

	// 우선순위 정렬
	TArray<FDebuffRepInfo> SortedDebuffs = SortDebuffsByPriority(DebuffList);

	// 위젯 업데이트
	if (DebuffIndicatorWidget)
	{
		DebuffIndicatorWidget->UpdateDebuffIcons(SortedDebuffs, MaxDisplayIcons);
	}
}

TArray<FDebuffRepInfo> UGS_DebuffIndicatorComponent::SortDebuffsByPriority(
    const TArray<FDebuffRepInfo>& DebuffList) const
{
	TArray<FDebuffRepInfo> Sorted = DebuffList;

	// 우선순위 기준 정렬 (FDebuffTypeUtils 사용)
	Sorted.Sort([](const FDebuffRepInfo& A, const FDebuffRepInfo& B)
	            { return FDebuffTypeUtils::GetDisplayPriority(A.Type) < FDebuffTypeUtils::GetDisplayPriority(B.Type); });

	return Sorted;
}

int32 UGS_DebuffIndicatorComponent::GetDebuffDisplayPriority(EDebuffType Type) const
{
	// 중앙 집중화된 유틸리티 함수로 위임
	return FDebuffTypeUtils::GetDisplayPriority(Type);
}

void UGS_DebuffIndicatorComponent::UpdateVisibilityByDistance()
{
	if (!GetOwner() || !bIsInitialized)
	{
		return;
	}

	// DebuffComp 유효성 확인
	if (!CachedDebuffComp.IsValid())
	{
		SetIndicatorVisible(false);
		return;
	}

	// 로컬 플레이어 컨트롤러 가져오기
	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!LocalPC)
	{
		SetIndicatorVisible(false);
		return;
	}

	APawn* LocalPawn = LocalPC->GetPawn();
	if (!LocalPawn)
	{
		SetIndicatorVisible(false);
		return;
	}

	// 자기 자신이면 표시하지 않음 (자신의 디버프는 팀 HUD에서 표시)
	if (LocalPawn == GetOwner())
	{
		SetIndicatorVisible(false);
		return;
	}

	// 거리 계산
	float DistanceSq = FVector::DistSquared(
	    LocalPawn->GetActorLocation(),
	    GetOwner()->GetActorLocation());

	// 거리 내에 있으면 표시
	bool bShouldBeVisible = DistanceSq <= FMath::Square(VisibilityDistance);

	// 디버프가 있는지도 확인
	if (bShouldBeVisible)
	{
		bShouldBeVisible = CachedDebuffComp->GetDebuffList().Num() > 0;
	}

	if (bShouldBeVisible != bIsVisible)
	{
		SetIndicatorVisible(bShouldBeVisible);

		// 표시되면 즉시 업데이트
		if (bShouldBeVisible)
		{
			OnDebuffListUpdated(CachedDebuffComp->GetDebuffList());
		}
	}
}
