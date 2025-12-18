// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/RTS/GS_MinimapWidget.h"
#include "AI/RTS/GS_RTSController.h"
#include "AI/RTS/GS_RTSCamera.h"
#include "AI/RTS/GS_MinimapCapture.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SceneCaptureComponent2D.h"
#include "EngineUtils.h"
#include "AI/RTS/GS_RTSAttackNotificationManager.h"

UGS_MinimapWidget::UGS_MinimapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsDraggingViewBox = false;
	bGeometryCached = false;
}

void UGS_MinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitializeReferences();
	BindDelegates();
	StartTimers();

	// 미니맵 위젯 전체에 클리핑 적용 (영역 밖으로 나가는 뷰박스/아이콘 자르기)
	SetClipping(EWidgetClipping::ClipToBounds);
}

void UGS_MinimapWidget::NativeDestruct()
{
	ClearTimers();         // CRITICAL: 타이머 정리
	UnbindDelegates();     // 델리게이트 정리
	ClearAllIcons();       // 아이콘 정리

	Super::NativeDestruct();
}

void UGS_MinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Geometry 캐싱 (좌표 변환에 필요)
	CachedMinimapGeometry = MyGeometry;
	bGeometryCached = true;

	// Update attack warnings
	UpdateAttackWarnings(InDeltaTime);
}

// ========== Core Functions ==========

void UGS_MinimapWidget::InitializeReferences()
{
	CachedRTSController = Cast<AGS_RTSController>(GetOwningPlayer());
	if (!CachedRTSController) return;

	// RTSCamera 찾기
	UWorld* World = GetWorld();
	if (World)
	{
		for (TActorIterator<AGS_RTSCamera> It(World); It; ++It)
		{
			CachedRTSCamera = *It;
			break;
		}

		// MinimapCapture 찾아서 WorldBounds 동기화 (좌표 오차 해결의 핵심)
		for (TActorIterator<AGS_MinimapCapture> It(World); It; ++It)
		{
			AGS_MinimapCapture* MinimapCapture = *It;
			if (MinimapCapture && MinimapCapture->SceneCaptureComponent)
			{
				float OrthoWidth = MinimapCapture->SceneCaptureComponent->OrthoWidth;
				FVector CaptureLoc = MinimapCapture->GetActorLocation();

				// 캡처 영역의 절반 크기 (Radius)
				float HalfSize = OrthoWidth * 0.5f;

				// WorldBounds 설정 (Min, Max)
				// 중요: 캡처 카메라는 중심에 있으므로, Min = Center - Half, Max = Center + Half
				WorldBounds.Min = FVector2D(CaptureLoc.X - HalfSize, CaptureLoc.Y - HalfSize);
				WorldBounds.Max = FVector2D(CaptureLoc.X + HalfSize, CaptureLoc.Y + HalfSize);

				break;
			}
		}

		// Register self to AttackNotificationManager if available
		if (CachedRTSController->AttackNotificationManager)
		{
			CachedRTSController->AttackNotificationManager->SetMinimapWidget(this);
			//UE_LOG(LogTemp, Log, TEXT("[MinimapWidget] Registered self to AttackNotificationManager"));
		}
	}
}

void UGS_MinimapWidget::BindDelegates()
{
	if (CachedRTSController)
	{
		// 유닛 선택 변경 시 아이콘 갱신
		CachedRTSController->OnSelectionChanged.AddDynamic(this, &UGS_MinimapWidget::OnSelectionChanged);

		// RTS 명령 변경 시 (선택 사항 - 시각적 피드백)
		CachedRTSController->OnRTSCommandChanged.AddDynamic(this, &UGS_MinimapWidget::OnRTSCommandChanged);
	}
}

void UGS_MinimapWidget::UnbindDelegates()
{
	if (CachedRTSController)
	{
		CachedRTSController->OnSelectionChanged.RemoveDynamic(this, &UGS_MinimapWidget::OnSelectionChanged);
		CachedRTSController->OnRTSCommandChanged.RemoveDynamic(this, &UGS_MinimapWidget::OnRTSCommandChanged);
	}
}

void UGS_MinimapWidget::StartTimers()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 아이콘 업데이트 타이머 (20 FPS)
	World->GetTimerManager().SetTimer(
		TimerHandle_IconUpdate,
		this,
		&UGS_MinimapWidget::UpdateUnitIcons,
		IconUpdateInterval,  // 0.05초
		true  // Loop
	);

	// 뷰 박스 업데이트 타이머 (30 FPS)
	World->GetTimerManager().SetTimer(
		TimerHandle_ViewBoxUpdate,
		this,
		&UGS_MinimapWidget::UpdateCameraViewBox,
		ViewBoxUpdateInterval,  // 0.033초
		true  // Loop
	);
}

void UGS_MinimapWidget::ClearTimers()
{
	UWorld* World = GetWorld();
	if (!World) return;

	World->GetTimerManager().ClearTimer(TimerHandle_IconUpdate);
	World->GetTimerManager().ClearTimer(TimerHandle_ViewBoxUpdate);
}

// ========== Delegate Callbacks ==========

void UGS_MinimapWidget::OnSelectionChanged(const TArray<AGS_Monster*>& NewSelection)
{
	// 유닛 선택 변경 시 아이콘 즉시 업데이트
	UpdateUnitIcons();
}

void UGS_MinimapWidget::OnRTSCommandChanged(ERTSCommand NewCommand)
{
	// TODO: 명령 모드에 따라 커서 또는 UI 피드백 변경 (선택 사항)
}

// ========== Timer Callbacks ==========

void UGS_MinimapWidget::UpdateUnitIcons()
{
	if (!CachedRTSController || !IconContainer) return;

	const TArray<AGS_Monster*>& SelectedUnits = CachedRTSController->GetUnitSelection();

	// 1. 기존 아이콘 중 유효하지 않은 것 제거
	TArray<TObjectPtr<AGS_Monster>> ToRemove;
	for (auto& Pair : ActiveIcons)
	{
		AGS_Monster* Unit = Pair.Key;
		if (!IsValid(Unit) || !SelectedUnits.Contains(Unit))
		{
			ToRemove.Add(Unit);
		}
	}

	for (AGS_Monster* Unit : ToRemove)
	{
		RemoveIconForUnit(Unit);
	}

	// 2. 새로운 유닛 아이콘 생성/업데이트
	for (AGS_Monster* Unit : SelectedUnits)
	{
		if (IsValid(Unit))
		{
			UpdateIconForUnit(Unit);
		}
	}
}

void UGS_MinimapWidget::UpdateCameraViewBox()
{
	if (!CachedRTSCamera || !CameraViewBox) return;

	// 드래그 중일 때는 뷰박스 위치를 마우스 입력(HandleDrag)이 제어하므로
	// 카메라 위치 기반 업데이트를 건너뛰어 떨림(Jittering) 방지
	if (bIsDraggingViewBox) return;

	RefreshViewBoxTransform();
}

// ========== Icon Management ==========

UImage* UGS_MinimapWidget::AcquireIconWidget()
{
	// 풀에서 재사용
	if (InactiveIconPool.Num() > 0)
	{
		UImage* IconWidget = InactiveIconPool.Pop();
		IconWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		return IconWidget;
	}

	// 풀이 비었으면 새로 생성
	if (IconContainer)
	{
		UImage* NewIcon = NewObject<UImage>(IconContainer);
		NewIcon->SetDesiredSizeOverride(IconSize);

		UCanvasPanelSlot* CanvasSlot = IconContainer->AddChildToCanvas(NewIcon);
		if (CanvasSlot)
		{
			CanvasSlot->SetSize(IconSize);
			CanvasSlot->SetZOrder(10); // 뷰 박스 위에 렌더링
		}

		return NewIcon;
	}

	return nullptr;
}

void UGS_MinimapWidget::ReleaseIconWidget(UImage* IconWidget)
{
	if (!IconWidget) return;

	// 풀 크기 제한 (메모리 누수 방지)
	if (InactiveIconPool.Num() >= MaxIconPoolSize)
	{
		IconWidget->RemoveFromParent();
		return;
	}

	// 풀로 반환
	IconWidget->SetVisibility(ESlateVisibility::Collapsed);
	InactiveIconPool.Add(IconWidget);
}

void UGS_MinimapWidget::UpdateIconForUnit(AGS_Monster* Unit)
{
	if (!Unit || !IconContainer) return;

	FGS_MinimapIconData* IconData = ActiveIcons.Find(Unit);

	// 아이콘이 없으면 생성
	if (!IconData)
	{
		FGS_MinimapIconData NewIconData;
		NewIconData.TrackedUnit = Unit;
		NewIconData.IconWidget = AcquireIconWidget();

		// 아이콘 텍스처 설정 (CharacterType 기반)
		if (NewIconData.IconWidget)
		{
			ECharacterType CharacterType = Unit->GetCharacterType();
			FName CharacterTypeName = *UEnum::GetValueAsString(CharacterType);
			UTexture2D** IconTexture = UnitIconTextures.Find(CharacterTypeName);

			if (IconTexture && *IconTexture)
			{
				NewIconData.IconWidget->SetBrushFromTexture(*IconTexture);
			}

		}

		ActiveIcons.Add(Unit, NewIconData);
		IconData = &ActiveIcons[Unit];
	}

	// 월드 위치 -> 미니맵 좌표
	FVector UnitWorldLocation = Unit->GetActorLocation();
	FVector2D NormalizedPos = WorldToMinimapNormalized(UnitWorldLocation);

	// 캐싱된 Geometry 사용
	if (!bGeometryCached) return;

	FVector2D MinimapSize = CachedMinimapGeometry.GetLocalSize();
	FVector2D ScreenPos = NormalizedPos * MinimapSize;

	// 아이콘 위치 업데이트 (Canvas Slot 이용)
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(IconData->IconWidget->Slot))
	{
		// 중앙 정렬을 위해 아이콘 크기의 절반만큼 오프셋
		FVector2D AdjustedPos = ScreenPos - (IconSize * 0.5f);
		CanvasSlot->SetPosition(AdjustedPos);
		CanvasSlot->SetSize(IconSize);
	}

	IconData->LastScreenPosition = ScreenPos;
}

void UGS_MinimapWidget::RemoveIconForUnit(AGS_Monster* Unit)
{
	if (!Unit) return;

	FGS_MinimapIconData IconData;
	if (ActiveIcons.RemoveAndCopyValue(Unit, IconData))
	{
		ReleaseIconWidget(IconData.IconWidget);
	}
}

void UGS_MinimapWidget::ClearAllIcons()
{
	for (auto& Pair : ActiveIcons)
	{
		ReleaseIconWidget(Pair.Value.IconWidget);
	}
	ActiveIcons.Empty();
}

// ========== Coordinate Conversion ==========

FVector2D UGS_MinimapWidget::WorldToMinimapNormalized(const FVector& WorldLocation) const
{
	FVector2D WorldPos2D(WorldLocation.X, WorldLocation.Y);
	FVector2D MinimapMin = WorldBounds.Min;
	FVector2D MinimapMax = WorldBounds.Max;

	// 정규화: (WorldPos - Min) / (Max - Min)
	// 중요: 미니맵은 위쪽이 World X(Forward), 오른쪽이 World Y(Right)임.
	// UI 좌표계: X(가로), Y(세로, 아래로 갈수록 증가)
	
	FVector2D Normalized;

	// 1. UI X (가로) <-> World Y (좌우)
	Normalized.X = (WorldPos2D.Y - MinimapMin.Y) / (MinimapMax.Y - MinimapMin.Y);

	// 2. UI Y (세로) <-> World X (상하)
	// World X가 커질수록(위쪽), UI Y는 작아져야 함(위쪽) -> 반전(1.0 - Value)
	float NormalizedWorldX = (WorldPos2D.X - MinimapMin.X) / (MinimapMax.X - MinimapMin.X);
	Normalized.Y = 1.0f - NormalizedWorldX;

	return Normalized;
}

FVector UGS_MinimapWidget::MinimapScreenToWorld(const FVector2D& ScreenPosition) const
{
	if (!bGeometryCached) return FVector::ZeroVector;

	FVector2D MinimapSize = CachedMinimapGeometry.GetLocalSize();

	// 스크린 픽셀 -> 정규화 (0~1)
	FVector2D Normalized;
	Normalized.X = ScreenPosition.X / MinimapSize.X;
	Normalized.Y = ScreenPosition.Y / MinimapSize.Y;

	// 클램프
	Normalized.X = FMath::Clamp(Normalized.X, 0.0f, 1.0f);
	Normalized.Y = FMath::Clamp(Normalized.Y, 0.0f, 1.0f);

	// 정규화 -> 월드 좌표 (WorldToMinimapNormalized의 역연산)
	// UI X -> World Y
	FVector2D WorldPos2D;
	WorldPos2D.Y = FMath::Lerp(WorldBounds.Min.Y, WorldBounds.Max.Y, Normalized.X);

	// UI Y -> World X (반전)
	// Normalized.Y = 1.0 - NormalizedWorldX  =>  NormalizedWorldX = 1.0 - Normalized.Y
	float NormalizedWorldX = 1.0f - Normalized.Y;
	WorldPos2D.X = FMath::Lerp(WorldBounds.Min.X, WorldBounds.Max.X, NormalizedWorldX);

	return FVector(WorldPos2D.X, WorldPos2D.Y, 0.0f);
}

FVector UGS_MinimapWidget::MinimapLocalToWorld(const FGeometry& Geometry, const FVector2D& LocalPosition) const
{
	FVector2D MinimapSize = Geometry.GetLocalSize();

	// 로컬 좌표 -> 정규화 (0~1)
	FVector2D Normalized;
	Normalized.X = FMath::Clamp(LocalPosition.X / MinimapSize.X, 0.0f, 1.0f);
	Normalized.Y = FMath::Clamp(LocalPosition.Y / MinimapSize.Y, 0.0f, 1.0f);

	// 정규화 -> 월드 좌표 (MinimapScreenToWorld와 동일 로직)
	// UI X -> World Y
	FVector2D WorldPos2D;
	WorldPos2D.Y = FMath::Lerp(WorldBounds.Min.Y, WorldBounds.Max.Y, Normalized.X);

	// UI Y -> World X (반전)
	float NormalizedWorldX = 1.0f - Normalized.Y;
	WorldPos2D.X = FMath::Lerp(WorldBounds.Min.X, WorldBounds.Max.X, NormalizedWorldX);

	return FVector(WorldPos2D.X, WorldPos2D.Y, 0.0f);
}

// ========== Input Handling ==========

FReply UGS_MinimapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// === 우클릭 처리 (즉시 반응하여 인식률 향상) ===
	if (InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		HandleRightClick(InGeometry, LocalPos);
		return FReply::Handled(); // 이벤트 소비하여 다른 위젯이 처리하지 않도록
	}

	// === 좌클릭 처리 ===
	if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

		// 1. RTS 명령 모드인지 확인 (공격, 이동, 홀드 등)
		if (CachedRTSController)
		{
			ERTSCommand CurrentCommand = CachedRTSController->GetCurrentCommand();
			if (CurrentCommand != ERTSCommand::None)
			{
				// 명령 실행 (미니맵 좌표 -> 월드 좌표)
				FVector WorldLocation = MinimapLocalToWorld(InGeometry, LocalPos);

				switch (CurrentCommand)
				{
				case ERTSCommand::Attack:
					CachedRTSController->AttackAIViaMinimap(WorldLocation);
					return FReply::Handled(); // 명령 실행 후 클릭 소비

				case ERTSCommand::Move:
					CachedRTSController->MoveAIViaMinimap(WorldLocation);
					return FReply::Handled(); // 명령 실행 후 클릭 소비

				default:
					// Hold, Stop 등 타겟이 필요 없는 명령은 미니맵 클릭 시 명령 모드 해제하고
					// 카메라 이동/드래그 로직으로 넘어감
					CachedRTSController->OnEscapeButtonClicked();
					break;
				}
			}
		}

		// 2. 뷰 박스 내부 클릭 시 드래그 시작 (Offset 계산)
		if (IsPointInsideViewBox(LocalPos))
		{
			bIsDraggingViewBox = true;

			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CameraViewBox->Slot))
			{
				FVector2D ViewBoxCenter = CanvasSlot->GetPosition() + (CanvasSlot->GetSize() * 0.5f);
				DragOffset = ViewBoxCenter - LocalPos;
			}
			else
			{
				DragOffset = FVector2D::ZeroVector;
			}

			return FReply::Handled().CaptureMouse(TakeWidget());
		}
		else
		{
			// 3. 외부 클릭 -> 즉시 이동 및 드래그 시작 (Offset 0)
			bIsDraggingViewBox = true;
			DragOffset = FVector2D::ZeroVector;

			HandleDrag(InGeometry, LocalPos);
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}

	return FReply::Unhandled();
}

FReply UGS_MinimapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bIsDraggingViewBox)
	{
		bIsDraggingViewBox = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	// 우클릭 처리는 NativeOnMouseButtonDown으로 이동 (즉시 반응)
	// MouseButtonUp에서는 더 이상 우클릭 처리하지 않음

	return FReply::Unhandled();
}

FReply UGS_MinimapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDraggingViewBox)
	{
		FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		HandleDrag(InGeometry, LocalPos);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void UGS_MinimapWidget::HandleLeftClick(const FGeometry& Geometry, const FVector2D& LocalPosition)
{
	// 드래그 시작 판정은 NativeOnMouseButtonDown에서 처리
}

void UGS_MinimapWidget::HandleRightClick(const FGeometry& Geometry, const FVector2D& LocalPosition)
{
	if (!CachedRTSController) return;

	FVector WorldLocation = MinimapLocalToWorld(Geometry, LocalPosition);

	// 우클릭은 항상 이동 명령으로 처리 (Smart Move)
	// 공격 모드 등 다른 명령 상태여도 우클릭은 취소 후 이동이 일반적임
	CachedRTSController->MoveAIViaMinimap(WorldLocation);
}

void UGS_MinimapWidget::HandleDrag(const FGeometry& Geometry, const FVector2D& LocalPosition)
{
	if (!CachedRTSController) return;

	// Apply Offset to get the desired center position in Local Space
	FVector2D TargetLocalPos = LocalPosition + DragOffset;

	FVector WorldLocation = MinimapLocalToWorld(Geometry, TargetLocalPos);

	// 카메라 각도에 따른 오프셋 보정
	FVector2D Offset = GetCameraToGroundOffset();
	FVector CorrectedLocation = WorldLocation;
	CorrectedLocation.X -= Offset.X;
	CorrectedLocation.Y -= Offset.Y;

	CachedRTSController->MoveCameraViaMinimap(CorrectedLocation);

	if (CameraViewBox)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CameraViewBox->Slot))
		{
			FVector2D CurrentSize = CanvasSlot->GetSize();
			FVector2D NewPosition = TargetLocalPos - (CurrentSize * 0.5f);
			CanvasSlot->SetPosition(NewPosition);
		}
	}
}

FVector2D UGS_MinimapWidget::GetCameraToGroundOffset() const
{
	if (!CachedRTSCamera) return FVector2D::ZeroVector;

	// 뷰 영역의 중심 (지면 기준)
	FBox2D ViewBounds = CachedRTSCamera->GetSimpleViewBounds();
	FVector2D ViewCenter = ViewBounds.GetCenter();

	// 카메라 실제 위치 (X, Y)
	FVector CamLoc = CachedRTSCamera->GetActorLocation();
	FVector2D CamPos2D(CamLoc.X, CamLoc.Y);

	// 오프셋 = 뷰 중심 - 카메라 위치
	return ViewCenter - CamPos2D;
}

bool UGS_MinimapWidget::IsPointInsideViewBox(const FVector2D& LocalPosition) const
{
	if (!CameraViewBox || !bGeometryCached) return false;

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CameraViewBox->Slot);
	if (!CanvasSlot) return false;

	FVector2D ViewBoxPos = CanvasSlot->GetPosition();
	FVector2D ViewBoxSize = CanvasSlot->GetSize();

	// AABB 히트 테스트
	return (LocalPosition.X >= ViewBoxPos.X && LocalPosition.X <= ViewBoxPos.X + ViewBoxSize.X &&
		LocalPosition.Y >= ViewBoxPos.Y && LocalPosition.Y <= ViewBoxPos.Y + ViewBoxSize.Y);
}

// ========== View Box Update ==========

void UGS_MinimapWidget::RefreshViewBoxTransform()
{
	if (!CachedRTSCamera || !CameraViewBox || !bGeometryCached) return;

	// GS_RTSCamera의 캐싱된 뷰 경계 사용
	FBox2D ViewBounds = CachedRTSCamera->GetSimpleViewBounds();

	FVector2D ViewBoxPosition;
	FVector2D ViewBoxSize;
	CalculateViewBoxScreenRect(ViewBounds, ViewBoxPosition, ViewBoxSize);

	// Canvas Slot 업데이트
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CameraViewBox->Slot))
	{
		CanvasSlot->SetPosition(ViewBoxPosition);
		CanvasSlot->SetSize(ViewBoxSize);
	}

	// 색상 설정 (Material Instance Dynamic 사용)
	UMaterialInstanceDynamic* DynMat = CameraViewBox->GetDynamicMaterial();
	if (DynMat)
	{
		DynMat->SetVectorParameterValue(FName("Color"), ViewBoxColor);
	}
}

void UGS_MinimapWidget::CalculateViewBoxScreenRect(const FBox2D& ViewBounds, FVector2D& OutPosition, FVector2D& OutSize) const
{
	// ViewBounds (월드 좌표) -> 미니맵 정규화
	FVector2D MinNormalized = WorldToMinimapNormalized(FVector(ViewBounds.Min.X, ViewBounds.Min.Y, 0.0f));
	FVector2D MaxNormalized = WorldToMinimapNormalized(FVector(ViewBounds.Max.X, ViewBounds.Max.Y, 0.0f));

	// 정규화 -> 스크린 픽셀
	FVector2D MinimapSize = CachedMinimapGeometry.GetLocalSize();

	FVector2D MinScreen = MinNormalized * MinimapSize;
	FVector2D MaxScreen = MaxNormalized * MinimapSize;

	// MinScreen is (Left, Bottom) because Y is inverted
	// MaxScreen is (Right, Top)

	OutPosition.X = MinScreen.X;
	OutPosition.Y = MaxScreen.Y; // Top Y is smaller

	OutSize.X = MaxScreen.X - MinScreen.X;
	OutSize.Y = MinScreen.Y - MaxScreen.Y; // Bottom Y - Top Y
}

// ========== Attack Warning System ==========

void UGS_MinimapWidget::ShowAttackWarning(const FVector& WorldLocation)
{
	if (!bGeometryCached)
	{
		return;
	}

	// Limit max warnings
	if (ActiveAttackWarnings.Num() >= 5)
	{
		// Remove oldest
		if (ActiveAttackWarnings[0].IconWidget)
		{
			ReleaseIconWidget(ActiveAttackWarnings[0].IconWidget);
		}
		ActiveAttackWarnings.RemoveAt(0);
	}

	// Acquire icon from pool
	UImage* WarningIcon = AcquireIconWidget();
	if (!WarningIcon)
	{
		return;
	}

	// Set texture
	if (AttackWarningIconTexture)
	{
		WarningIcon->SetBrushFromTexture(AttackWarningIconTexture);
	}

	// Position on minimap
	FVector2D NormalizedPos = WorldToMinimapNormalized(WorldLocation);
	FVector2D ScreenPos = NormalizedPos * CachedMinimapGeometry.GetLocalSize();

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(WarningIcon->Slot))
	{
		CanvasSlot->SetPosition(ScreenPos - (AttackWarningIconSize * 0.5f));
		CanvasSlot->SetSize(AttackWarningIconSize);
		CanvasSlot->SetZOrder(20); // Above unit icons
	}

	WarningIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	WarningIcon->SetRenderOpacity(1.0f);

	// Add to active warnings (2.5s duration)
	FGS_MinimapAttackWarning Warning;
	Warning.IconWidget = WarningIcon;
	Warning.WorldLocation = WorldLocation;
	Warning.ExpirationTime = GetWorld()->GetTimeSeconds() + 2.5f;
	ActiveAttackWarnings.Add(Warning);
}

void UGS_MinimapWidget::UpdateAttackWarnings(float DeltaTime)
{
	if (ActiveAttackWarnings.IsEmpty())
	{
		return;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();

	for (int32 i = ActiveAttackWarnings.Num() - 1; i >= 0; --i)
	{
		FGS_MinimapAttackWarning& Warning = ActiveAttackWarnings[i];

		if (!Warning.IconWidget)
		{
			ActiveAttackWarnings.RemoveAt(i);
			continue;
		}

		// Fade out in last 0.5 seconds
		float TimeRemaining = Warning.ExpirationTime - CurrentTime;
		if (TimeRemaining < 0.5f && TimeRemaining > 0.0f)
		{
			float Opacity = TimeRemaining / 0.5f;
			Warning.IconWidget->SetRenderOpacity(Opacity);
		}

		// Remove expired
		if (CurrentTime >= Warning.ExpirationTime)
		{
			ReleaseIconWidget(Warning.IconWidget);
			ActiveAttackWarnings.RemoveAt(i);
		}
	}
}
