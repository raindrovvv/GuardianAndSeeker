// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/GS_BasePlayerController.h"
#include "RTSCommand.h"
#include "AkGameplayStatics.h"
#include "ResourceSystem/Aether/GS_AetherComp.h"
#include "GS_RTSController.generated.h"

struct FInputActionInstance;
struct FInputActionValue;
class AGS_Monster;
class AGS_Character;
class AGS_Seeker;
class UInputMappingContext;
class UInputAction;
class UGS_AetherComp;
class UGS_RTSSkillComponent;

// 지정된 부대 
USTRUCT(BlueprintType)
struct FUnitGroup
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<AGS_Monster*> Units;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectionChanged, const TArray<AGS_Monster*>&, NewSelection);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSeekerSelectionChanged, AGS_Seeker*, NewSeeker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRTSCommandChanged, ERTSCommand, NewCommand);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectedUnitsSkillChanged, bool, bAnyUnitHasSkill);
//[Aether]
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAetherCompReady, UGS_AetherComp*, AetherComp);

UCLASS()
class GAS_API AGS_RTSController : public AGS_BasePlayerController
{
	GENERATED_BODY()

public:
	AGS_RTSController();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	UInputMappingContext* InputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	UInputAction* CameraMoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	UInputAction* AttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	UInputAction* StopAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	UInputAction* HoldAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	UInputAction* SkillAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	UInputAction* LeftClickAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	UInputAction* RightClickAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	UInputAction* CtrlAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	UInputAction* ShiftAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	UInputAction* DoubleClickAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TArray<UInputAction*> GroupKeyActions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TArray<UInputAction*> CameraKeyActions;

	// Guardian RTS 스킬 입력 (1-4 키)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TArray<UInputAction*> RTSSkillKeyActions;

	// 선택 변경 델리게이트
	UPROPERTY(BlueprintAssignable, Category="Selection")
	FOnSelectionChanged OnSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category="Command")
	FOnRTSCommandChanged OnRTSCommandChanged;

	UPROPERTY(BlueprintAssignable, Category = "Selection")
	FOnSelectedUnitsSkillChanged OnSelectedUnitsSkillChanged;

	UPROPERTY(BlueprintAssignable)
	FOnSeekerSelectionChanged OnSeekerSelectionChanged;
	
	//[Aether]AetherComp + 생성 완료 알리는 델리게이트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource")
	TObjectPtr<UGS_AetherComp> AetherComp;

	UPROPERTY(BlueprintAssignable, Category="Resource")
	FOnAetherCompReady OnAetherCompReady;

	// Guardian RTS 스킬 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RTS|Skill")
	TObjectPtr<UGS_RTSSkillComponent> RTSSkillComp;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual AActor* GetViewTarget() const override;

	// 현재 선택된 유닛들
	UFUNCTION(BlueprintCallable)
	const TArray<AGS_Monster*>& GetUnitSelection() const { return UnitSelection; }

	UFUNCTION(BlueprintCallable)
	bool IsCtrlDown() const { return bCtrlDown; }
	
	UFUNCTION(BlueprintCallable)
	bool IsShiftDown() const { return bShiftDown; }
	
	// 카메라 이동 입력 처리
	void CameraMove(const FInputActionValue& InputValue);
	void CameraMoveEnd();

	// 명령 모드 전환
	void OnCommandMove(const FInputActionValue& Value);
	void OnCommandAttack(const FInputActionValue& Value);
	void OnCommandStop(const FInputActionValue& Value);
	void OnCommandHold(const FInputActionValue& Value);
	void OnCommandSkill(const FInputActionValue& Value);

	// 실제 구현 + HUD 버튼 클릭시
	UFUNCTION(BlueprintCallable, Category="RTS")
	void MoveSelectedUnits();
	
	UFUNCTION(BlueprintCallable, Category="RTS")
	void AttackSelectedUnits();
	
	UFUNCTION(BlueprintCallable, Category="RTS")
	void StopSelectedUnits();
	
	UFUNCTION(BlueprintCallable, Category="RTS")
	void HoldSelectedUnits();

	UFUNCTION(BlueprintCallable, Category="RTS")
	void SkillSelectedUnits();

	// Guardian RTS 스킬 발동 (1-4 키)
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void ActivateGuardianSkill(int32 SkillIndex);

	// 타겟팅 모드 관련
	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	bool IsInGuardianSkillTargetingMode() const;

	UFUNCTION(BlueprintCallable, Category="RTS|Skill")
	void CancelGuardianSkillTargeting();

	// 마우스 클릭 처리
	void OnLeftMousePressed();
	void OnLeftMouseReleased();
	void OnRightMousePressed(const FInputActionValue& InputValue);

	// 유닛 선택
	UFUNCTION(BlueprintCallable)
	void AddUnitToSelection(AGS_Monster* Unit);
	
	void AddMultipleUnitsToSelection(const TArray<AGS_Monster*>& Units); // 다중 선택

	UFUNCTION(BlueprintCallable)
	void SelectSameTypeFromSelection(AGS_Monster* Unit);
	
	UFUNCTION(BlueprintCallable)
	void RemoveUnitFromSelection(AGS_Monster* Unit);

	UFUNCTION(BlueprintCallable)
	void ClearUnitSelection();

	// 부대 지정, 호출 
	void OnCtrlPressed(const FInputActionInstance& InputInstance);
	void OnCtrlReleased(const FInputActionInstance& InputInstance);
	void OnShiftPressed(const FInputActionInstance& InputInstance);
	void OnShiftReleased(const FInputActionInstance& InputInstance);
	void OnGroupKey(const FInputActionInstance& InputInstance, int32 GroupIdx);

	// 미니맵
	void OnCameraKey(const FInputActionInstance& InputInstance, int32 CameraIndex);
	
	UFUNCTION(BlueprintCallable)
	void MoveAIViaMinimap(const FVector& WorldLocation);
	
	UFUNCTION(BlueprintCallable)
	void AttackAIViaMinimap(const FVector& WorldLocation);
	
	UFUNCTION(BlueprintCallable)
	void MoveCameraViaMinimap(const FVector& WorldLocation);

	UFUNCTION(BlueprintCallable)
	ERTSCommand GetCurrentCommand() const { return CurrentCommand; }
	
	// 선택 동기화 RPC (클라이언트 → 서버)
	UFUNCTION(Server, Reliable)
	void Server_AddUnitToSelection(AGS_Monster* Unit);

	UFUNCTION(Server, Reliable)
	void Server_RemoveUnitFromSelection(AGS_Monster* Unit);

	UFUNCTION(Server, Reliable)
	void Server_ClearUnitSelection();

	UFUNCTION(Server, Reliable)
	void Server_SetMultipleUnitsSelection(const TArray<AGS_Monster*>& Units);

	// Server - 유닛 배열을 RPC로 전달하지 않고 서버에서 UnitSelection 직접 참조
	UFUNCTION(Server, Reliable)
	void Server_RTSMove(const FVector& Dest);

	UFUNCTION(Server, Reliable)
	void Server_RTSAttackMove(const FVector& Dest);

	UFUNCTION(Server, Reliable)
	void Server_RTSAttack(AGS_Character* TargetActor);

	UFUNCTION(Server, Reliable)
	void Server_RTSStop();

	UFUNCTION(Server, Reliable)
	void Server_RTSHold();

	UFUNCTION(Server, Reliable)
	void Server_RTSSkill();

	// Client
	// UFUNCTION(Client, Reliable)
	// void Client_HideDungeonElements();

	// BGM Control
	UFUNCTION(Client, Reliable)
	void Client_PlayBossBGM(UAkAudioEvent* StartEvent, UAkAudioEvent* StopEvent);

	UFUNCTION(Client, Reliable)
	void Client_StopBossBGM();

	UFUNCTION()
	void HideDungeonElements();
	
	// UI 버튼 클릭 함수
	UFUNCTION(BlueprintCallable, Category="RTS")
	void OnEscapeButtonClicked();

	bool HasAnySelectedUnitSkill() const;
	
	UFUNCTION(BlueprintCallable, Category="Cursor")
	void SetRTSCursor(const FName& CursorPath);

	UFUNCTION()
	void HandleSeekerHover(bool bIsHover);

	//[Aether]AetherComp 추가 
	UGS_AetherComp* GetAetherComp() const;

	//마우스 설정 함수
	void ApplyRTSInputMode();

	virtual void Client_StartGame_Implementation() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaTime) override;

private:
	// 입력 상태
	ERTSCommand CurrentCommand;

	// 카메라 
	FVector2D KeyboardDir;
	FVector2D MouseEdgeDir;

	UPROPERTY()
	class AGS_RTSCamera* CameraActor;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraSpeed;
	
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EdgeScreenRatio;

	UPROPERTY(Replicated)
	TArray<AGS_Monster*> UnitSelection; // 현재 선택된 유닛 (서버 동기화)

	UPROPERTY()
	AGS_Seeker* SelectedSeeker; 

	UPROPERTY()
	TArray<FUnitGroup> UnitGroups; // 지정된 부대

	bool bCtrlDown;
	bool bShiftDown;
	int32 MaxSelectableUnits;
	
	UPROPERTY()
	TMap<int32, FVector> SavedCameraPositions; // 카메라 저장 위치

	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UUserWidget> RTSWidgetClass;

	// RTS 스킬 바 위젯 클래스
	UPROPERTY(EditDefaultsOnly, Category="UI|RTS Skill")
	TSubclassOf<class UGS_RTSSkillBarWidget> RTSSkillBarWidgetClass;

	// 생성된 스킬 바 위젯 인스턴스
	UPROPERTY()
	UGS_RTSSkillBarWidget* SkillBarWidget;

	bool bSeekerHovered;
	bool bShowAttackCursor;
	bool bCursorReady; // 커서 시스템 사용 가능 여부
	int32 CursorInitRetryCount; // 재시도 횟수 (최대 10)
	FName CurrentCursorPath; // 현재 설정된 커서 (캐싱용)
	FTimerHandle CursorInitTimerHandle; // 커서 초기화 타이머
	FName DefaultCursorPath;
	FName CommandCursorPath;
	FName AttackCommandCursorPath;
	FName SeekerAttackCursorPath;
	FName ScrollUpCursorPath;
	FName ScrollDownCursorPath;
	FName ScrollLeftCursorPath;
	FName ScrollRightCursorPath;
	FName SkillTargetingCursorPath;

	UPROPERTY()
	FTimerHandle AttackCursorTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> MouseClickSound;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> CommandButtonSound;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> CommandMoveSound;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> CommandAttackSound;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> CommandCancelSound;

	FTimerHandle DetectionTimerHandle;

	// 그룹 더블 클릭 감지
	int32 LastPressedGroupIdx;
	float LastGroupKeyPressTime;
	FTimerHandle GroupDoubleClickTimerHandle;

	UPROPERTY(EditAnywhere, Category = "Input")
	float DoubleClickTimeThreshold; // 더블 클릭 인식 시간 (기본 0.3초)

	// 감지된 시커들 추적
	UPROPERTY()
	TArray<AGS_Seeker*> DetectedSeekers;

	// 감지 업데이트 주기 (초)
	UPROPERTY(EditAnywhere, Category = "Detection")
	float DetectionUpdateInterval = 0.1f;

	// RPC 쿨다운 시간 (초)
	UPROPERTY(EditAnywhere, Category = "Detection")
	float DetectionRPCCooldown = 0.5f;

	// RPC 쿨다운 추적용 맵
	UPROPERTY()
	TMap<AGS_Seeker*, float> LastSeekerNotifyTimes;

	FVector2D GetKeyboardDirection() const;
	FVector2D GetMouseEdgeDirection() const;
	FVector2D GetFinalDirection() const;
	void MoveCamera(const FVector2D& Direction, float DeltaTime);
	void InitCameraActor();
	
	void SelectOnCtrlClick();
	void ToggleOnShiftClick();
	
	// 명령 가능한 유닛들
	void GatherCommandableUnits(TArray<AGS_Monster*>& Out) const;
	bool IsSelectable(AGS_Monster* Monster) const;

	UFUNCTION()
	void OnSelectedUnitDead(AGS_Monster* Monster);

	void UpdateCursorForCommand();
	void UpdateCursorForEdgeScroll();
	void ShowAttackCursor();

	// 커서 초기화
	UFUNCTION()
	void InitializeCursor(); // 타이머 콜백
	void TryInitializeCursorInTick(); // Tick 재시도

	// 시커 감지 시스템
	void UpdateSeekerDetection();
	bool IsSeekerInCameraView(AGS_Seeker* Seeker);
	void NotifySeekerDetection(AGS_Seeker* Seeker, bool bIsDetected);

	// 시커 파괴 시 정리
	UFUNCTION()
	void OnTrackedSeekerDestroyed(AActor* DestroyedActor);

	// 서버로 감지 상태를 알리는 RPC
	UFUNCTION(Server, Reliable)
	void Server_NotifySeekerDetection(AGS_Seeker* Seeker, bool bIsDetected);

	// 화면 중앙과의 거리 계산 (0.0 = 중앙, 1.0 = 가장자리)
	float CalculateSeekerDistanceFromScreenCenter(AGS_Seeker* Seeker);

	// 그룹 더블 클릭 관련
	FVector CalculateGroupCenterLocation(int32 GroupIdx) const;
	void MoveCameraToGroupCenter(int32 GroupIdx);

	UFUNCTION()
	void ResetGroupDoubleClickState();

	// Guardian RTS 스킬 입력 핸들러
	void OnRTSSkillKey(const FInputActionInstance& InputInstance, int32 SkillIndex);

	// 타겟팅 모드 처리
	void HandleSkillTargetingClick(const FVector& TargetLocation);

	// ==========================================
	// RTS 명령 데칼 시스템
	// ==========================================

	/** 명령 타입별 데칼 머티리얼 맵 */
	UPROPERTY(EditDefaultsOnly, Category = "RTS|CommandDecal")
	TMap<ERTSCommand, UMaterialInterface*> CommandDecalMaterials;

	/** 명령 데칼 크기 */
	UPROPERTY(EditAnywhere, Category = "RTS|CommandDecal", meta = (ClampMin = "10.0", ClampMax = "200.0"))
	FVector CommandDecalSize = FVector(60.0f, 60.0f, 60.0f);

	/** 명령 데칼 수명 (초) */
	UPROPERTY(EditAnywhere, Category = "RTS|CommandDecal", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float CommandDecalLifeSpan = 0.5f;

	/** 명령 데칼 Z축 오프셋 (Z파이팅 방지) */
	UPROPERTY(EditAnywhere, Category = "RTS|CommandDecal", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float CommandDecalZOffset = 1.0f;

	/**
	 * RTS 명령 위치에 데칼을 스폰 (로컬 전용)
	 * @param CommandType 명령 타입
	 * @param Location 월드 위치
	 */
	void SpawnCommandDecal(ERTSCommand CommandType, const FVector& Location);

	// 시커 근접도 업데이트 (서버 RPC)
	UFUNCTION(Server, Unreliable)
	void Server_UpdateSeekerProximity(AGS_Seeker* Seeker, float DistanceFromCenter);
};