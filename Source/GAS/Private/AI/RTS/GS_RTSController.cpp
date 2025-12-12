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
#include "ResourceSystem/Aether/GS_AetherExtractor.h"
#include "System/GameMode/GS_InGameGM.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "AI/RTS/RTS_Skill/GS_RTSSkillComponent.h"
#include "UI/RTS/GS_RTSSkillBarWidget.h"
#include "Components/CanvasPanelSlot.h"



AGS_RTSController::AGS_RTSController()
{
	bShowMouseCursor = true;

	CurrentCommand = ERTSCommand::None;
	KeyboardDir = FVector2D::ZeroVector;
	MouseEdgeDir = FVector2D::ZeroVector;
	CameraActor = nullptr;
	CameraSpeed = 2000.f;
	EdgeScreenRatio = 0.01f;
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
	if (!HasAuthority() && IsLocalController())
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
				return;
			}

			SkillBarWidget = CreateWidget<UGS_RTSSkillBarWidget>(this, RTSSkillBarWidgetClass);
			if (SkillBarWidget)
			{
				// 화면에 추가
				SkillBarWidget->AddToViewport(10); // Z-Order 10 (HUD 위에 표시)

				// 초기화
				SkillBarWidget->InitializeSkillBar(RTSSkillComp);

				UE_LOG(LogTemp, Log, TEXT("RTSSkillBarWidget created and initialized successfully"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to create RTSSkillBarWidget"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("RTSSkillComp is null, cannot create skill bar"));
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
	
	// 시커 감지 타이머 시작 (로컬 컨트롤러만)
	if (!HasAuthority() && IsLocalController())
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
	}
}

void AGS_RTSController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 마우스 엣지 감지
	MouseEdgeDir = GetMouseEdgeDirection();

	// 커서 준비된 경우에만 커서 업데이트
	if (bCursorReady && !bSeekerHovered)
	{
		UpdateCursorForEdgeScroll();
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

	for (AGS_Seeker* Seeker : TActorRange<AGS_Seeker>(GetWorld()))
	{
		if (IsValid(Seeker))
		{
			Seeker->HPTextWidgetComp->SetVisibility(true);
			Seeker->OnSeekerHover.AddDynamic(this, &AGS_RTSController::HandleSeekerHover);

			// 시커 파괴 시 정리를 위한 델리게이트 바인딩
			Seeker->OnDestroyed.AddDynamic(this, &AGS_RTSController::OnTrackedSeekerDestroyed);
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

FVector2D AGS_RTSController::GetMouseEdgeDirection() const
{
	// 뷰포트 유효성 검사
	if (!GetWorld() || !GetWorld()->GetGameViewport())
	{
		return FVector2D::ZeroVector;
	}

	float MouseX, MouseY;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return FVector2D::ZeroVector;
	}

	int32 ViewportX, ViewportY;
	GetViewportSize(ViewportX, ViewportY);

	// 뷰포트 크기가 유효한지 확인
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return FVector2D::ZeroVector;
	}

	float EdgeW = ViewportX * EdgeScreenRatio;
	float EdgeH = ViewportY * EdgeScreenRatio;

	FVector2D Dir = FVector2D::ZeroVector;
	if (MouseX <= EdgeW) // 좌·우 엣지 판정
	{
		Dir.X = -1.f;
	}
	else if (MouseX >= ViewportX - EdgeW)
	{
		Dir.X = 1.f;
	}
	if (MouseY <= EdgeH) // 위·아래 엣지 판정 
	{
		Dir.Y = 1.f;
	}
	else if (MouseY >= ViewportY - EdgeH)
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
		[this]() { bShowAttackCursor = false; },
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
	// 뷰포트 유효성 검사
	if (!GetWorld() || !GetWorld()->GetGameViewport())
	{
		return;
	}

	int32 ViewportX, ViewportY;
	GetViewportSize(ViewportX, ViewportY);

	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return;
	}

	FHitResult Hit;
	bool bHit = GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel1), true, Hit);
	if (!bHit || !Hit.GetActor())
	{
		return;
	}

	AGS_Monster* Monster = Cast<AGS_Monster>(Hit.GetActor());
	if (!Monster || !IsSelectable(Monster))
	{
		return;
	}
	
	ECharacterType MonsterType = Monster->GetCharacterType();
	TArray<AGS_Monster*> SameTypeUnits;
	
	// 월드에 있는 모든 몬스터를 순회 
	for (TActorIterator<AGS_Monster> It(GetWorld()); It; ++It)
	{
		AGS_Monster* CurrentMonster = *It;
		if (CurrentMonster->GetCharacterType() != MonsterType)
		{
			continue;
		}

		// 월드 좌표를 스크린 좌표로 투영
		FVector WorldLoc = CurrentMonster->GetActorLocation();
		FVector2D ScreenPos;
		bool bProjected = ProjectWorldLocationToScreen(WorldLoc, ScreenPos, true);

		// HUD 제외 카메라 뷰에서만 보이는 몬스터만 선택되도록 
		if (bProjected && ScreenPos.X >= 0.0f && ScreenPos.X <= ViewportX && ScreenPos.Y >= 0.0f && ScreenPos.Y <= ViewportY*0.77)
		{
			SameTypeUnits.Add(CurrentMonster);
		}
	}

	// 유닛으로부터의 거리를 기준으로 정렬
	SameTypeUnits.Sort([Monster](const AGS_Monster& A, const AGS_Monster& B)
	{
		return Monster->GetDistanceTo(&A) < Monster->GetDistanceTo(&B);
	});

	// 클릭된 유닛 포함하여 가까이에 있는 12개만 선택되도록 
	TArray<AGS_Monster*> Selection;
	for (int32 i = 0; i < SameTypeUnits.Num() && Selection.Num() < MaxSelectableUnits; ++i)
	{
		AGS_Monster* UnitToAdd = SameTypeUnits[i];
		if (!Selection.Contains(UnitToAdd)) 
		{
			Selection.Add(UnitToAdd);
		}
	}

	// 한 번에 선택하여 첫 번째 유닛만 소리 재생
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
		if (!IsSelectable(Monster))
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
	if (!Unit || !IsSelectable(Unit))
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
	UnitSelection.Empty();

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

		if (!IsSelectable(Unit))
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

	UnitSelection.Empty();
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
	if (!IsValid(Unit) || !IsSelectable(Unit))
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

		if (!IsValid(Unit) || !IsSelectable(Unit))
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

void AGS_RTSController::Server_RTSMove_Implementation(const FVector& Dest)
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
				BlackboardComp->SetValueAsEnum(AGS_AIController::CommandKey, static_cast<uint8>(ERTSCommand::Move));
				BlackboardComp->SetValueAsVector(AGS_AIController::MoveLocationKey, Dest);
				BlackboardComp->ClearValue(AGS_AIController::TargetActorKey);
				BlackboardComp->SetValueAsBool(AGS_AIController::TargetLockedKey, false);

				// 첫 번째 유닛만 이동 사운드 재생
				if (i == 0 && Unit->MonsterAudioComponent)
				{
					Unit->MonsterAudioComponent->PlayRTSCommandSound(ERTSCommandSoundType::Move);
				}
			}
		}
	}
}

void AGS_RTSController::Server_RTSAttackMove_Implementation(const FVector& Dest)
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
				BlackboardComp->SetValueAsEnum(AGS_AIController::CommandKey, static_cast<uint8>(ERTSCommand::Attack));
				BlackboardComp->SetValueAsVector(AGS_AIController::MoveLocationKey, Dest);
				BlackboardComp->SetValueAsBool(AGS_AIController::TargetLockedKey, false);

				// 첫 번째 유닛만 공격 사운드 재생
				if (i == 0 && Unit->MonsterAudioComponent)
				{
					Unit->MonsterAudioComponent->PlayRTSCommandSound(ERTSCommandSoundType::Attack);
				}
			}
		}
	}
}

void AGS_RTSController::Server_RTSAttack_Implementation(AGS_Character* TargetActor)
{
	// 타겟 유효성 검증
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server_RTSAttack] Invalid TargetActor"));
		return;
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

void AGS_RTSController::GatherCommandableUnits(TArray<AGS_Monster*>& Out) const
{
	for (AGS_Monster* Unit : UnitSelection)
	{
		if (IsValid(Unit) && Unit->IsCommandable())
		{
			Out.Add(Unit);
		}
	}
}

bool AGS_RTSController::IsSelectable(AGS_Monster* Monster) const
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

}

void AGS_RTSController::UpdateSeekerDetection()
{
	if (!CameraActor)
	{
		return;
	}

	// 현재 카메라 시야 안에 있는 시커들 찾기
	TArray<AGS_Seeker*> CurrentVisibleSeekers;

	for (TActorIterator<AGS_Seeker> It(GetWorld()); It; ++It)
	{
		AGS_Seeker* Seeker = *It;
		if (IsValid(Seeker) && !Seeker->IsDead())
		{
			if (IsSeekerInCameraView(Seeker))
			{
				CurrentVisibleSeekers.Add(Seeker);

				// 화면 중앙과의 거리 계산 (0.0 = 중앙, 1.0 = 가장자리)
				float DistanceFromCenter = CalculateSeekerDistanceFromScreenCenter(Seeker);

				// 서버에 거리 정보 전송
				Server_UpdateSeekerProximity(Seeker, DistanceFromCenter);
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
	}
}

bool AGS_RTSController::IsSeekerInCameraView(AGS_Seeker* Seeker)
{
	if (!CameraActor || !Seeker)
	{
		return false;
	}

	// 카메라 시야 경계 계산
	FBox2D ViewBounds = CameraActor->GetSimpleViewBounds();

	// 시커의 위치를 2D로 변환
	FVector SeekerLocation = Seeker->GetActorLocation();
	FVector2D Seeker2DLocation(SeekerLocation.X, SeekerLocation.Y);

	// 시커가 카메라 시야 안에 있는지 확인
	return ViewBounds.IsInside(Seeker2DLocation);
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

    // 서버 검증: 기본적인 거리 기반 검증
    // 서버는 카메라 정보가 없으므로 세밀한 시야각 검증 불가. 대신 최대 거리 내에 있는지만 확인
    if (CameraActor)
    {
        const float MaxDetectionDistance = 5000.0f; // 최대 감지 거리
        float DistanceToSeeker = FVector::Dist(CameraActor->GetActorLocation(), Seeker->GetActorLocation());

        if (bIsDetected && DistanceToSeeker > MaxDetectionDistance)
        {
            return; // 너무 먼 거리의 감지 요청은 무시
        }
    }

    // 클라이언트의 판단을 신뢰하여 상태 변경
    Seeker->OnDetectedByGuardian(bIsDetected);
}

float AGS_RTSController::CalculateSeekerDistanceFromScreenCenter(AGS_Seeker* Seeker)
{
	if (!CameraActor || !Seeker)
	{
		return 1.0f; // 최대 거리 반환
	}

	// 화면 경계 가져오기
	FBox2D ViewBounds = CameraActor->GetSimpleViewBounds();
	FVector2D Center = ViewBounds.GetCenter();

	// 시커의 2D 위치
	FVector SeekerLocation = Seeker->GetActorLocation();
	FVector2D Seeker2D(SeekerLocation.X, SeekerLocation.Y);

	// 화면 크기 계산
	FVector2D ViewSize = ViewBounds.GetSize();
	float MaxDistance = ViewSize.Size() * 0.5f; // 대각선 거리의 절반

	// 중앙으로부터의 거리 계산
	float Distance = FVector2D::Distance(Center, Seeker2D);

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
}

void AGS_RTSController::HandleSkillTargetingClick(const FVector& TargetLocation)
{
	if (!RTSSkillComp || !RTSSkillComp->IsInSkillTargetingMode())
	{
		return;
	}

	// 스킬 실행
	RTSSkillComp->ExecuteSkillAtLocation(TargetLocation);

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
	}
}
