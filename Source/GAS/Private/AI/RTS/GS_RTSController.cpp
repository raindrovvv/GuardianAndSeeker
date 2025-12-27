// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/RTS/GS_RTSController.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "AI/GS_AIController.h"
#include "AI/RTS/GS_RTSCamera.h"
#include "AI/RTS/GS_RTSHUD.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/UserWidget.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Player/Monster/GS_Monster.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Skill/Monster/GS_MonsterSkillBase.h"
#include "Character/Skill/Monster/GS_MonsterSkillComp.h"
#include "UI/Character/GS_HPTextWidgetComp.h"
#include "Sound/GS_AudioManager.h"
#include "System/GameState/GS_InGameGS.h"
#include "ResourceSystem/Aether/GS_AetherExtractor.h"
#include "System/GameMode/GS_InGameGM.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillComponent.h"
#include "UI/RTS/GS_RTSSkillBarWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "AI/RTS/GS_RTSAttackNotificationManager.h"
#include "UI/RTS/GS_RTSAttackWarningWidget.h"
#include "UI/RTS/GS_MinimapWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"



AGS_RTSController::AGS_RTSController()
{
	bShowMouseCursor = true;

	CurrentCommand = ERTSCommand::None;
	KeyboardDir = FVector2D::ZeroVector;
	MouseEdgeDir = FVector2D::ZeroVector;
	CameraActor = nullptr;
	CameraSpeed = 2000.f;
	EdgeScreenRatio = 0.01f;
	LastMousePosition = FVector2D(-1.f, -1.f);
	LastViewportSize = FIntPoint(0, 0);
	UnitGroups.SetNum(9);
	bCtrlDown = false;
	bShiftDown = false;
	MaxSelectableUnits = 12;
	bShowAttackCursor = false;
	bSeekerHovered = false;
	bCursorReady = false;
	CursorInitRetryCount = 0;
	CurrentCursorPath = NAME_None;

	// 그룹 더블 클릭 초기화
	LastPressedGroupIdx = -1;
	LastGroupKeyPressTime = 0.f;
	DoubleClickTimeThreshold = 0.3f;

	DefaultCursorPath = FName(TEXT("UI/RTS/Cursor/Icon_Cursor_RTSDefault"));
	CommandCursorPath = FName(TEXT("UI/RTS/Cursor/Icon_Cursor_ETC"));
	AttackCommandCursorPath = FName(TEXT("UI/RTS/Cursor/Icon_Cursor_AttackCommand"));
	SeekerAttackCursorPath = FName(TEXT("UI/RTS/Cursor/Icon_Cursor_SeekerAttack"));
	ScrollUpCursorPath = FName(TEXT("UI/RTS/Cursor/Icon_Cursor_Scroll_U"));
	ScrollDownCursorPath = FName(TEXT("UI/RTS/Cursor/Icon_Cursor_Scroll_D"));
	ScrollLeftCursorPath = FName(TEXT("UI/RTS/Cursor/Icon_Cursor_Scroll_L"));
	ScrollRightCursorPath = FName(TEXT("UI/RTS/Cursor/Icon_Cursor_Scroll_R"));

	//[RTS Skill] 스킬 타겟팅 커서 경로
	SkillTargetingCursorPath = FName(TEXT("UI/RTS/Cursor/Icon_Cursor_SkillTarget"));

	//[Aether] AetherComp 연결
	AetherComp = CreateDefaultSubobject<UGS_AetherComp>(TEXT("AetherComp"));

	//[RTS Skill] RTSSkillComp 생성
	RTSSkillComp = CreateDefaultSubobject<UGS_RTSSkillComponent>(TEXT("RTSSkillComp"));

	// Create AttackNotificationManager component
	AttackNotificationManager = CreateDefaultSubobject<UGS_RTSAttackNotificationManager>(TEXT("AttackNotificationManager"));

	// Sound
	static ConstructorHelpers::FObjectFinder<USoundBase> MouseClickSoundRef(TEXT("/Game/WwiseAudio/Audio/UI/UI_SFX_ClickSound.UI_SFX_ClickSound"));
	if (MouseClickSoundRef.Succeeded())
	{
		MouseClickSound = MouseClickSoundRef.Object;
		CommandButtonSound = MouseClickSoundRef.Object; // 기본값으로 동일한 사운드 사용
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> CommandMoveSoundRef(TEXT("/Game/WwiseAudio/Audio/UI/UI_SFX_MoveSound.UI_SFX_MoveSound"));
	if (CommandMoveSoundRef.Succeeded())
	{
		CommandMoveSound = CommandMoveSoundRef.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> CommandAttackSoundRef(TEXT("/Game/WwiseAudio/Audio/UI/UI_SFX_AttackSound.UI_SFX_AttackSound"));
	if (CommandAttackSoundRef.Succeeded())
	{
		CommandAttackSound = CommandAttackSoundRef.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> CommandCancelSoundRef(TEXT("/Game/WwiseAudio/Audio/UI/UI_SFX_CancelSound.UI_SFX_CancelSound"));
	if (CommandCancelSoundRef.Succeeded())
	{
		CommandCancelSound = CommandCancelSoundRef.Object;
	}
}

void AGS_RTSController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// UnitSelection을 클라이언트에서 서버로 리플리케이션
	DOREPLIFETIME(AGS_RTSController, UnitSelection);
}

AActor* AGS_RTSController::GetViewTarget() const
{
	return Super::GetViewTarget();
}

void AGS_RTSController::BeginPlay()
{
	Super::BeginPlay();
	
	// UI 및 로컬 설정 (Client 및 Listen Server Host 모두 실행)
	if (IsLocalController())
	{
		// 타이머로 커서 및 입력 모드 초기화 (150ms 지연)
		// 뷰포트 초기화 이슈 방지를 위해 지연 실행
		GetWorldTimerManager().SetTimer(
			CursorInitTimerHandle,
			this,
			&AGS_RTSController::InitializeCursor,
			0.15f,
			false
		);

		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (InputMappingContext)
				{
					Subsystem->AddMappingContext(InputMappingContext, 0);
				}
			}
		}

		InitCameraActor();

		if (RTSWidgetClass)
		{
			UUserWidget* RTSWidget = CreateWidget<UUserWidget>(this, RTSWidgetClass);
			if (RTSWidget)
			{
				RTSWidget->AddToViewport();
			}
		}

		// RTS 스킬 바 위젯 생성
		if (RTSSkillComp)
		{
			if (!RTSSkillBarWidgetClass)
			{
				UE_LOG(LogTemp, Error, TEXT("RTSSkillBarWidgetClass is not set! Please set WBP_RTSSkillBarWidget in BP_RTSController"));
				// return; // 중단하지 않고 계속 진행
			}
			else
			{
				if (!SkillBarWidget)
				{
					SkillBarWidget = CreateWidget<UGS_RTSSkillBarWidget>(this, RTSSkillBarWidgetClass);
				}
				
				if (SkillBarWidget)
				{
					if (!SkillBarWidget->IsInViewport())
					{
						SkillBarWidget->AddToViewport(10); // Z-Order 10 (HUD 위에 표시)
					}
					else
					{
						SkillBarWidget->SetVisibility(ESlateVisibility::Visible);
					}
					
					// 초기화
					SkillBarWidget->InitializeSkillBar(RTSSkillComp);

					UE_LOG(LogTemp, Log, TEXT("RTSSkillBarWidget created and initialized successfully"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to create RTSSkillBarWidget"));
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("RTSSkillComp is null, cannot create skill bar"));
		}

		// Create attack warning widget
		if (AttackWarningWidgetClass && AttackNotificationManager)
		{
			UGS_RTSAttackWarningWidget* WarningWidget = AttackNotificationManager->GetWarningWidget();
			if (!WarningWidget)
			{
				WarningWidget = CreateWidget<UGS_RTSAttackWarningWidget>(this, AttackWarningWidgetClass);
			}

			if (WarningWidget)
			{
				if (!WarningWidget->IsInViewport())
				{
					WarningWidget->AddToViewport(15); // Higher Z-order than skill bar (which is 10)
				}
				else
				{
					WarningWidget->SetVisibility(ESlateVisibility::Visible);
				}
				
				AttackNotificationManager->SetWarningWidget(WarningWidget);

				UE_LOG(LogTemp, Log, TEXT("RTSAttackWarningWidget created successfully"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to create RTSAttackWarningWidget"));
			}
		}
		else if (!AttackWarningWidgetClass)
		{
			UE_LOG(LogTemp, Error, TEXT("AttackWarningWidgetClass is NOT set in BP_RTSController! Attack warning will not appear."));
		}

		// Auto-connect minimap widget to AttackNotificationManager
		if (AttackNotificationManager)
		{
			// Find minimap widget in viewport
			TArray<UUserWidget*> FoundWidgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UGS_MinimapWidget::StaticClass(), false);

			if (FoundWidgets.Num() > 0)
			{
				UGS_MinimapWidget* MinimapWidget = Cast<UGS_MinimapWidget>(FoundWidgets[0]);
				if (MinimapWidget)
				{
					AttackNotificationManager->SetMinimapWidget(MinimapWidget);
					UE_LOG(LogTemp, Log, TEXT("Minimap auto-connected to AttackNotificationManager"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("MinimapWidget not found - attack warnings will not show on minimap"));
			}
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->RegisterRTSController(this);
		}
	}

	//[Aether] 준비 완료 시 broadcast
	if (IsValid(AetherComp))
	{
		OnAetherCompReady.Broadcast(AetherComp);
		
	}
	for (TActorIterator<AGS_AetherExtractor> It(GetWorld()); It; ++It)
	{
		It->RegisterRTSController(this);

	}

	UE_LOG(LogTemp, Warning, TEXT("[RTSController::BeginPlay] Authority=%d, AetherComp=%s"),
		HasAuthority(),
		*GetNameSafe(AetherComp));
	if (HasAuthority() && IsValid(AetherComp))
	{
		AetherComp->InitializeMaxAmount(AetherComp->GetMaxAmount());
	}
	
	// 시커 감지 타이머 시작
	if (IsLocalController())
	{
		GetWorldTimerManager().SetTimer(
			DetectionTimerHandle,
			this,
			&AGS_RTSController::UpdateSeekerDetection,
			DetectionUpdateInterval,
			true
		);
	}
}

void AGS_RTSController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 타이머 정리
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(AttackCursorTimerHandle);
		GetWorldTimerManager().ClearTimer(DetectionTimerHandle);
		GetWorldTimerManager().ClearTimer(CursorInitTimerHandle);
		GetWorldTimerManager().ClearTimer(GroupDoubleClickTimerHandle);
	}

	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->UnregisterRTSController(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AGS_RTSController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent);
	{
		EnhancedInputComponent->BindAction(CameraMoveAction, ETriggerEvent::Triggered, this, &AGS_RTSController::CameraMove);
		EnhancedInputComponent->BindAction(CameraMoveAction, ETriggerEvent::Completed, this, &AGS_RTSController::CameraMoveEnd);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Started, this, &AGS_RTSController::OnCommandMove);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AGS_RTSController::OnCommandAttack);
		EnhancedInputComponent->BindAction(StopAction, ETriggerEvent::Started, this, &AGS_RTSController::OnCommandStop);
		EnhancedInputComponent->BindAction(HoldAction, ETriggerEvent::Started, this, &AGS_RTSController::OnCommandHold);
		EnhancedInputComponent->BindAction(SkillAction, ETriggerEvent::Started, this, &AGS_RTSController::OnCommandSkill);
		
		EnhancedInputComponent->BindAction(LeftClickAction, ETriggerEvent::Started, this, &AGS_RTSController::OnLeftMousePressed);
		EnhancedInputComponent->BindAction(LeftClickAction, ETriggerEvent::Completed, this, &AGS_RTSController::OnLeftMouseReleased);
		EnhancedInputComponent->BindAction(RightClickAction, ETriggerEvent::Started, this, &AGS_RTSController::OnRightMousePressed);
		
		EnhancedInputComponent->BindAction(CtrlAction, ETriggerEvent::Started,   this, &AGS_RTSController::OnCtrlPressed);
		EnhancedInputComponent->BindAction(CtrlAction, ETriggerEvent::Completed, this, &AGS_RTSController::OnCtrlReleased);
		EnhancedInputComponent->BindAction(ShiftAction, ETriggerEvent::Started,   this, &AGS_RTSController::OnShiftPressed);
		EnhancedInputComponent->BindAction(ShiftAction, ETriggerEvent::Completed, this, &AGS_RTSController::OnShiftReleased);
		EnhancedInputComponent->BindAction(DoubleClickAction, ETriggerEvent::Completed,   this, &AGS_RTSController::SelectOnCtrlClick);
		
		for (int32 i = 0; i < GroupKeyActions.Num(); ++i)
		{
			if (GroupKeyActions[i])
			{
				EnhancedInputComponent->BindAction(GroupKeyActions[i], ETriggerEvent::Started, this, &AGS_RTSController::OnGroupKey, i);
			}
		}

		for (int32 i = 0; i < CameraKeyActions.Num(); ++i)
		{
			if (CameraKeyActions[i])
			{
				EnhancedInputComponent->BindAction(CameraKeyActions[i], ETriggerEvent::Started, this, &AGS_RTSController::OnCameraKey, i);
			}
		}

		//[RTS Skill] 1-4 키 바인딩
		for (int32 i = 0; i < RTSSkillKeyActions.Num(); ++i)
		{
			if (RTSSkillKeyActions[i])
			{
				EnhancedInputComponent->BindAction(RTSSkillKeyActions[i], ETriggerEvent::Started, this, &AGS_RTSController::OnRTSSkillKey, i);
			}
		}

		// 스페이스바로 마지막 공격 위치로 카메라 이동
		if (JumpToAttackAction)
		{
			EnhancedInputComponent->BindAction(JumpToAttackAction, ETriggerEvent::Started, this, &AGS_RTSController::OnJumpToLastAttack);
		}
	}
}

void AGS_RTSController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 마우스 엣지 감지 최적화: 마우스 위치나 뷰포트 크기가 변했을 때만 방향 재계산
	float MouseX, MouseY;
	int32 ViewportX, ViewportY;
	GetViewportSize(ViewportX, ViewportY);
	
	if (GetMousePosition(MouseX, MouseY))
	{
		FVector2D CurrentMousePos(MouseX, MouseY);
		FIntPoint CurrentViewportSize(ViewportX, ViewportY);

		if (!CurrentMousePos.Equals(LastMousePosition) || CurrentViewportSize != LastViewportSize)
		{
			FVector2D NewEdgeDir = CalculateMouseEdgeDirection(CurrentMousePos, CurrentViewportSize);
			
			// 방향이 바뀌었을 때만 커서 업데이트 호출
			if (!NewEdgeDir.Equals(MouseEdgeDir))
			{
				MouseEdgeDir = NewEdgeDir;
				if (bCursorReady && !bSeekerHovered)
				{
					UpdateCursorForEdgeScroll();
				}
			}

			LastMousePosition = CurrentMousePos;
			LastViewportSize = CurrentViewportSize;
		}
	}

	FVector2D FinalDir = GetFinalDirection();
	if (!FinalDir.IsNearlyZero())
	{
		MoveCamera(FinalDir, DeltaTime);
	}
}

void AGS_RTSController::CameraMove(const FInputActionValue& InputValue)
{
	const FVector2D MoveInput = InputValue.Get<FVector2D>();
	KeyboardDir = MoveInput;
}

void AGS_RTSController::CameraMoveEnd()
{
	KeyboardDir = FVector2D::ZeroVector;
}


void AGS_RTSController::OnCommandMove(const FInputActionValue& Value)
{
	MoveSelectedUnits();
}

void AGS_RTSController::MoveSelectedUnits()
{
	CurrentCommand = ERTSCommand::Move;
	OnRTSCommandChanged.Broadcast(CurrentCommand);

	// 키보드 단축키 사운드 재생
	if (CommandButtonSound)
	{
		UGameplayStatics::PlaySound2D(this, CommandButtonSound);
	}

	UpdateCursorForEdgeScroll();
}


void AGS_RTSController::OnCommandAttack(const FInputActionValue& Value)
{
	AttackSelectedUnits();
}

void AGS_RTSController::AttackSelectedUnits()
{
	CurrentCommand = ERTSCommand::Attack;
	OnRTSCommandChanged.Broadcast(CurrentCommand);

	// 키보드 단축키 사운드 재생
	if (CommandButtonSound)
	{
		UGameplayStatics::PlaySound2D(this, CommandButtonSound);
	}

	UpdateCursorForEdgeScroll();
}


void AGS_RTSController::OnCommandStop(const FInputActionValue& Value)
{
	StopSelectedUnits();
}

void AGS_RTSController::StopSelectedUnits()
{
	CurrentCommand = ERTSCommand::Stop;

	Server_RTSStop();

	// 키보드 단축키 사운드 재생
	if (CommandButtonSound)
	{
		UGameplayStatics::PlaySound2D(this, CommandButtonSound);
	}

	// 즉시 실행 명령이므로 바로 None으로 변경 (우클릭 취소 사운드 방지)
	CurrentCommand = ERTSCommand::None;
	OnRTSCommandChanged.Broadcast(CurrentCommand);

	UpdateCursorForEdgeScroll();
}


void AGS_RTSController::OnCommandHold(const FInputActionValue& Value)
{
	HoldSelectedUnits();
}

void AGS_RTSController::HoldSelectedUnits()
{
	CurrentCommand = ERTSCommand::Hold;

	Server_RTSHold();

	// 키보드 단축키 사운드 재생
	if (CommandButtonSound)
	{
		UGameplayStatics::PlaySound2D(this, CommandButtonSound);
	}

	// 즉시 실행 명령이므로 바로 None으로 변경 (우클릭 취소 사운드 방지)
	CurrentCommand = ERTSCommand::None;
	OnRTSCommandChanged.Broadcast(CurrentCommand);

	UpdateCursorForEdgeScroll();
}


void AGS_RTSController::OnCommandSkill(const FInputActionValue& Value)
{
	SkillSelectedUnits();
}

void AGS_RTSController::SkillSelectedUnits()
{
	CurrentCommand = ERTSCommand::Skill;

	Server_RTSSkill();

	// 키보드 단축키 사운드 재생
	if (CommandButtonSound)
	{
		UGameplayStatics::PlaySound2D(this, CommandButtonSound);
	}

	// 현재는 논타겟팅 스킬이므로 즉시 완료 처리 (우클릭 취소 사운드 방지)
	// TODO: 타게팅 스킬 추가 시 스킬 타입별 분기 처리 필요
	CurrentCommand = ERTSCommand::None;
	OnRTSCommandChanged.Broadcast(CurrentCommand);

	UpdateCursorForEdgeScroll();
}


void AGS_RTSController::OnLeftMousePressed()
{
	//[RTS Skill] 타겟팅 모드 체크 (최우선)
	if (RTSSkillComp && RTSSkillComp->IsInSkillTargetingMode())
	{
		FHitResult Hit;
		if (GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel2), true, Hit))
		{
			HandleSkillTargetingClick(Hit.Location);
		}
		return; // 다른 입력 처리 방지
	}

	if (bCtrlDown && !bShiftDown)
	{
		SelectOnCtrlClick();
		return;
	}

	if (bShiftDown)
	{
		ToggleOnShiftClick();
		return;
	}

	FHitResult Hit;
	bool bHit = GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel2), true, Hit);

	// 명령 모드에 따라
	switch (CurrentCommand)
	{
	case ERTSCommand::Move:
		if (bHit)
		{
			SpawnCommandDecal(ERTSCommand::Move, Hit.Location);

			Server_RTSMove(Hit.Location);
			if (CommandMoveSound)
			{
				UGameplayStatics::PlaySound2D(this, CommandMoveSound);
			}
		}
		break;
	case ERTSCommand::Attack:
		if (bHit)
		{
			ShowAttackCursor();

			if (AGS_Character* Target = Cast<AGS_Character>(Hit.GetActor()))
			{
				SpawnCommandDecal(ERTSCommand::Attack, Target->GetActorLocation());
				Server_RTSAttack(Target);
			}
			else
			{
				SpawnCommandDecal(ERTSCommand::Attack, Hit.Location);
				Server_RTSAttackMove(Hit.Location);
			}

			if (CommandAttackSound)
			{
				UGameplayStatics::PlaySound2D(this, CommandAttackSound);
			}
		}
		break;
	default:
		if (bHit)
		{
			if (AGS_Seeker* Seeker = Cast<AGS_Seeker>(Hit.GetActor()))
			{
				ClearUnitSelection();
				SelectedSeeker = Seeker;
				OnSeekerSelectionChanged.Broadcast(SelectedSeeker);
				
				if (MouseClickSound)
				{
					UGameplayStatics::PlaySound2D(this, MouseClickSound);
				}
				return;
			}
		}

		if (SelectedSeeker)
		{
			SelectedSeeker = nullptr;
			OnSeekerSelectionChanged.Broadcast(nullptr);
		}
		
		if (AGS_RTSHUD* HUD = Cast<AGS_RTSHUD>(GetHUD()))
		{
			HUD->StartSelection();
			// 드래그 선택 시작 시에는 소리를 재생하지 않거나, 필요하다면 여기서 재생
		}
		break;
	}
	
	CurrentCommand = ERTSCommand::None;
	OnRTSCommandChanged.Broadcast(CurrentCommand);

	UpdateCursorForEdgeScroll();
}

void AGS_RTSController::OnLeftMouseReleased()
{
	if (bShiftDown || bCtrlDown)
	{
		return;
	}
	
	if (AGS_RTSHUD* HUD = Cast<AGS_RTSHUD>(GetHUD()))
	{
		HUD->StopSelection();
	}
}

void AGS_RTSController::OnRightMousePressed(const FInputActionValue& InputValue)
{
	//[RTS Skill] 타겟팅 모드 취소 (최우선)
	if (RTSSkillComp && RTSSkillComp->IsInSkillTargetingMode())
	{
		CancelGuardianSkillTargeting();

		if (CommandCancelSound)
		{
			UGameplayStatics::PlaySound2D(this, CommandCancelSound);
		}
		return;
	}

	// 커맨드 모드 중이면 취소
	if (CurrentCommand != ERTSCommand::None)
	{
		CurrentCommand = ERTSCommand::None;
		OnRTSCommandChanged.Broadcast(CurrentCommand);

		if (CommandCancelSound)
		{
			UGameplayStatics::PlaySound2D(this, CommandCancelSound);
		}
		return;
	}
	
	FHitResult GroundHit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel2), true, GroundHit))
	{
		return;
	}

	SpawnCommandDecal(ERTSCommand::Move, GroundHit.Location);

	Server_RTSMove(GroundHit.Location);

	if (CommandMoveSound)
	{
		UGameplayStatics::PlaySound2D(this, CommandMoveSound);
	}
}

void AGS_RTSController::Client_StartGame_Implementation()
{
	Super::Client_StartGame_Implementation();

	if (UGS_ActorRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UGS_ActorRegistrySubsystem>())
	{
		const TArray<TWeakObjectPtr<AGS_Seeker>>& Seekers = Registry->GetSeekers();
		for (const TWeakObjectPtr<AGS_Seeker>& SeekerPtr : Seekers)
		{
			AGS_Seeker* Seeker = SeekerPtr.Get();
			if (IsValid(Seeker))
			{
				Seeker->HPTextWidgetComp->SetVisibility(true);
				Seeker->OnSeekerHover.AddDynamic(this, &AGS_RTSController::HandleSeekerHover);

				// 시커 파괴 시 정리를 위한 델리게이트 바인딩
				Seeker->OnDestroyed.AddDynamic(this, &AGS_RTSController::OnTrackedSeekerDestroyed);
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("준비 완료. TODO: 화면 가리개 제거."));
}

void AGS_RTSController::OnEscapeButtonClicked()
{
	//[RTS Skill] 타겟팅 모드 취소 우선
	if (RTSSkillComp && RTSSkillComp->IsInSkillTargetingMode())
	{
		CancelGuardianSkillTargeting();

		if (CommandCancelSound)
		{
			UGameplayStatics::PlaySound2D(this, CommandCancelSound);
		}
		return;
	}

	if (CurrentCommand != ERTSCommand::None)
	{
		CurrentCommand = ERTSCommand::None;
		OnRTSCommandChanged.Broadcast(CurrentCommand);

		if (CommandCancelSound)
		{
			UGameplayStatics::PlaySound2D(this, CommandCancelSound);
		}

		UpdateCursorForEdgeScroll();
	}
}

FVector2D AGS_RTSController::GetKeyboardDirection() const
{
	if (FMath::Abs(KeyboardDir.X) > 0.1f || FMath::Abs(KeyboardDir.Y) > 0.1f)
	{
		return KeyboardDir;
	}
	return FVector2D::ZeroVector;
}

FVector2D AGS_RTSController::CalculateMouseEdgeDirection(FVector2D MousePos, FIntPoint ViewportSize) const
{
	float EdgeW = ViewportSize.X * EdgeScreenRatio;
	float EdgeH = ViewportSize.Y * EdgeScreenRatio;

	FVector2D Dir = FVector2D::ZeroVector;
	if (MousePos.X <= EdgeW) // 좌·우 엣지 판정
	{
		Dir.X = -1.f;
	}
	else if (MousePos.X >= ViewportSize.X - EdgeW)
	{
		Dir.X = 1.f;
	}
	
	if (MousePos.Y <= EdgeH) // 위·아래 엣지 판정 
	{
		Dir.Y = 1.f;
	}
	else if (MousePos.Y >= ViewportSize.Y - EdgeH)
	{
		Dir.Y = -1.f;
	}

	return Dir;
}

// 키보드 입력이 1순위로 
FVector2D AGS_RTSController::GetFinalDirection() const
{
	FVector2D Dir = GetKeyboardDirection();
	return !Dir.IsNearlyZero() ? Dir : MouseEdgeDir;
}

void AGS_RTSController::MoveCamera(const FVector2D& Direction, float DeltaTime)
{
	FVector2D NormDir = Direction.GetSafeNormal();
	FVector Delta = FVector(NormDir.Y, NormDir.X, 0.f) * CameraSpeed * DeltaTime;
	if (CameraActor)
	{
		CameraActor->AddActorWorldOffset(Delta, true);
	}
}

void AGS_RTSController::InitCameraActor()
{
	for (TActorIterator<AGS_RTSCamera> It(GetWorld()); It; ++It)
	{
		CameraActor = *It;
		break;
	}

	if (!CameraActor)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		CameraActor = GetWorld()->SpawnActor<AGS_RTSCamera>(AGS_RTSCamera::StaticClass(), Params);
	}

	// 뷰 타깃으로 설정
	if (CameraActor && GetViewTarget() != CameraActor) 
	{
		SetViewTarget(CameraActor);
	}
}

void AGS_RTSController::HideDungeonElements()
{
	UE_LOG(LogTemp, Warning, TEXT("[숨김 처리 로그]===== CLIENT RPC RECEIVED on %s! ====="), *GetName());
	if (CameraActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[숨김 처리 로그] RTSCamera is valid. Hiding walls."));
		CameraActor->HideWallAndCeiling();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[숨김 처리 로그] 으악! 실패!! 카메라 액터가 없음."));
	}
}

void AGS_RTSController::SetRTSCursor(const FName& CursorPath)
{
	// 커서 시스템 준비 안 됨 → 무시
	if (!bCursorReady)
	{
		return;
	}

	// 같은 커서면 스킵 (성능 최적화)
	if (CurrentCursorPath == CursorPath)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameViewportClient* ViewportClient = World->GetGameViewport();
	if (!ViewportClient)
	{
		return;
	}

	// 커서 변경
	if (CursorPath.IsNone())
	{
		ViewportClient->SetHardwareCursor(EMouseCursor::Default, NAME_None, FIntPoint(48, 48));
	}
	else
	{
		ViewportClient->SetHardwareCursor(EMouseCursor::Default, CursorPath, FIntPoint(48, 48));
	}

	CurrentCursorPath = CursorPath;
}

void AGS_RTSController::UpdateCursorForEdgeScroll()
{
	bool bShouldShowEdgeCursor = !MouseEdgeDir.IsNearlyZero();
    
	if (bShouldShowEdgeCursor)
	{
		if (MouseEdgeDir.Y > 0.5f) // 위
		{
			SetRTSCursor(ScrollUpCursorPath);
		}
		else if (MouseEdgeDir.Y < -0.5f) // 아래
		{
			SetRTSCursor(ScrollDownCursorPath);
		}
		else if (MouseEdgeDir.X < -0.5f) // 왼쪽
		{
			SetRTSCursor(ScrollLeftCursorPath);
		}
		else if (MouseEdgeDir.X > 0.5f) // 오른쪽 
		{
			SetRTSCursor(ScrollRightCursorPath);
		}
	}
	else
	{
		UpdateCursorForCommand();
	}
}

void AGS_RTSController::UpdateCursorForCommand()
{
	if (bShowAttackCursor)
	{
		return;
	}

	//[RTS Skill] 타겟팅 모드 커서
	if (RTSSkillComp && RTSSkillComp->IsInSkillTargetingMode())
	{
		SetRTSCursor(SkillTargetingCursorPath);
		return;
	}

	switch (CurrentCommand)
	{
	case ERTSCommand::Attack:
		SetRTSCursor(AttackCommandCursorPath);
		break;
	case ERTSCommand::Move:
		SetRTSCursor(CommandCursorPath);
		break;
	default:
		SetRTSCursor(DefaultCursorPath);
		break;
	}
}

void AGS_RTSController::ShowAttackCursor()
{
	bShowAttackCursor = true;
	SetRTSCursor(SeekerAttackCursorPath);

	GetWorldTimerManager().SetTimer(
		AttackCursorTimerHandle,
		[this]() 
		{ 
			bShowAttackCursor = false; 
			UpdateCursorForEdgeScroll();
		},
		0.3f,
		false
	);
}

void AGS_RTSController::InitializeCursor()
{
	if (bCursorReady)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameViewportClient* ViewportClient = World->GetGameViewport();
	if (!ViewportClient)
	{
		// 뷰포트가 아직 없으면 재시도 (0.1초 후)
		UE_LOG(LogTemp, Warning, TEXT("[RTSCursor] ViewportClient null. Retrying in 0.1s..."));
		GetWorldTimerManager().SetTimer(CursorInitTimerHandle, this, &AGS_RTSController::InitializeCursor, 0.1f, false);
		return;
	}

	// 커서 설정 시도
	ViewportClient->SetHardwareCursor(EMouseCursor::Default, DefaultCursorPath, FIntPoint(48, 48));

	ApplyRTSInputMode();

	bCursorReady = true;
	CurrentCursorPath = DefaultCursorPath;
}

void AGS_RTSController::TryInitializeCursorInTick()
{
	// Deprecated: Timer logic used instead.
}

void AGS_RTSController::HandleSeekerHover(bool bIsHover)
{
	bSeekerHovered = bIsHover;
	
	if (bIsHover)
	{
		SetRTSCursor(SeekerAttackCursorPath);
	}
	else
	{
		if (!MouseEdgeDir.IsNearlyZero())
		{
			UpdateCursorForEdgeScroll();
		}
		else
		{
			UpdateCursorForCommand();
		}
	}
}


void AGS_RTSController::SelectOnCtrlClick()
{
	if (!GetWorld()) return;

	int32 ViewportX, ViewportY;
	GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0) return;

	FHitResult Hit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel1), true, Hit)) return;

	AGS_Monster* Monster = Cast<AGS_Monster>(Hit.GetActor());
	if (!Monster || !CheckMonsterSelectable(Monster)) return;
	
	ECharacterType MonsterType = Monster->GetCharacterType();
	TArray<AGS_Monster*> SameTypeUnits;
	
	// [최적화] GameState에 캐싱된 LiveMonsters 리스트를 사용하여 전체 액터 순회 대체
	AGS_InGameGS* GS = GetWorld()->GetGameState<AGS_InGameGS>();
	if (!GS) return;

	for (AGS_Monster* CurrentMonster : GS->LiveMonsters)
	{
		if (!IsValid(CurrentMonster) || CurrentMonster->GetCharacterType() != MonsterType) continue;

		FVector2D ScreenPos;
		if (ProjectWorldLocationToScreen(CurrentMonster->GetActorLocation(), ScreenPos, true))
		{
			if (ScreenPos.X >= 0.0f && ScreenPos.X <= ViewportX && ScreenPos.Y >= 0.0f && ScreenPos.Y <= ViewportY * 0.77f)
			{
				SameTypeUnits.Add(CurrentMonster);
			}
		}
	}

	SameTypeUnits.Sort([Monster](const AGS_Monster& A, const AGS_Monster& B) {
		return Monster->GetDistanceTo(&A) < Monster->GetDistanceTo(&B);
	});

	TArray<AGS_Monster*> Selection;
	for (int32 i = 0; i < SameTypeUnits.Num() && Selection.Num() < MaxSelectableUnits; ++i)
	{
		Selection.Add(SameTypeUnits[i]);
	}

	AddMultipleUnitsToSelection(Selection);
}

void AGS_RTSController::ToggleOnShiftClick()
{
	FHitResult ShiftHit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel1), true, ShiftHit))
	{
		return;
	}

	if (AGS_Monster* Monster = Cast<AGS_Monster>(ShiftHit.GetActor()))
	{
		if (!CheckMonsterSelectable(Monster))
		{
			return;
		}
		
		if (UnitSelection.Contains(Monster))
		{
			RemoveUnitFromSelection(Monster);
		}
		else
		{
			AddUnitToSelection(Monster);
		}
	}
}


void AGS_RTSController::AddUnitToSelection(AGS_Monster* Unit)
{
	if (!Unit || !CheckMonsterSelectable(Unit))
	{
		return;
	}

	if (UnitSelection.Num() >= MaxSelectableUnits)
	{
		return;
	}

	// 첫 번째로 추가되는 유닛만 소리 재생
	bool bShouldPlaySound = UnitSelection.IsEmpty();

	Unit->OnMonsterDead.AddUniqueDynamic(this, &AGS_RTSController::OnSelectedUnitDead);

	UnitSelection.AddUnique(Unit);
	OnSelectionChanged.Broadcast(UnitSelection);
	OnSelectedUnitsSkillChanged.Broadcast(HasAnySelectedUnitSkill());
	Unit->SetSelected(true, bShouldPlaySound);

	// 서버에 동기화
	if (!HasAuthority())
	{
		Server_AddUnitToSelection(Unit);
	}
}

// 여러 유닛을 한번에 선택할 때 사용할 새로운 함수 추가
void AGS_RTSController::AddMultipleUnitsToSelection(const TArray<AGS_Monster*>& Units)
{
	if (Units.IsEmpty())
	{
		return;
	}

	// 기존 선택 해제 (로컬만, RPC 없이)
	for (AGS_Monster* Unit : UnitSelection)
	{
		if (IsValid(Unit))
		{
			Unit->OnMonsterDead.RemoveDynamic(this, &AGS_RTSController::OnSelectedUnitDead);
			Unit->SetSelected(false);
		}
	}
	UnitSelection.Reset();

	// 새 유닛 선택
	int32 AddedCount = 0;
	for (int32 i = 0; i < Units.Num(); ++i)
	{
		if (AddedCount >= MaxSelectableUnits)
		{
			break;
		}

		AGS_Monster* Unit = Units[i];
		if (!Unit)
		{
			continue;
		}

		if (!CheckMonsterSelectable(Unit))
		{
			continue;
		}

		Unit->OnMonsterDead.AddUniqueDynamic(this, &AGS_RTSController::OnSelectedUnitDead);

		UnitSelection.AddUnique(Unit);
		// 첫 번째 유닛만 소리 재생
		Unit->SetSelected(true, i == 0);
		AddedCount++;
	}

	OnSelectionChanged.Broadcast(UnitSelection);
	OnSelectedUnitsSkillChanged.Broadcast(HasAnySelectedUnitSkill());

	// 서버에 동기화 (한 번만)
	if (!HasAuthority())
	{
		Server_SetMultipleUnitsSelection(UnitSelection);
	}
}

void AGS_RTSController::SelectSameTypeFromSelection(AGS_Monster* Unit)
{
	if (!Unit)
	{
		return;
	}

	if (!UnitSelection.Contains(Unit))
	{
		return;
	}
	
	ECharacterType MonsterType = Unit->GetCharacterType();
	TArray<AGS_Monster*> SameTypeUnits;
	for (AGS_Monster* Monster : UnitSelection)
	{
		if (Monster->GetCharacterType() != MonsterType)
		{
			continue;
		}

		SameTypeUnits.Add(Monster);
	}
	
	AddMultipleUnitsToSelection(SameTypeUnits);
}

void AGS_RTSController::RemoveUnitFromSelection(AGS_Monster* Unit)
{
	if (!Unit)
	{
		return;
	}

	Unit->OnMonsterDead.RemoveDynamic(this, &AGS_RTSController::OnSelectedUnitDead);

	UnitSelection.Remove(Unit);
	OnSelectionChanged.Broadcast(UnitSelection);
	OnSelectedUnitsSkillChanged.Broadcast(HasAnySelectedUnitSkill());
	Unit->SetSelected(false);

	// 서버에 동기화
	if (!HasAuthority())
	{
		Server_RemoveUnitFromSelection(Unit);
	}
}

void AGS_RTSController::ClearUnitSelection()
{
	for (AGS_Monster* Unit : UnitSelection)
	{
		if (IsValid(Unit))
		{
			Unit->OnMonsterDead.RemoveDynamic(this, &AGS_RTSController::OnSelectedUnitDead);

			Unit->SetSelected(false);
		}
	}

	UnitSelection.Reset();
	OnSelectionChanged.Broadcast(UnitSelection);
	OnSelectedUnitsSkillChanged.Broadcast(HasAnySelectedUnitSkill());

	// 서버에 동기화
	if (!HasAuthority())
	{
		Server_ClearUnitSelection();
	}
}


void AGS_RTSController::OnCtrlPressed(const FInputActionInstance& InputInstance)
{
	bCtrlDown = true;
}

void AGS_RTSController::OnCtrlReleased(const FInputActionInstance& InputInstance)
{
	bCtrlDown = false;
}

void AGS_RTSController::OnShiftPressed(const FInputActionInstance& InputInstance)
{
	bShiftDown = true;
}

void AGS_RTSController::OnShiftReleased(const FInputActionInstance& InputInstance)
{
	bShiftDown = false;
}


// 유닛 그룹 저장 + 불러오기
void AGS_RTSController::OnGroupKey(const FInputActionInstance& InputInstance, int32 GroupIdx)
{
	if (bCtrlDown) // 부대 저장
	{
		UnitGroups[GroupIdx].Units = UnitSelection;
		return;
	}

	// 부대 호출
	if (!UnitGroups.IsValidIndex(GroupIdx))
	{
		return;
	}

	// 더블 클릭 감지
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const bool bIsSameGroup = (LastPressedGroupIdx == GroupIdx);
	const bool bWithinTimeWindow = (CurrentTime - LastGroupKeyPressTime) <= DoubleClickTimeThreshold;

	if (bIsSameGroup && bWithinTimeWindow)
	{
		// 더블 클릭: 카메라 이동
		MoveCameraToGroupCenter(GroupIdx);

		// 상태 초기화
		GetWorldTimerManager().ClearTimer(GroupDoubleClickTimerHandle);
		LastPressedGroupIdx = -1;
		LastGroupKeyPressTime = 0.f;
	}
	else
	{
		// 첫 번째 클릭: 그룹 선택
		AddMultipleUnitsToSelection(UnitGroups[GroupIdx].Units);

		// 더블 클릭 대기 상태
		LastPressedGroupIdx = GroupIdx;
		LastGroupKeyPressTime = CurrentTime;

		// 타이머 시작 (0.3초 후 상태 초기화)
		GetWorldTimerManager().ClearTimer(GroupDoubleClickTimerHandle);
		GetWorldTimerManager().SetTimer(
			GroupDoubleClickTimerHandle,
			this,
			&AGS_RTSController::ResetGroupDoubleClickState,
			DoubleClickTimeThreshold,
			false
		);
	}
}

// 카메라 위치 저장 + 불러오기 
void AGS_RTSController::OnCameraKey(const FInputActionInstance& InputInstance, int32 CameraIndex)
{
	if (bShiftDown) // 저장
	{
		if (CameraActor)
		{
			SavedCameraPositions.Add(CameraIndex, CameraActor->GetActorLocation());
		}
	}
	else // 로드
	{
		if (CameraActor && SavedCameraPositions.Contains(CameraIndex))
		{
			CameraActor->SetActorLocation(SavedCameraPositions[CameraIndex]);
		}
	}
}

void AGS_RTSController::MoveAIViaMinimap(const FVector& WorldLocation)
{
	SpawnCommandDecal(ERTSCommand::Move, WorldLocation);

	Server_RTSMove(WorldLocation);

	if (CommandMoveSound)
	{
		UGameplayStatics::PlaySound2D(this, CommandMoveSound);
	}

	CurrentCommand = ERTSCommand::None;
	OnRTSCommandChanged.Broadcast(CurrentCommand);
}

void AGS_RTSController::AttackAIViaMinimap(const FVector& WorldLocation)
{
	SpawnCommandDecal(ERTSCommand::Attack, WorldLocation);

	Server_RTSAttackMove(WorldLocation);

	if (CommandAttackSound)
	{
		UGameplayStatics::PlaySound2D(this, CommandAttackSound);
	}

	CurrentCommand = ERTSCommand::None;
	OnRTSCommandChanged.Broadcast(CurrentCommand);
}

void AGS_RTSController::MoveCameraViaMinimap(const FVector& WorldLocation)
{
	if (CameraActor)
	{
		FVector NewLocation = FVector(WorldLocation.X, WorldLocation.Y, CameraActor->GetActorLocation().Z);
		CameraActor->SetActorLocation(NewLocation);
	}
}

// 그룹 중심 위치 계산
FVector AGS_RTSController::CalculateGroupCenterLocation(int32 GroupIdx) const
{
	if (!UnitGroups.IsValidIndex(GroupIdx))
	{
		return FVector::ZeroVector;
	}

	const TArray<AGS_Monster*>& Units = UnitGroups[GroupIdx].Units;
	if (Units.IsEmpty())
	{
		return FVector::ZeroVector;
	}

	// 유효한 유닛들의 위치 합산
	FVector SumLocation = FVector::ZeroVector;
	int32 ValidUnitCount = 0;

	for (AGS_Monster* Unit : Units)
	{
		if (IsValid(Unit))
		{
			SumLocation += Unit->GetActorLocation();
			++ValidUnitCount;
		}
	}

	// 평균 위치 반환
	if (ValidUnitCount > 0)
	{
		return SumLocation / static_cast<float>(ValidUnitCount);
	}

	return FVector::ZeroVector;
}

// 그룹 중심으로 카메라 이동
void AGS_RTSController::MoveCameraToGroupCenter(int32 GroupIdx)
{
	const FVector GroupCenter = CalculateGroupCenterLocation(GroupIdx);

	// 유효한 위치가 아니면 이동하지 않음
	if (GroupCenter.IsZero())
	{
		return;
	}

	// 기존 미니맵 카메라 이동 함수 재사용
	MoveCameraViaMinimap(GroupCenter);
}

// 더블 클릭 상태 초기화
void AGS_RTSController::ResetGroupDoubleClickState()
{
	LastPressedGroupIdx = -1;
	LastGroupKeyPressTime = 0.f;
}

// ==========================================
// RTS 명령 데칼 시스템
// ==========================================

void AGS_RTSController::SpawnCommandDecal(ERTSCommand CommandType, const FVector& Location)
{
	// 로컬 컨트롤러 체크 (네트워크 안전성)
	if (!IsLocalController())
	{
		return;
	}

	// None 명령은 데칼 표시 안 함
	if (CommandType == ERTSCommand::None)
	{
		return;
	}

	// 월드 유효성 검사
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 머티리얼 검색
	UMaterialInterface* const* FoundMaterial = CommandDecalMaterials.Find(CommandType);
	if (!FoundMaterial || !(*FoundMaterial))
	{
		return;
	}

	// 데칼 스폰 위치 계산 (Z파이팅 방지 오프셋)
	FVector DecalLocation = Location + FVector(0.0f, 0.0f, CommandDecalZOffset);

	// 데칼 회전 (지면에 수평으로 투사)
	FRotator DecalRotation = FRotator(-90.0f, 0.0f, 0.0f);

	// 데칼 스폰
	UGameplayStatics::SpawnDecalAtLocation(
		World,
		*FoundMaterial,
		CommandDecalSize,
		DecalLocation,
		DecalRotation,
		CommandDecalLifeSpan
	);
}

// ==========================================
// 선택 동기화 RPC 구현 (클라이언트 → 서버)
// ==========================================

void AGS_RTSController::Server_AddUnitToSelection_Implementation(AGS_Monster* Unit)
{
	if (!IsValid(Unit) || !CheckMonsterSelectable(Unit))
	{
		return;
	}

	if (UnitSelection.Num() >= MaxSelectableUnits)
	{
		return;
	}

	UnitSelection.AddUnique(Unit);
}

void AGS_RTSController::Server_RemoveUnitFromSelection_Implementation(AGS_Monster* Unit)
{
	if (!IsValid(Unit))
	{
		return;
	}

	UnitSelection.Remove(Unit);
}

void AGS_RTSController::Server_ClearUnitSelection_Implementation()
{
	UnitSelection.Empty();
}

void AGS_RTSController::Server_SetMultipleUnitsSelection_Implementation(const TArray<AGS_Monster*>& Units)
{
	UnitSelection.Empty();

	int32 AddedCount = 0;
	for (AGS_Monster* Unit : Units)
	{
		if (AddedCount >= MaxSelectableUnits)
		{
			break;
		}

		if (!IsValid(Unit) || !CheckMonsterSelectable(Unit))
		{
			continue;
		}

		UnitSelection.AddUnique(Unit);
		AddedCount++;
	}
}

// ==========================================
// 명령 RPC 구현
// ==========================================

bool AGS_RTSController::Server_RTSMove_Validate(const FVector& Dest)
{
	// 유닛 선택 배열 검증
	if (UnitSelection.IsEmpty()) return false;
	return true;
}

void AGS_RTSController::Server_RTSMove_Implementation(const FVector& Dest)
{
	TArray<AGS_Monster*> Commandables;
	GatherCommandableUnits(Commandables);

	for (int32 i = 0; i < Commandables.Num(); ++i)
	{
		AGS_Monster* Unit = Commandables[i];
		if (!IsValid(Unit)) continue;

		// 유닛 소유권/권한 검증 추가
		if (Unit->GetOwner() != this && !HasAuthority()) continue;

		if (AGS_AIController* AIController = Cast<AGS_AIController>(Unit->GetController()))
		{
			AIController->ClearCurrentTarget();

			if (UBlackboardComponent* BlackboardComp = AIController->GetBlackboardComponent())
			{
				BlackboardComp->ClearValue(AGS_AIController::CommandKey);
				BlackboardComp->SetValueAsEnum(AGS_AIController::CommandKey, static_cast<uint8>(ERTSCommand::Move));
				BlackboardComp->SetValueAsVector(AGS_AIController::MoveLocationKey, Dest);
				BlackboardComp->ClearValue(AGS_AIController::TargetActorKey);
				BlackboardComp->SetValueAsBool(AGS_AIController::TargetLockedKey, false);

				if (i == 0 && Unit->MonsterAudioComponent)
				{
					Unit->MonsterAudioComponent->PlayRTSCommandSound(ERTSCommandSoundType::Move);
				}
			}
		}
	}
}

bool AGS_RTSController::Server_RTSAttackMove_Validate(const FVector& Dest)
{
	if (UnitSelection.IsEmpty()) return false;
	return true;
}

void AGS_RTSController::Server_RTSAttackMove_Implementation(const FVector& Dest)
{
	TArray<AGS_Monster*> Commandables;
	GatherCommandableUnits(Commandables);

	for (int32 i = 0; i < Commandables.Num(); ++i)
	{
		AGS_Monster* Unit = Commandables[i];
		if (!IsValid(Unit)) continue;

		if (AGS_AIController* AIController = Cast<AGS_AIController>(Unit->GetController()))
		{
			if (UBlackboardComponent* BlackboardComp = AIController->GetBlackboardComponent())
			{
				BlackboardComp->ClearValue(AGS_AIController::CommandKey);
				BlackboardComp->SetValueAsEnum(AGS_AIController::CommandKey, static_cast<uint8>(ERTSCommand::Attack));
				BlackboardComp->SetValueAsVector(AGS_AIController::MoveLocationKey, Dest);
				BlackboardComp->SetValueAsBool(AGS_AIController::TargetLockedKey, false);

				if (i == 0 && Unit->MonsterAudioComponent)
				{
					Unit->MonsterAudioComponent->PlayRTSCommandSound(ERTSCommandSoundType::Attack);
				}
			}
		}
	}
}

bool AGS_RTSController::Server_RTSAttack_Validate(AGS_Character* TargetActor)
{
	if (UnitSelection.IsEmpty() || !IsValid(TargetActor)) return false;
	return true;
}

void AGS_RTSController::Server_RTSAttack_Implementation(AGS_Character* TargetActor)
{
	// 타겟 유효성 검증
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server_RTSAttack] Invalid TargetActor"));
		return;
	}

	// 빈사 상태인 시커는 공격 대상이 될 수 없음 -> 해당 위치로 Attack Move
	if (AGS_Seeker* SeekerTarget = Cast<AGS_Seeker>(TargetActor))
	{
		if (SeekerTarget->IsInDyingState())
		{
			Server_RTSAttackMove(SeekerTarget->GetActorLocation());
			return;
		}
	}

	// 서버에서 직접 명령 가능한 유닛 수집
	TArray<AGS_Monster*> Commandables;
	GatherCommandableUnits(Commandables);

	for (int32 i = 0; i < Commandables.Num(); ++i)
	{
		AGS_Monster* Unit = Commandables[i];
		if (!IsValid(Unit))
		{
			continue;
		}

		if (AGS_AIController* AIController = Cast<AGS_AIController>(Unit->GetController()))
		{
			if (UBlackboardComponent* BlackboardComp = AIController->GetBlackboardComponent())
			{
				BlackboardComp->ClearValue(AGS_AIController::CommandKey);
				BlackboardComp->SetValueAsEnum(AGS_AIController::CommandKey, static_cast<uint8>(ERTSCommand::Attack));
				BlackboardComp->SetValueAsObject(AGS_AIController::TargetActorKey, TargetActor);
				BlackboardComp->ClearValue(AGS_AIController::MoveLocationKey);
				BlackboardComp->SetValueAsBool(AGS_AIController::TargetLockedKey, true);

				// 첫 번째 유닛만 공격 사운드 재생
				if (i == 0 && Unit->MonsterAudioComponent)
				{
					Unit->MonsterAudioComponent->PlayRTSCommandSound(ERTSCommandSoundType::Attack);
				}
			}
		}
	}
}

void AGS_RTSController::Server_RTSStop_Implementation()
{
	// 서버에서 직접 명령 가능한 유닛 수집
	TArray<AGS_Monster*> Commandables;
	GatherCommandableUnits(Commandables);

	for (int32 i = 0; i < Commandables.Num(); ++i)
	{
		AGS_Monster* Unit = Commandables[i];
		if (!IsValid(Unit))
		{
			continue;
		}

		if (AGS_AIController* AIController = Cast<AGS_AIController>(Unit->GetController()))
		{
			AIController->StopMovement();

			// 기존 타겟을 명시적으로 클리어
			AIController->ClearCurrentTarget();

			if (UBlackboardComponent* BlackboardComp = AIController->GetBlackboardComponent())
			{
				BlackboardComp->ClearValue(AGS_AIController::CommandKey);
				BlackboardComp->SetValueAsEnum(AGS_AIController::CommandKey, static_cast<uint8>(ERTSCommand::None));
				BlackboardComp->ClearValue(AGS_AIController::TargetActorKey);
				BlackboardComp->SetValueAsBool(AGS_AIController::TargetLockedKey, false);

				// 첫 번째 유닛만 정지 소리 재생
				if (i == 0 && Unit->MonsterAudioComponent)
				{
					Unit->MonsterAudioComponent->PlayRTSCommandSound(ERTSCommandSoundType::Move);
				}
			}
		}
	}
}

void AGS_RTSController::Server_RTSHold_Implementation()
{
	// 서버에서 직접 명령 가능한 유닛 수집
	TArray<AGS_Monster*> Commandables;
	GatherCommandableUnits(Commandables);

	for (int32 i = 0; i < Commandables.Num(); ++i)
	{
		AGS_Monster* Unit = Commandables[i];
		if (!IsValid(Unit))
		{
			continue;
		}

		if (AGS_AIController* AIController = Cast<AGS_AIController>(Unit->GetController()))
		{
			// 기존 타겟을 명시적으로 클리어
			AIController->ClearCurrentTarget();

			if (UBlackboardComponent* BlackboardComp = AIController->GetBlackboardComponent())
			{
				BlackboardComp->ClearValue(AGS_AIController::CommandKey);
				BlackboardComp->SetValueAsEnum(AGS_AIController::CommandKey, static_cast<uint8>(ERTSCommand::Hold));
				BlackboardComp->ClearValue(AGS_AIController::TargetActorKey);
				BlackboardComp->SetValueAsBool(AGS_AIController::TargetLockedKey, false);

				// 첫 번째 유닛만 정지 소리 재생
				if (i == 0 && Unit->MonsterAudioComponent)
				{
					Unit->MonsterAudioComponent->PlayRTSCommandSound(ERTSCommandSoundType::Move);
				}
			}
		}
	}
}

void AGS_RTSController::Server_RTSSkill_Implementation()
{
	// 서버에서 직접 명령 가능한 유닛 수집
	TArray<AGS_Monster*> Commandables;
	GatherCommandableUnits(Commandables);

	for (int32 i = 0; i < Commandables.Num(); ++i)
	{
		AGS_Monster* Unit = Commandables[i];
		if (!IsValid(Unit))
		{
			continue;
		}

		if (AGS_AIController* AIController = Cast<AGS_AIController>(Unit->GetController()))
		{
			if (UBlackboardComponent* BlackboardComp = AIController->GetBlackboardComponent())
			{
				BlackboardComp->ClearValue(AGS_AIController::CommandKey);
				BlackboardComp->SetValueAsEnum(AGS_AIController::CommandKey, static_cast<uint8>(ERTSCommand::Skill));

				// 첫 번째 유닛만 스킬 소리 재생
				if (i == 0 && Unit->MonsterAudioComponent)
				{
					Unit->MonsterAudioComponent->PlayRTSCommandSound(ERTSCommandSoundType::Move);
				}
			}
		}
	}
}


bool AGS_RTSController::HasAnySelectedUnitSkill() const
{
	for (AGS_Monster* SelectedMonster : UnitSelection)
	{
		UGS_MonsterSkillComp* SkillComp = SelectedMonster->GetMonsterSkillComp();
		if (SkillComp && SkillComp->MonsterSkill)
		{
			return true; 
		}
	}
	return false; 
}

bool AGS_RTSController::CheckMonsterSelectable(AGS_Monster* Monster) const
{
	if (!IsValid(Monster))
	{
		return false;
	}
	
	if (UGS_StatComp* Stat = Monster->GetStatComp())
	{
		if (Stat->GetCurrentHealth() <= 0.f)
		{
			return false;  
		}
	}
	
	return Monster->IsSelectable();
}

void AGS_RTSController::GatherCommandableUnits(TArray<AGS_Monster*>& Out) const
{
	// 캐시 배열 초기화 (메모리는 유지하고 개수만 0으로)
	CachedCommandableUnits.Reset();

	// 현재 선택된 유닛 중에서 명령 가능한 유닛만 수집
	for (AGS_Monster* Unit : UnitSelection)
	{
		if (IsValid(Unit) && Unit->IsCommandable())
		{
			CachedCommandableUnits.Add(Unit);
		}
	}

	// 결과 복사 (Out 배열의 메모리가 충분하다면 복사 비용은 크지 않음)
	Out = CachedCommandableUnits;
}

void AGS_RTSController::OnSelectedUnitDead(AGS_Monster* Monster)
{
    if (IsValid(Monster))
    {
        if (Monster->MonsterAudioComponent)
        {
            Monster->MonsterAudioComponent->PlayRTSCommandSound(ERTSCommandSoundType::Death);
        }
    }
    RemoveUnitFromSelection(Monster);
}

//[Aether] 에테르 반환
UGS_AetherComp* AGS_RTSController::GetAetherComp() const
{
	return AetherComp;
}

// RTS 마우스 설정
void AGS_RTSController::ApplyRTSInputMode()
{
	// RTS에서는 UI와 게임 입력을 모두 받아야 함
	FInputModeGameAndUI InputModeData;
	InputModeData.SetHideCursorDuringCapture(false);
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputModeData);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

// ==========================================
// 시커 감지 시스템
// ==========================================

void AGS_RTSController::OnTrackedSeekerDestroyed(AActor* DestroyedActor)
{
	AGS_Seeker* DestroyedSeeker = Cast<AGS_Seeker>(DestroyedActor);
	if (!DestroyedSeeker)
	{
		return;
	}

	// 감지된 시커 목록에서 제거
	DetectedSeekers.Remove(DestroyedSeeker);

	// RPC 쿨다운 추적 맵에서 제거
	LastSeekerNotifyTimes.Remove(DestroyedSeeker);

	// 근접도 캐시에서 제거
	LastSeekerProximityValues.Remove(DestroyedSeeker);

}

void AGS_RTSController::UpdateSeekerDetection()
{
	if (!CameraActor)
	{
		return;
	}

	// [최적화] 서브시스템을 활용하여 시커 목록 캐싱 순회
	UGS_ActorRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UGS_ActorRegistrySubsystem>();
	if (!Registry) return;

	TArray<AGS_Seeker*> CurrentVisibleSeekers;

	const TArray<TWeakObjectPtr<AGS_Seeker>>& RegisteredSeekers = Registry->GetSeekers();
	for (int32 i = 0; i < RegisteredSeekers.Num(); ++i)
	{
		AGS_Seeker* Seeker = RegisteredSeekers[i].Get();
		
		if (IsValid(Seeker) && !Seeker->IsDead())
		{
			if (IsSeekerInCameraView(Seeker))
			{
				CurrentVisibleSeekers.Add(Seeker);

				// 화면 중앙과의 거리 계산 (0.0 = 중앙, 1.0 = 가장자리)
				float DistanceFromCenter = CalculateSeekerDistanceFromScreenCenter(Seeker);

				// 이전 값과 비교하여 유의미한 변화(5% 이상)가 있을 때만 서버로 전송
				float* LastValue = LastSeekerProximityValues.Find(Seeker);
				if (!LastValue || FMath::Abs(*LastValue - DistanceFromCenter) >= ProximityChangeThreshold)
				{
					LastSeekerProximityValues.Add(Seeker, DistanceFromCenter);
					Server_UpdateSeekerProximity(Seeker, DistanceFromCenter);
				}
			}
		}
	}

	// 새로 감지된 시커들 처리
	for (AGS_Seeker* Seeker : CurrentVisibleSeekers)
	{
		if (!DetectedSeekers.Contains(Seeker))
		{
			DetectedSeekers.Add(Seeker);
			NotifySeekerDetection(Seeker, true);
		}
	}

	// 더 이상 감지되지 않는 시커들 처리
	TArray<AGS_Seeker*> SeekersToRemove;
	for (AGS_Seeker* Seeker : DetectedSeekers)
	{
		if (!CurrentVisibleSeekers.Contains(Seeker))
		{
			SeekersToRemove.Add(Seeker);
		}
	}

	for (AGS_Seeker* Seeker : SeekersToRemove)
	{
		DetectedSeekers.Remove(Seeker);
		NotifySeekerDetection(Seeker, false);

		// 감지 해제 시 거리 1.0 (최대값)으로 설정
		Server_UpdateSeekerProximity(Seeker, 1.0f);

		LastSeekerNotifyTimes.Remove(Seeker);
		LastSeekerProximityValues.Remove(Seeker);
	}
}

bool AGS_RTSController::IsSeekerInCameraView(AGS_Seeker* Seeker)
{
	if (!Seeker)
	{
		return false;
	}

	FVector2D ScreenLocation;
	if (!ProjectWorldLocationToScreen(Seeker->GetActorLocation(), ScreenLocation))
	{
		return false;
	}

	int32 SizeX, SizeY;
	GetViewportSize(SizeX, SizeY);

	if (SizeX <= 0 || SizeY <= 0)
	{
		return false;
	}

	return ScreenLocation.X >= 0 && ScreenLocation.X <= SizeX &&
		   ScreenLocation.Y >= 0 && ScreenLocation.Y <= SizeY;
}

void AGS_RTSController::NotifySeekerDetection(AGS_Seeker* Seeker, bool bIsDetected)
{
	if (!Seeker)
	{
		return;
	}

	// RPC 쿨다운 체크: 동일한 시커에 대해 너무 자주 호출되는 것을 방지
	float CurrentTime = GetWorld()->GetTimeSeconds();
	float* LastNotifyTime = LastSeekerNotifyTimes.Find(Seeker);

	if (LastNotifyTime && (CurrentTime - *LastNotifyTime) < DetectionRPCCooldown)
	{
		return;
	}

	// 마지막 호출 시간 갱신
	LastSeekerNotifyTimes.Add(Seeker, CurrentTime);

	// 서버 RPC 호출
	Server_NotifySeekerDetection(Seeker, bIsDetected);
}

void AGS_RTSController::Server_NotifySeekerDetection_Implementation(AGS_Seeker* Seeker, bool bIsDetected)
{
    if (!Seeker)
    {
        return;
    }

    // 클라이언트의 판단을 신뢰하여 상태 변경
    Seeker->OnDetectedByGuardian(bIsDetected);
}

float AGS_RTSController::CalculateSeekerDistanceFromScreenCenter(AGS_Seeker* Seeker)
{
	if (!Seeker)
	{
		return 1.0f; // 최대 거리 반환
	}

	FVector2D ScreenLocation;
	bool bProjected = ProjectWorldLocationToScreen(Seeker->GetActorLocation(), ScreenLocation);

	if (!bProjected)
	{
		return 1.0f;
	}

	int32 SizeX, SizeY;
	GetViewportSize(SizeX, SizeY);

	if (SizeX <= 0 || SizeY <= 0)
	{
		return 1.0f;
	}

	// 화면 중앙 좌표
	FVector2D Center(SizeX * 0.5f, SizeY * 0.5f);

	// 중앙으로부터의 거리 계산
	float Distance = FVector2D::Distance(Center, ScreenLocation);

	// 최대 거리 (중앙에서 코너까지)
	float MaxDistance = Center.Size();

	// 0.0 (중앙) ~ 1.0 (가장자리)로 정규화
	float NormalizedDistance = FMath::Clamp(Distance / MaxDistance, 0.0f, 1.0f);

	return NormalizedDistance;
}

void AGS_RTSController::Server_UpdateSeekerProximity_Implementation(AGS_Seeker* Seeker, float DistanceFromCenter)
{
	if (!Seeker)
	{
		return;
	}

	// 거리 값을 강도로 변환 (0.0 = 중앙 = 최대 강도, 1.0 = 가장자리 = 최소 강도)
	float Intensity = 1.0f - DistanceFromCenter;

	// 시커에 강도 설정
	Seeker->SetDetectionIntensity(Intensity);
}

void AGS_RTSController::Client_PlayBossBGM_Implementation(UAkAudioEvent* StartEvent, UAkAudioEvent* StopEvent)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UGS_AudioManager* AudioManager = GameInstance->GetSubsystem<UGS_AudioManager>())
		{
			AudioManager->StartBossSequenceLocal(this, StartEvent, StopEvent);
		}
	}
}

void AGS_RTSController::Client_StopBossBGM_Implementation()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UGS_AudioManager* AudioManager = GameInstance->GetSubsystem<UGS_AudioManager>())
		{
			AudioManager->EndBossSequenceLocal(this, 2.0f);
		}
	}
}

// ==========================================
// Guardian RTS 스킬 시스템
// ==========================================

void AGS_RTSController::OnRTSSkillKey(const FInputActionInstance& InputInstance, int32 SkillIndex)
{
	ActivateGuardianSkill(SkillIndex);
}

void AGS_RTSController::ActivateGuardianSkill(int32 SkillIndex)
{
	if (!RTSSkillComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RTSController] RTSSkillComp is null"));
		return;
	}

	// 스킬 발동 시도 (타겟팅 필요 시 EnterSkillTargetingMode 자동 호출됨)
	if (!RTSSkillComp->TryActivateSkill(SkillIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RTSController] Failed to activate skill %d"), SkillIndex);
	}
	
	// 타겟팅 모드 진입 가능성이 있으므로 커서 갱신
	UpdateCursorForEdgeScroll();
}

void AGS_RTSController::HandleSkillTargetingClick(const FVector& TargetLocation)
{
	if (!RTSSkillComp || !RTSSkillComp->IsInSkillTargetingMode())
	{
		return;
	}

	// 스킬 실행
	RTSSkillComp->ExecuteSkillAtLocation(TargetLocation);
	
	// 타겟팅 모드 종료 후 커서 갱신
	UpdateCursorForEdgeScroll();

	UE_LOG(LogTemp, Log, TEXT("[RTSController] Guardian skill executed at %s"), *TargetLocation.ToString());
}

bool AGS_RTSController::IsInGuardianSkillTargetingMode() const
{
	return RTSSkillComp && RTSSkillComp->IsInSkillTargetingMode();
}

void AGS_RTSController::CancelGuardianSkillTargeting()
{
	if (RTSSkillComp)
	{
		RTSSkillComp->ExitSkillTargetingMode();
		UpdateCursorForEdgeScroll();
	}
}

void AGS_RTSController::OnJumpToLastAttack(const FInputActionValue& Value)
{
	if (!AttackNotificationManager)
	{
		return;
	}

	FVector AttackLocation = AttackNotificationManager->GetLastAttackLocation();
	if (!AttackLocation.IsZero())
	{
		MoveCameraViaMinimap(AttackLocation);
	}
}
