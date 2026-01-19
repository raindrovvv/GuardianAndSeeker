// Copyright 2024 Greed Fennec Studio. All Rights Reserved.

#include "Character/GS_TpsController.h"
#include "Character/Component/Seeker/GS_MarkerPlacementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Controller.h"
#include "Character/GS_Character.h"
#include "Character/Player/Seeker/GS_Seeker.h"
#include "Character/Player/GS_Player.h"
#include "Character/Player/Seeker/GS_Merci.h"
#include "Character/Interface/GS_AttackInterface.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "System/GS_PlayerState.h"
#include "UI/Character/GS_HPBoardWidget.h"
#include "System/GS_GameInstance.h"
#include "UI/Character/GS_BossHP.h"
#include "UI/Character/GS_DrakharFeverGauge.h"
#include "UI/Character/GS_FeverGaugeBoard.h"
#include "UI/Character/GS_CrossHairImage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Character/Component/GS_DebuffComp.h"
#include "System/GameMode/GS_InGameGM.h"
#include "EngineUtils.h"
#include "UI/Character/GS_ReviveIndicatorWidget.h"
#include "Interface/GS_InteractableInterface.h"
#include "UI/Interaction/GS_InteractionWidget.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "UI/Character/GS_Timer.h"
#include "Components/PanelWidget.h"


AGS_TpsController::AGS_TpsController()
{
	InputMappingContext = nullptr;
	MoveAction = nullptr;
	LookAction = nullptr;
	WalkToggleAction = nullptr;

	bReplicates = true;
	bIsAutoMoving = false;
}

void AGS_TpsController::Move(const FInputActionValue& InputValue)
{
	const FVector2D InputAxisVector = InputValue.Get<FVector2D>();
	LastRotatorInMoving = GetControlRotation();

	// Throttling logic: only send input update to server if the value changed significantly or interval expired
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const bool bValueSignificantChange =
		FVector2D::Distance(InputAxisVector, LastSentMoveInputValue) > MoveInputSendThreshold;
	const bool bIntervalPassed = (CurrentTime - LastMoveInputSentTime) > MoveInputMinSendInterval;
	const bool bIsStopping = InputAxisVector.IsNearlyZero() && !LastSentMoveInputValue.IsNearlyZero();

	if (bValueSignificantChange || bIntervalPassed || bIsStopping)
	{
		Server_CacheMoveInputValue(InputAxisVector);
		LastSentMoveInputValue = InputAxisVector;
		LastMoveInputSentTime = CurrentTime;
	}

	if (AGS_Character* ControlledCharacter = Cast<AGS_Character>(GetPawn()))
	{
		// Handle turns during automated forward movement
		if (bIsAutoMoving)
		{
			if (!FMath::IsNearlyZero(InputAxisVector.Y))
			{
				const float TurnSpeed = 0.7f;
				if (IsLocalController())
				{
					FRotator ControlRot = GetControlRotation();
					ControlRot.Yaw += InputAxisVector.Y * TurnSpeed;
					SetControlRotation(ControlRot);
				}
			}
			return;
		}

		// Standard character move logic
		const FRotator YawRotation(0.f, LastRotatorInMoving.Yaw, 0.0f);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		if (ControlValues.bCanMoveForward)
		{
			ControlledCharacter->AddMovementInput(ForwardDirection, InputAxisVector.X);
		}
		if (ControlValues.bCanMoveRight)
		{
			ControlledCharacter->AddMovementInput(RightDirection, InputAxisVector.Y);
		}
	}
}

void AGS_TpsController::Look(const FInputActionValue& InputValue)
{
	if (bIsAutoMoving)
	{
		// Disable look control during automated movement
		return;
	}

	const FVector2D InputAxisVector = InputValue.Get<FVector2D>();
	if (AGS_Character* ControlledCharacter = Cast<AGS_Character>(GetPawn()))
	{
		if (GameInstance)
		{
			const float SensitivityMultiplier = GameInstance->GetMouseSensitivity();
			if (ControlValues.bCanLookRight)
			{
				ControlledCharacter->AddControllerYawInput(InputAxisVector.X * SensitivityMultiplier);
			}

			if (ControlValues.bCanLookUp)
			{
				FRotator CurrentRot = GetControlRotation();

				// Vertical pitch clamping - Prevents camera flipping or gimballock
				static constexpr float PitchMin = -80.f;
				static constexpr float PitchMax = 50.f;

				float NewPitch = CurrentRot.Pitch + (-InputAxisVector.Y * SensitivityMultiplier);
				NewPitch = FMath::ClampAngle(NewPitch, PitchMin, PitchMax);

				CurrentRot.Pitch = NewPitch;
				SetControlRotation(CurrentRot);
			}
		}
	}
}

void AGS_TpsController::WalkToggle(const FInputActionValue& InputValue)
{
	if (AGS_Seeker* SeekerPawn = Cast<AGS_Seeker>(GetPawn()))
	{
		if (SeekerPawn->CanChangeSeekerGait)
		{
			const EGait CurGait = SeekerPawn->GetSeekerGait();
			const EGait NewGait = (CurGait == EGait::Walk) ? EGait::Run : EGait::Walk;
			SeekerPawn->Server_SetSeekerGait(NewGait);
		}
	}
}

void AGS_TpsController::PlaceMarker(const FInputActionValue& InputValue)
{
	if (AGS_Seeker* ControlledPawn = Cast<AGS_Seeker>(GetPawn()))
	{
		if (ControlledPawn->MarkerPlacementComponent)
		{
			ControlledPawn->MarkerPlacementComponent->TryPlaceMarker();
		}
	}
}

FControlValue AGS_TpsController::GetControlValue() const
{
	return ControlValues;
}

void AGS_TpsController::PageUp(const FInputActionValue& InputValue)
{
	if (IsLocalController())
	{
		ServerRPCSpectatePlayer(1);
	}
}

void AGS_TpsController::PageDown(const FInputActionValue& InputValue)
{
	if (IsLocalController())
	{
		ServerRPCSpectatePlayer(-1);
	}
}

void AGS_TpsController::SetMoveControlValue(bool CanMoveRight, bool CanMoveForward)
{
	// Restrict movement changes if the character is currently stunned
	if (AGS_Character* ControlledCharacter = Cast<AGS_Character>(GetPawn()))
	{
		if ((CanMoveRight || CanMoveForward) && ControlledCharacter->GetDebuffComp()->IsDebuffActive(EDebuffType::Stun))
		{
			return;
		}
	}

	ControlValues.bCanMoveForward = CanMoveForward;
	ControlValues.bCanMoveRight = CanMoveRight;
}

FControlValue AGS_TpsController::GetMoveControlValue()
{
	return ControlValues;
}

void AGS_TpsController::SetLookControlValue(bool CanLookRight, bool CanLookUp)
{
	ControlValues.bCanLookUp = CanLookUp;
	ControlValues.bCanLookRight = CanLookRight;
}

void AGS_TpsController::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGS_TpsController, ControlValues);
	DOREPLIFETIME_CONDITION(AGS_TpsController, MoveInputValue, COND_OwnerOnly);
	DOREPLIFETIME(AGS_TpsController, bIsAutoMoving);
	DOREPLIFETIME(AGS_TpsController, bIsHoldingReviveKey);
}

float AGS_TpsController::GetCurrentMouseSensitivity() const
{
	if (GameInstance)
	{
		return GameInstance->GetMouseSensitivity();
	}
	return 1.0f;
}

void AGS_TpsController::InitControllerPerWorld()
{
	SetInputMode(FInputModeGameOnly());

	if (!HasAuthority() && IsLocalController())
	{
		if (!InputMappingContext)
		{
			UE_LOG(LogTemp, Error, TEXT("[Controller] InputMappingContext is NULL! Check Seeker PC Blueprint."));
			return;
		}

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(InputMappingContext, 0);

			// Setup audio listener with a slight delay to ensure pawn validity
			FTimerHandle AudioTimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
				AudioTimerHandle, this, &AGS_TpsController::SetupPlayerAudioListener, 0.1f, false);
		}
	}
}

void AGS_TpsController::ServerRPCSpectatePlayer_Implementation(int32 Step)
{
	UWorld* World = GetWorld();
	if (!World || !World->GetGameState())
	{
		return;
	}

	// 1. 자신의 폰 해제 (최초 관전 진입 시 1회 수행)
	if (GetPawn())
	{
		APawn* DeadPawn = GetPawn();
		UnPossess();
		if (DeadPawn)
		{
			DeadPawn->SetLifeSpan(2.0f);
		}

		// 관전자 모드 진입 알림 및 UI 처리
		ClientRPC_OnSpectatorModeStarted();
	}

	// 2. 생존한 시커 목록 필터링 (관전 가능 대상)
	TArray<AGS_PlayerState*> AliveSeekers;
	for (APlayerState* PS : World->GetGameState()->PlayerArray)
	{
		AGS_PlayerState* GS_PS = Cast<AGS_PlayerState>(PS);
		if (GS_PS && GS_PS->bIsAlive && GS_PS->CurrentPlayerRole == EPlayerRole::PR_Seeker)
		{
			AliveSeekers.Add(GS_PS);
		}
	}

	if (AliveSeekers.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Spectator] No alive seekers to spectate."));
		return;
	}

	// 3. 관전 인덱스 계산 (사이클링)
	if (SpectatorIndex == -1)
	{
		SpectatorIndex = 0;
	}
	else
	{
		SpectatorIndex = (SpectatorIndex + Step) % AliveSeekers.Num();
		if (SpectatorIndex < 0)
		{
			SpectatorIndex += AliveSeekers.Num();
		}
	}

	// 4. 관전 대상 설정
	AGS_PlayerState* TargetPS = AliveSeekers[SpectatorIndex];
	if (IsValid(TargetPS))
	{
		APawn* TargetPawn = TargetPS->GetPawn();
		if (IsValid(TargetPawn))
		{
			// 부드러운 카메라 전환 (0.5초 블렌딩)
			SetViewTargetWithBlend(TargetPawn, 0.5f, EViewTargetBlendFunction::VTBlend_Linear, 0.0f, true);
			UE_LOG(LogTemp, Log, TEXT("[Spectator] Now spectating: %s"), *TargetPS->GetPlayerName());
		}
	}
}

void AGS_TpsController::ClientRPC_OnSpectatorModeStarted_Implementation()
{
	// TODO: 관전자 전용 전용 UI로 교체하는 로직 구현 필요 (예: 관전 대상 이름 표시 등)

	if (IsValid(PlayerWidgetInstance))
	{
		UWidgetTree* Tree = PlayerWidgetInstance->WidgetTree;
		if (Tree)
		{
			// 1. 모든 위젯을 일단 숨김.
			Tree->ForEachWidget(
				[&](UWidget* Widget)
				{
					if (Widget)
					{
						Widget->SetVisibility(ESlateVisibility::Collapsed);
					}
				});

			// 2. 타이머 위젯(UGS_Timer)을 찾아 그 부모 계층까지 다시 보이도록 설정.
			TArray<UWidget*> AllWidgets;
			Tree->GetAllWidgets(AllWidgets);
			for (UWidget* Widget : AllWidgets)
			{
				if (Widget && Widget->IsA<UGS_Timer>())
				{
					UWidget* Temp = Widget;
					while (Temp)
					{
						// 부모들은 SelfHitTestInvisible로 설정하여 자신은 입력을 안 받지만 자식은 보이게 함
						Temp->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
						Temp = Temp->GetParent();
					}
					// 타이머 본체는 Visible
					Widget->SetVisibility(ESlateVisibility::Visible);
				}
			}
		}
	}
}

void AGS_TpsController::Server_CacheMoveInputValue_Implementation(FVector2D InputValue)
{
	MoveInputValue = InputValue;
}

void AGS_TpsController::InitializePlayerHUD()
{
	AGS_Character* ControlledCharacter = Cast<AGS_Character>(GetPawn());
	if (!IsValid(ControlledCharacter))
	{
		return;
	}

	TSubclassOf<UUserWidget> WidgetClass = PlayerWidgetClasses[ControlledCharacter->GetCharacterType()];
	if (!IsValid(WidgetClass))
	{
		return;
	}

	// Reuse existing widget if classes match, otherwise recreate
	if (PlayerWidgetInstance)
	{
		if (PlayerWidgetInstance->GetClass() != WidgetClass)
		{
			PlayerWidgetInstance->RemoveFromParent();
			PlayerWidgetInstance = CreateWidget<UUserWidget>(this, WidgetClass);
		}
	}
	else
	{
		PlayerWidgetInstance = CreateWidget<UUserWidget>(this, WidgetClass);
	}

	if (!IsValid(PlayerWidgetInstance))
	{
		return;
	}

	// Initialize specific HUD components
	UGS_HPBoardWidget* HPBoard = Cast<UGS_HPBoardWidget>(PlayerWidgetInstance->GetWidgetFromName(TEXT("WBP_HPBoard")));
	UGS_BossHP* BossHUD = Cast<UGS_BossHP>(PlayerWidgetInstance->GetWidgetFromName(TEXT("WBP_BossHPBoard")));
	UGS_FeverGaugeBoard* FeverHUD =
		Cast<UGS_FeverGaugeBoard>(PlayerWidgetInstance->GetWidgetFromName(TEXT("WBP_FeverBoard")));

	if (BossHUD)
	{
		BossHUD->SetOwningActor(ControlledCharacter);
		BossHUD->InitGuardianHPWidget();
	}

	if (FeverHUD)
	{
		FeverHUD->InitDrakharFeverWidget();
	}

	if (HPBoard)
	{
		HPBoard->InitBoardWidget();
	}

	if (!PlayerWidgetInstance->IsInViewport())
	{
		PlayerWidgetInstance->AddToViewport(0);
	}
	else
	{
		PlayerWidgetInstance->SetVisibility(ESlateVisibility::Visible);
	}

	CrosshairWidget = Cast<UGS_CrossHairImage>(PlayerWidgetInstance->GetWidgetFromName(TEXT("WBP_CrossHairImage")));
	if (CrosshairWidget)
	{
		if (AGS_Merci* MerciPawn = Cast<AGS_Merci>(ControlledCharacter))
		{
			MerciPawn->SetCrosshairWidget(CrosshairWidget);
		}
	}
}

void AGS_TpsController::StartAutoMoveForward()
{
	bIsAutoMoving = true;
	Client_StartAutoMoveForward(); // 돌진 시작
}

void AGS_TpsController::StopAutoMoveForward()
{
	bIsAutoMoving = false;
	Client_StopAutoMoveForward();
}

void AGS_TpsController::Client_StartAutoMoveForward_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}

	bIsAutoMoving = true;
	SaveOriginalCameraSettings();
	ApplyChargeCameraSettings(true);
	SnapCameraToCharacterYaw();
	GetWorld()->GetTimerManager().SetTimer(AutoMoveTickHandle, this, &AGS_TpsController::AutoMoveTick, 0.01f, true);
}

void AGS_TpsController::Client_StopAutoMoveForward_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}

	bIsAutoMoving = false;
	GetWorld()->GetTimerManager().ClearTimer(AutoMoveTickHandle);
	RestoreOriginalCameraSettings();
}

void AGS_TpsController::SnapCameraToCharacterYaw()
{
	if (!IsLocalController())
	{
		return;
	}

	if (APawn* MyPawn = GetPawn())
	{
		// 카메라를 캐릭터 뒤쪽으로 정렬
		FRotator CharacterRotation = MyPawn->GetActorRotation();
		SetControlRotation(CharacterRotation);
	}
}

void AGS_TpsController::SetIsAutoMoving(bool InIsAutoMoving)
{
	if (HasAuthority())
	{
		bIsAutoMoving = InIsAutoMoving;
	}
}

void AGS_TpsController::AutoMoveTick()
{
	if (!IsValid(this) || !IsLocalController() || !bIsAutoMoving)
	{
		return;
	}

	AGS_Character* ControlledCharacter = Cast<AGS_Character>(GetPawn());
	if (!IsValid(ControlledCharacter) || !ControlledCharacter->GetCharacterMovement())
	{
		StopAutoMoveForward();
		return;
	}

	const FRotator YawRotation(0.f, ControlledCharacter->GetActorRotation().Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	ControlledCharacter->AddMovementInput(ForwardDirection, 10.0f);
}

void AGS_TpsController::SaveOriginalCameraSettings()
{
	if (AGS_Character* ControlledCharacter = Cast<AGS_Character>(GetPawn()))
	{
		bOriginalUseControllerRotationYaw = ControlledCharacter->bUseControllerRotationYaw;

		if (UCharacterMovementComponent* Movement = ControlledCharacter->GetCharacterMovement())
		{
			bOriginalOrientRotationToMovement = Movement->bOrientRotationToMovement;
		}

		if (USpringArmComponent* SpringArm = ControlledCharacter->FindComponentByClass<USpringArmComponent>())
		{
			bOriginalUsePawnControlRotation = SpringArm->bUsePawnControlRotation;
			bOriginalEnableCameraLag = SpringArm->bEnableCameraLag;
			bOriginalEnableCameraRotationLag = SpringArm->bEnableCameraRotationLag;
			bOriginalInheritYaw = SpringArm->bInheritYaw;
		}
	}
}

void AGS_TpsController::RestoreOriginalCameraSettings()
{
	if (AGS_Character* ControlledCharacter = Cast<AGS_Character>(GetPawn()))
	{
		ControlledCharacter->bUseControllerRotationYaw = bOriginalUseControllerRotationYaw;

		if (UCharacterMovementComponent* Movement = ControlledCharacter->GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = bOriginalOrientRotationToMovement;
		}

		if (USpringArmComponent* SpringArm = ControlledCharacter->FindComponentByClass<USpringArmComponent>())
		{
			SpringArm->bUsePawnControlRotation = bOriginalUsePawnControlRotation;
			SpringArm->bEnableCameraLag = bOriginalEnableCameraLag;
			SpringArm->bEnableCameraRotationLag = bOriginalEnableCameraRotationLag;
			SpringArm->bInheritYaw = bOriginalInheritYaw;
		}
	}

	SetLookControlValue(true, true);
}

void AGS_TpsController::ApplyChargeCameraSettings(bool bCharging)
{
	if (!IsLocalController())
		return;

	if (AGS_Character* ControlledCharacter = Cast<AGS_Character>(GetPawn()))
	{
		ControlledCharacter->bUseControllerRotationYaw = true;

		if (UCharacterMovementComponent* Movement = ControlledCharacter->GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = false;
		}

		if (USpringArmComponent* SpringArm = ControlledCharacter->FindComponentByClass<USpringArmComponent>())
		{
			SpringArm->bUsePawnControlRotation = false;
			SpringArm->bEnableCameraLag = false;
			SpringArm->bEnableCameraRotationLag = false;
			SpringArm->bInheritYaw = true;
		}
	}

	SetLookControlValue(false, false);
}

void AGS_TpsController::Client_DrawAimAssistDebug_Implementation(const FVector& Start,
																 const FVector& End,
																 const FVector& TargetLocation,
																 float Duration)
{
	if (UWorld* World = GetWorld())
	{
		DrawDebugLine(World, Start, End, FColor::Yellow, false, Duration, 0, 1.5f);
		DrawDebugSphere(World, TargetLocation, 24.f, 12, FColor::Red, false, Duration);
	}
}

void AGS_TpsController::BeginPlay()
{
	Super::BeginPlay();

	GameInstance = Cast<UGS_GameInstance>(GetGameInstance());
	InitControllerPerWorld();

	if (IsLocalController())
	{
		// Initialize Revive HUD
		if (ReviveIndicatorWidgetClass)
		{
			ReviveIndicatorWidget = CreateWidget<UGS_ReviveIndicatorWidget>(this, ReviveIndicatorWidgetClass);
			if (ReviveIndicatorWidget)
				ReviveIndicatorWidget->AddToViewport();
		}

		// Initialize Interaction HUD
		if (InteractionWidgetClass)
		{
			InteractionWidget = CreateWidget<UGS_InteractionWidget>(this, InteractionWidgetClass);
			if (InteractionWidget)
				InteractionWidget->AddToViewport();
		}

		// Setup monitoring timers
		GetWorldTimerManager().SetTimer(
			ReviveIndicatorTimerHandle, this, &AGS_TpsController::UpdateReviveIndicatorVisibility, 0.2f, true);
		GetWorldTimerManager().SetTimer(
			InteractableUpdateTimerHandle, this, &AGS_TpsController::UpdateNearbyInteractable, 0.1f, true);
	}
}

UUserWidget* AGS_TpsController::GetPlayerWidget()
{
	if (!IsValid(PlayerWidgetInstance))
	{
		return nullptr;
	}
	return PlayerWidgetInstance;
}

void AGS_TpsController::SetupPlayerAudioListener()
{
	if (!IsLocalController())
	{
		return;
	}

	if (AGS_Player* ControlledPlayer = Cast<AGS_Player>(GetPawn()))
	{
		ControlledPlayer->SetupLocalAudioListener();
		UE_LOG(LogAudio,
			   Log,
			   TEXT("Audio listener setup initiated from controller for: %s"),
			   *ControlledPlayer->GetName());
	}
}

void AGS_TpsController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent);
	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGS_TpsController::Move);
	}
	if (LookAction)
	{
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGS_TpsController::Look);
	}
	if (PageUpAction)
	{
		EnhancedInputComponent->BindAction(PageUpAction, ETriggerEvent::Started, this, &AGS_TpsController::PageUp);
	}
	if (PageDownAction)
	{
		EnhancedInputComponent->BindAction(PageDownAction, ETriggerEvent::Started, this, &AGS_TpsController::PageDown);
	}
	if (WalkToggleAction)
	{
		EnhancedInputComponent->BindAction(
			WalkToggleAction, ETriggerEvent::Started, this, &AGS_TpsController::WalkToggle);
	}
	if (PlaceMarkerAction)
	{
		EnhancedInputComponent->BindAction(
			PlaceMarkerAction, ETriggerEvent::Started, this, &AGS_TpsController::PlaceMarker);
	}

	// 빈사 플레이어 구조 (E키)
	if (ReviveAction)
	{
		EnhancedInputComponent->BindAction(
			ReviveAction, ETriggerEvent::Started, this, &AGS_TpsController::TryStartRevive);
		EnhancedInputComponent->BindAction(
			ReviveAction, ETriggerEvent::Completed, this, &AGS_TpsController::StopRevive);
	}
	else
	{
		UE_LOG(LogTemp,
			   Error,
			   TEXT("[Revive] ReviveAction is NULL! Please assign IA_ReviveAction in BP_PlayerController!"));
	}
}

void AGS_TpsController::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	InitControllerPerWorld();
}

void AGS_TpsController::BeginPlayingState()
{
	Super::BeginPlayingState();

	UE_LOG(LogTemp, Warning, TEXT("AGS_TpsController (%s) --- BeginPlayingState CALLED ---"), *GetNameSafe(this));
	if (IsLocalController())
	{
		CreateMainHUD();
	}
}

void AGS_TpsController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoMoveTickHandle);
		GetWorld()->GetTimerManager().ClearTimer(ReviveIndicatorTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(InteractableUpdateTimerHandle);
	}
}

void AGS_TpsController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsLocalController())
		return;

	UpdateInteractionProgress(DeltaTime);

	// Cleanup revive state if target recovers suddenly
	if (bIsReviving && ReviveTarget.IsValid())
	{
		if (!ReviveTarget->IsInDyingState())
		{
			bIsReviving = false;
			bIsHoldingReviveKey = false;
			ReviveTarget = nullptr;

			if (ReviveIndicatorWidget)
				ReviveIndicatorWidget->StopRevive();
			return;
		}

		// Distance check validation
		if (AGS_Seeker* MySeeker = Cast<AGS_Seeker>(GetPawn()))
		{
			if (FVector::Dist(MySeeker->GetActorLocation(), ReviveTarget->GetActorLocation()) > ReviveDistance)
			{
				StopRevive(FInputActionValue());
			}
		}
	}
}

// ==========================================
// 빈사 플레이어 구조 시스템 구현
// ==========================================

void AGS_TpsController::UpdateReviveIndicatorVisibility()
{
	AGS_Seeker* MySeeker = Cast<AGS_Seeker>(GetPawn());
	if (!IsValid(MySeeker) || bIsReviving)
		return;

	if (MySeeker->IsInDyingState())
	{
		if (bNearbyDyingSeekerDetected && ReviveIndicatorWidget)
		{
			bNearbyDyingSeekerDetected = false;
			ReviveIndicatorWidget->HideNearbyIndicator();
		}
		return;
	}

	AGS_Seeker* NearbyDyingSeeker = FindNearbyDyingSeeker();
	if (IsValid(NearbyDyingSeeker))
	{
		if (!bNearbyDyingSeekerDetected || LastDetectedDyingSeeker != NearbyDyingSeeker)
		{
			bNearbyDyingSeekerDetected = true;
			LastDetectedDyingSeeker = NearbyDyingSeeker;
			if (ReviveIndicatorWidget)
				ReviveIndicatorWidget->ShowNearbyIndicator(NearbyDyingSeeker);
		}
	}
	else if (bNearbyDyingSeekerDetected)
	{
		bNearbyDyingSeekerDetected = false;
		LastDetectedDyingSeeker = nullptr;
		if (ReviveIndicatorWidget)
			ReviveIndicatorWidget->HideNearbyIndicator();
	}
}

void AGS_TpsController::TryStartRevive(const FInputActionValue& InputValue)
{
	if (!IsLocalController())
		return;

	AGS_Seeker* MySeeker = Cast<AGS_Seeker>(GetPawn());
	if (!IsValid(MySeeker) || MySeeker->IsInDyingState())
		return;

	// Teammate revival takes priority over object interaction
	AGS_Seeker* DyingSeeker = FindNearbyDyingSeeker();
	if (IsValid(DyingSeeker))
	{
		bIsReviving = true;
		bIsHoldingReviveKey = true;
		ReviveTarget = DyingSeeker;

		if (ReviveIndicatorWidget)
			ReviveIndicatorWidget->StartRevive(DyingSeeker);

		Server_SetHoldingReviveKey(true);
		Server_RequestRevive(DyingSeeker);
		return;
	}

	// Trigger object interaction if no revivable targets are found
	if (CachedInteractable.IsValid())
	{
		AActor* TargetActor = CachedInteractable.Get();
		if (TargetActor->Implements<UGS_InteractableInterface>())
		{
			if (IGS_InteractableInterface::Execute_CanInteract(TargetActor, MySeeker))
			{
				StartInteraction(TargetActor);
			}
		}
	}
}

void AGS_TpsController::StopRevive(const FInputActionValue& InputValue)
{
	if (bIsReviving)
	{
		bIsReviving = false;
		bIsHoldingReviveKey = false;
		if (ReviveIndicatorWidget)
			ReviveIndicatorWidget->StopReviveProgress();
		if (ReviveTarget.IsValid())
			Server_CancelRevive();
		Server_SetHoldingReviveKey(false);
		ReviveTarget = nullptr;
	}
	else if (bIsInteracting)
	{
		CancelInteraction();
	}
}

AGS_Seeker* AGS_TpsController::FindNearbyDyingSeeker() const
{
	AGS_Seeker* MySeeker = Cast<AGS_Seeker>(GetPawn());
	if (!IsValid(MySeeker))
		return nullptr;

	UWorld* World = GetWorld();
	if (!World)
		return nullptr;

	const FVector MyLocation = MySeeker->GetActorLocation();
	AGS_Seeker* ClosestDyingSeeker = nullptr;
	float ClosestDistance = ReviveDistance;

	if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
	{
		const TArray<TWeakObjectPtr<AGS_Seeker>>& SeekerPtrs = Registry->GetSeekers();
		for (const TWeakObjectPtr<AGS_Seeker>& SeekerPtr : SeekerPtrs)
		{
			AGS_Seeker* OtherSeeker = SeekerPtr.Get();
			if (!IsValid(OtherSeeker) || OtherSeeker == MySeeker || !OtherSeeker->IsInDyingState())
				continue;

			const float Distance = FVector::Dist(MyLocation, OtherSeeker->GetActorLocation());
			if (Distance < ClosestDistance)
			{
				FHitResult LOSHit;
				FCollisionQueryParams Params;
				Params.AddIgnoredActor(MySeeker);
				Params.AddIgnoredActor(OtherSeeker);

				// Ensure direct line of sight to prevent reviving through walls
				if (!World->LineTraceSingleByChannel(
						LOSHit, MyLocation, OtherSeeker->GetActorLocation(), ECC_Visibility, Params))
				{
					ClosestDistance = Distance;
					ClosestDyingSeeker = OtherSeeker;
				}
			}
		}
	}
	return ClosestDyingSeeker;
}

void AGS_TpsController::Server_RequestRevive_Implementation(AGS_Seeker* Target)
{
	AGS_Seeker* MySeeker = Cast<AGS_Seeker>(GetPawn());
	if (!IsValid(Target) || !IsValid(MySeeker))
		return;

	// Server-side validation of range and line of sight
	if (FVector::Dist(MySeeker->GetActorLocation(), Target->GetActorLocation()) > ReviveDistance)
		return;

	FHitResult LOSHit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(MySeeker);
	Params.AddIgnoredActor(Target);

	if (GetWorld()->LineTraceSingleByChannel(
			LOSHit, MySeeker->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Params))
		return;

	ReviveTarget = Target;
	bIsHoldingReviveKey = true;
	Target->Server_StartRevive(MySeeker);
}

void AGS_TpsController::Server_CancelRevive_Implementation()
{
	bIsHoldingReviveKey = false;
	if (ReviveTarget.IsValid())
		ReviveTarget->Server_CancelRevive();
	ReviveTarget = nullptr;
}

void AGS_TpsController::Server_SetHoldingReviveKey_Implementation(bool bIsHolding)
{
	bIsHoldingReviveKey = bIsHolding;
}

// ============================================
// 일반 상호작용 시스템 구현
// ============================================

float AGS_TpsController::GetInteractionProgress() const
{
	if (!bIsInteracting || CurrentInteractionDuration <= 0.0f)
		return 0.0f;
	const float Elapsed = GetWorld()->GetTimeSeconds() - InteractionStartTime;
	return FMath::Clamp(Elapsed / CurrentInteractionDuration, 0.0f, 1.0f);
}

void AGS_TpsController::UpdateNearbyInteractable()
{
	AGS_Seeker* MySeeker = Cast<AGS_Seeker>(GetPawn());
	if (!IsValid(MySeeker))
	{
		CachedInteractable.Reset();
		return;
	}

	TArray<AActor*> OverlappingActors;
	MySeeker->GetOverlappingActors(OverlappingActors);

	AActor* BestInteractable = nullptr;
	int32 HighestPriority = INT_MIN;

	for (AActor* Actor : OverlappingActors)
	{
		if (!Actor || !Actor->Implements<UGS_InteractableInterface>() ||
			!IGS_InteractableInterface::Execute_CanInteract(Actor, MySeeker))
			continue;

		int32 Priority = IGS_InteractableInterface::Execute_GetInteractionPriority(Actor);
		if (Priority > HighestPriority)
		{
			HighestPriority = Priority;
			BestInteractable = Actor;
		}
	}

	if (InteractionWidget && (BestInteractable != CachedInteractable.Get() || bIsInteracting))
	{
		if (BestInteractable && !bIsInteracting)
		{
			const FText ActionText = IGS_InteractableInterface::Execute_GetInteractionText(BestInteractable);
			InteractionWidget->ShowNearbyIndicator(BestInteractable, ActionText);
		}
		else
		{
			InteractionWidget->HideNearbyIndicator();
		}
	}
	CachedInteractable = BestInteractable;
}

void AGS_TpsController::StartInteraction(AActor* Target)
{
	AGS_Seeker* MySeeker = Cast<AGS_Seeker>(GetPawn());
	if (!Target || !Target->Implements<UGS_InteractableInterface>() || !IsValid(MySeeker))
		return;

	bIsInteracting = true;
	CurrentInteractTarget = Target;
	InteractionStartTime = GetWorld()->GetTimeSeconds();
	CurrentInteractionDuration = IGS_InteractableInterface::Execute_GetInteractionDuration(Target);

	IGS_InteractableInterface::Execute_BeginInteract(Target, MySeeker);

	if (InteractionWidget)
	{
		const FText ActionText = IGS_InteractableInterface::Execute_GetInteractionText(Target);
		InteractionWidget->ShowInteraction(Target, CurrentInteractionDuration, ActionText);
	}
}

void AGS_TpsController::CancelInteraction()
{
	if (!bIsInteracting)
		return;

	if (CurrentInteractTarget.IsValid() && CurrentInteractTarget->Implements<UGS_InteractableInterface>())
	{
		IGS_InteractableInterface::Execute_EndInteract(CurrentInteractTarget.Get(), Cast<AGS_Seeker>(GetPawn()), false);
	}

	bIsInteracting = false;
	CurrentInteractTarget.Reset();
	CurrentInteractionDuration = 0.0f;

	if (InteractionWidget)
	{
		InteractionWidget->OnInteractionCancelled();
		InteractionWidget->HideInteraction();
	}
}

void AGS_TpsController::CompleteInteraction()
{
	if (!bIsInteracting)
		return;
	if (AActor* Target = CurrentInteractTarget.Get())
		Server_CompleteInteraction(Target);

	bIsInteracting = false;
	CurrentInteractTarget.Reset();
	CurrentInteractionDuration = 0.0f;

	if (InteractionWidget)
	{
		InteractionWidget->OnInteractionComplete();
		InteractionWidget->HideInteraction();
	}
}

void AGS_TpsController::Server_CompleteInteraction_Implementation(AActor* Target)
{
	AGS_Seeker* MySeeker = Cast<AGS_Seeker>(GetPawn());
	if (!IsValid(Target) || !IsValid(MySeeker))
		return;

	if (Target->Implements<UGS_InteractableInterface>())
	{
		IGS_InteractableInterface::Execute_EndInteract(Target, MySeeker, true);
	}
}

void AGS_TpsController::UpdateInteractionProgress(float DeltaTime)
{
	if (!bIsInteracting)
		return;

	if (!CurrentInteractTarget.IsValid())
	{
		CancelInteraction();
		return;
	}

	const float Progress = GetInteractionProgress();
	if (InteractionWidget)
		InteractionWidget->UpdateProgress(Progress);
	if (Progress >= 1.0f)
		CompleteInteraction();
}

void AGS_TpsController::CreateMainHUD()
{
	UE_LOG(LogTemp, Warning, TEXT("=================CreateMainHUD 호출==================="));
	AGS_Character* GS_Character = Cast<AGS_Character>(GetPawn());
	if (IsValid(GS_Character))
	{
		TSubclassOf<UUserWidget> Widget = PlayerWidgetClasses[GS_Character->GetCharacterType()];
		if (IsValid(Widget))
		{
			if (PlayerWidgetInstance)
			{
				PlayerWidgetInstance->RemoveFromParent();
				PlayerWidgetInstance = nullptr;
				CrosshairWidget = nullptr;
			}

			PlayerWidgetInstance = CreateWidget<UUserWidget>(this, Widget);
			if (IsValid(PlayerWidgetInstance))
			{
				UGS_HPBoardWidget* HPBoardWidget =
					Cast<UGS_HPBoardWidget>(PlayerWidgetInstance->GetWidgetFromName(TEXT("WBP_HPBoard")));
				UGS_BossHP* BossWidget =
					Cast<UGS_BossHP>(PlayerWidgetInstance->GetWidgetFromName(TEXT("WBP_BossHPBoard")));
				UGS_FeverGaugeBoard* FeverWidget =
					Cast<UGS_FeverGaugeBoard>(PlayerWidgetInstance->GetWidgetFromName(TEXT("WBP_FeverBoard")));

				if (IsValid(HPBoardWidget))
				{
					HPBoardWidget->InitBoardWidget();
				}
				if (IsValid(BossWidget))
				{
					BossWidget->InitGuardianHPWidget();
				}
				if (IsValid(FeverWidget))
				{
					FeverWidget->InitDrakharFeverWidget();
				}

				CrosshairWidget =
					Cast<UGS_CrossHairImage>(PlayerWidgetInstance->GetWidgetFromName(TEXT("WBP_CrossHair")));

				PlayerWidgetInstance->AddToViewport();
				UE_LOG(LogTemp, Warning, TEXT("CreateMainHUD에서 PlayerWidgetInstance 생성 및 뷰포트에 추가 성공"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CreateMainHUD 호출 시 Pawn이 유효하지 않습니다!"));
	}
}

void AGS_TpsController::TryCreatingPlayerWidget()
{
	// GetPawn()으로 Pawn이 유효한지 확인
	if (GetPawn())
	{
		// Pawn이 유효하면 CreateMainHUD()을 호출하고 타이머를 정리
		CreateMainHUD();
		if (GetWorld() && WaitForPawnTimerHandle.IsValid())
		{
			GetWorld()->GetTimerManager().ClearTimer(WaitForPawnTimerHandle);
		}
	}
	else
	{
		// Pawn이 아직 유효하지 않으면 0.2초 후에 이 함수를 다시 시도하도록 타이머 설정
		UE_LOG(LogTemp, Warning, TEXT("UI 위젯을 생성하기 위해 Pawn을 기다리는 중..."));
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(
				WaitForPawnTimerHandle, this, &AGS_TpsController::TryCreatingPlayerWidget, 0.2f, false);
		}
	}
}
