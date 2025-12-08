// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AI/RTS/RTSCommand.h"
#include "GS_MinimapWidget.generated.h"

class UImage;
class UCanvasPanel;
class UOverlay;
class UCanvasPanelSlot;
class AGS_RTSController;
class AGS_RTSCamera;
class AGS_Monster;
class UTexture2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 미니맵 아이콘 데이터 구조체
 */
USTRUCT()
struct FGS_MinimapIconData
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AGS_Monster> TrackedUnit;

	UPROPERTY()
	TObjectPtr<UImage> IconWidget;

	FVector2D LastScreenPosition;

	FGS_MinimapIconData()
		: LastScreenPosition(FVector2D::ZeroVector)
	{}
};

UCLASS()
class GAS_API UGS_MinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGS_MinimapWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** 미니맵 렌더 타겟 이미지 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> MinimapImage;

	/** 아이콘 및 뷰박스를 배치할 오버레이 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> MinimapOverlay;

	/** 카메라 뷰 박스 이미지 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> CameraViewBox;

	/** 아이콘 컨테이너 (동적 생성된 아이콘들) */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> IconContainer;

	// ========== Editable Properties ==========

	/** 아이콘 업데이트 주기 (초) - 성능 조절 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Performance")
	float IconUpdateInterval = 0.05f; // 20 FPS

	/** 카메라 뷰 박스 업데이트 주기 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Performance")
	float ViewBoxUpdateInterval = 0.033f; // 30 FPS

	/** 유닛 아이콘 텍스처 맵 (CharacterType별) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Icons")
	TMap<FName, UTexture2D*> UnitIconTextures;

	/** 적 아이콘 텍스처 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Icons")
	TObjectPtr<UTexture2D> EnemyIconTexture;

	/** 아이콘 기본 크기 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Icons")
	FVector2D IconSize = FVector2D(16.0f, 16.0f);

	/** 미니맵 월드 경계 (X, Y 최소/최대) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|World")
	FBox2D WorldBounds = FBox2D(FVector2D(-5000.0f, -5000.0f), FVector2D(5000.0f, 5000.0f));

	/** 카메라 뷰 박스 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|ViewBox")
	FLinearColor ViewBoxColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);

	/** 아이콘 풀 최대 크기 (성능 최적화) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Performance")
	int32 MaxIconPoolSize = 50;

private:
	// ========== Cached References ==========

	UPROPERTY()
	TObjectPtr<AGS_RTSController> CachedRTSController;

	UPROPERTY()
	TObjectPtr<AGS_RTSCamera> CachedRTSCamera;

	// ========== Timer Handles ==========

	FTimerHandle TimerHandle_IconUpdate;
	FTimerHandle TimerHandle_ViewBoxUpdate;

	// ========== Icon Pool Management ==========

	/** 활성 아이콘 데이터 (유닛별) */
	UPROPERTY()
	TMap<TObjectPtr<AGS_Monster>, FGS_MinimapIconData> ActiveIcons;

	/** 재사용 가능한 아이콘 위젯 풀 */
	UPROPERTY()
	TArray<TObjectPtr<UImage>> InactiveIconPool;

	// ========== Drag State ==========

	bool bIsDraggingViewBox = false;
	FVector2D DragStartPosition;
	FVector2D DragOffset;

	// ========== Geometry Cache ==========

	FGeometry CachedMinimapGeometry;
	bool bGeometryCached = false;

	// ========== Core Functions ==========

	/** 컨트롤러 및 카메라 참조 초기화 */
	void InitializeReferences();

	/** 델리게이트 바인딩 */
	void BindDelegates();

	/** 델리게이트 언바인딩 */
	void UnbindDelegates();

	/** 타이머 시작 */
	void StartTimers();

	/** 타이머 정리 */
	void ClearTimers();

	// ========== Delegate Callbacks ==========

	UFUNCTION()
	void OnSelectionChanged(const TArray<AGS_Monster*>& NewSelection);

	UFUNCTION()
	void OnRTSCommandChanged(ERTSCommand NewCommand);

	// ========== Timer Callbacks ==========

	UFUNCTION()
	void UpdateUnitIcons();

	UFUNCTION()
	void UpdateCameraViewBox();

	// ========== Icon Management ==========

	/** 아이콘 풀에서 위젯 가져오기 (또는 생성) */
	UImage* AcquireIconWidget();

	/** 아이콘 풀로 반환 */
	void ReleaseIconWidget(UImage* IconWidget);

	/** 유닛 아이콘 생성/업데이트 */
	void UpdateIconForUnit(AGS_Monster* Unit);

	/** 유닛 아이콘 제거 */
	void RemoveIconForUnit(AGS_Monster* Unit);

	/** 모든 아이콘 제거 */
	void ClearAllIcons();

	// ========== Coordinate Conversion ==========

	/** 월드 좌표 -> 미니맵 스크린 좌표 (정규화 0~1) */
	FVector2D WorldToMinimapNormalized(const FVector& WorldLocation) const;

	/** 미니맵 스크린 좌표 (픽셀) -> 월드 좌표 */
	FVector MinimapScreenToWorld(const FVector2D& ScreenPosition) const;

	/** 미니맵 로컬 좌표 -> 월드 좌표 (마우스 입력용) */
	FVector MinimapLocalToWorld(const FGeometry& Geometry, const FVector2D& LocalPosition) const;

	/** 카메라 위치와 실제 뷰 중심(지면) 간의 오프셋 계산 */
	FVector2D GetCameraToGroundOffset() const;

	// ========== Input Handling ==========

	/** 왼쪽 클릭 처리 (드래그 시작 판정) */
	void HandleLeftClick(const FGeometry& Geometry, const FVector2D& LocalPosition);

	/** 오른쪽 클릭 처리 (명령 전달) */
	void HandleRightClick(const FGeometry& Geometry, const FVector2D& LocalPosition);

	/** 드래그 처리 (카메라 이동) */
	void HandleDrag(const FGeometry& Geometry, const FVector2D& LocalPosition);

	/** 뷰 박스 히트 테스트 */
	bool IsPointInsideViewBox(const FVector2D& LocalPosition) const;

	// ========== View Box Update ==========

	/** 카메라 뷰 박스 위치/크기 업데이트 */
	void RefreshViewBoxTransform();

	/** FBox2D -> 미니맵 스크린 좌표 변환 */
	void CalculateViewBoxScreenRect(const FBox2D& ViewBounds, FVector2D& OutPosition, FVector2D& OutSize) const;
};
