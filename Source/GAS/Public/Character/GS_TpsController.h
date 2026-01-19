// Copyright 2024 Greed Fennec Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/GS_BasePlayerController.h"
#include "Character/GS_Character.h"
#include "UI/Character/GS_CrossHairImage.h"
#include "Character/Skill/ESkill.h"
#include "GS_TpsController.generated.h"

class UGS_GameInstance;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class AGS_Seeker;
class UGS_ReviveIndicatorWidget;
class IGS_InteractableInterface;
class UGS_InteractionWidget;

/**
 * @brief Third-person player controller for the Seeker team, managing movement, combat input, revival, and interaction.
 */
UCLASS()
class GAS_API AGS_TpsController : public AGS_BasePlayerController
{
	GENERATED_BODY()

public:
	AGS_TpsController();

	/** --- Input Actions --- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeker|Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeker|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeker|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeker|Input")
	TObjectPtr<UInputAction> WalkToggleAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeker|Input")
	TObjectPtr<UInputAction> PlaceMarkerAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeker|Input")
	TObjectPtr<UInputAction> RClickAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeker|Input")
	TObjectPtr<UInputAction> PageUpAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeker|Input")
	TObjectPtr<UInputAction> PageDownAction;

	/** Action for reviving downed teammates (default: E key) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeker|Input|Revive")
	TObjectPtr<UInputAction> ReviveAction;


	/** --- UI Management --- */

	/** Active instance of the player's main HUD/Overlay widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> PlayerWidgetInstance;

	/** Mapping of character types to their specific HUD widget classes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeker|UI")
	TMap<ECharacterType, TSubclassOf<UUserWidget>> PlayerWidgetClasses;

	/** @return The active player HUD widget instance */
	UFUNCTION(BlueprintCallable, Category = "Seeker|UI")
	UUserWidget* GetPlayerWidget();

	/** Initializes or updates the player HUD based on the current pawn's character type */
	UFUNCTION(BlueprintCallable, Category = "Seeker|UI")
	void InitializePlayerHUD();


	/** --- System Logic --- */

	/** Sets up the local audio listener to follow the player pawn */
	UFUNCTION(BlueprintCallable, Category = "Seeker|Audio")
	void SetupPlayerAudioListener();

	/** Input handlers */
	void Move(const FInputActionValue& InputValue);
	void Look(const FInputActionValue& InputValue);
	void WalkToggle(const FInputActionValue& InputValue);
	void PlaceMarker(const FInputActionValue& InputValue);
	void PageUp(const FInputActionValue& InputValue);
	void PageDown(const FInputActionValue& InputValue);

	/** Global initialization per level load */
	void InitControllerPerWorld();

	/** Requests to spectate a different surviving teammate on the server */
	UFUNCTION(Server, Unreliable)
	void ServerRPCSpectatePlayer(int32 Step = 1);

	/** Triggered on clients when they enter spectator mode to update UI visibility */
	UFUNCTION(Client, Reliable)
	void ClientRPC_OnSpectatorModeStarted();

	/** Index of the player currently being spectated */
	int32 SpectatorIndex = -1;

	/** Movement and look restriction control */
	UFUNCTION()
	FControlValue GetControlValue() const;

	UFUNCTION()
	void SetMoveControlValue(bool CanMoveRight, bool CanMoveForward);

	UFUNCTION()
	FControlValue GetMoveControlValue();

	UFUNCTION()
	void SetLookControlValue(bool CanLookRight, bool CanLookUp);

	/** Active movement/look permissions, replicated for server authority */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Seeker|Control")
	FControlValue ControlValues;

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	/** Stores the rotation state when movement started for interpolation or snapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeker|Control")
	FRotator LastRotatorInMoving;

	/** Current raw movement input vector, throttled and replicated */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Seeker|Control")
	FVector2D MoveInputValue;

	/** Synchronizes high-frequency movement input to the server */
	UFUNCTION(Server, Unreliable)
	void Server_CacheMoveInputValue(FVector2D InputValue);

	/** @return The current effective mouse sensitivity from settings */
	UFUNCTION(BlueprintCallable, Category = "Seeker|Settings")
	float GetCurrentMouseSensitivity() const;

	/** @return The crosshair widget if valid (primary used by Seeker class "Merci") */
	UFUNCTION(BlueprintCallable, Category = "Seeker|UI")
	UGS_CrossHairImage* GetCrosshairWidget() const
	{
		return CrosshairWidget;
	}


	/** --- Movement Skills --- */

	/** Initiates automated forward movement (e.g., during a charge skill) */
	void StartAutoMoveForward();

	/** Terminates automated forward movement */
	void StopAutoMoveForward();

	/** Sets the auto-movement state on the server */
	void SetIsAutoMoving(bool InIsAutoMoving);

	/** --- Debugging --- */

	/** Renders aim-assist debug visuals on the local client */
	UFUNCTION(Client, Unreliable)
	void
	Client_DrawAimAssistDebug(const FVector& Start, const FVector& End, const FVector& TargetLocation, float Duration);


	/** --- Revival System --- */

	/** Attempt to start reviving a downed teammate */
	void TryStartRevive(const FInputActionValue& InputValue);

	/** Stop reviving attempt (e.g., key released or target moved) */
	void StopRevive(const FInputActionValue& InputValue);

	/** @return Whether the controller is currently in a reviving state */
	UFUNCTION(BlueprintPure, Category = "Seeker|Revive")
	bool IsReviving() const
	{
		return bIsReviving;
	}

	/** @return Whether the revive input key is currently held down */
	UFUNCTION(BlueprintPure, Category = "Seeker|Revive")
	bool IsHoldingReviveKey() const
	{
		return bIsHoldingReviveKey;
	}

	/** @return The current target teammate being revived */
	UFUNCTION(BlueprintPure, Category = "Seeker|Revive")
	AGS_Seeker* GetReviveTarget() const
	{
		return ReviveTarget.Get();
	}

	/** Notifies the server to begin health restoration on a target */
	UFUNCTION(Server, Reliable, Category = "Seeker|Revive")
	void Server_RequestRevive(AGS_Seeker* Target);

	/** Notifies the server to abort the current revive operation */
	UFUNCTION(Server, Reliable, Category = "Seeker|Revive")
	void Server_CancelRevive();

	/** Synchronizes revive key hold state for server-side logic validation */
	UFUNCTION(Server, Unreliable, Category = "Seeker|Revive")
	void Server_SetHoldingReviveKey(bool bIsHolding);


	/** --- Interaction System --- */

	/** @return The closest interactable actor detected recently */
	UFUNCTION(BlueprintCallable, Category = "Seeker|Interaction")
	AActor* GetNearbyInteractable() const
	{
		return CachedInteractable.Get();
	}

	/** @return Whether the player is currently performing an interaction */
	UFUNCTION(BlueprintPure, Category = "Seeker|Interaction")
	bool IsInteracting() const
	{
		return bIsInteracting;
	}

	/** @return Current interaction progress normalized (0.0 to 1.0) */
	UFUNCTION(BlueprintPure, Category = "Seeker|Interaction")
	float GetInteractionProgress() const;

	/** Signals interaction completion to the server for validation/rewards */
	UFUNCTION(Server, Reliable, Category = "Seeker|Interaction")
	void Server_CompleteInteraction(AActor* Target);

	/** Scans the vicinity for interactable objects based on priority */
	void UpdateNearbyInteractable();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupInputComponent() override;
	virtual void PostSeamlessTravel() override;
	virtual void BeginPlayingState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Cached reference to the global game instance */
	UPROPERTY(BlueprintReadOnly, Category = "Seeker|Context")
	TObjectPtr<UGS_GameInstance> GameInstance;

	/** Managed crosshair widget for ranged characters */
	UPROPERTY(BlueprintReadOnly, Category = "Seeker|UI")
	TObjectPtr<UGS_CrossHairImage> CrosshairWidget;

	/** Aligns the camera rotation with the character's forward vector */
	void SnapCameraToCharacterYaw();

	/** Periodically scans for downed allies and updates HUD visibility markers */
	void UpdateReviveIndicatorVisibility();

private:
	/** Timer handles for various periodic updates */
	FTimerHandle AutoMoveTickHandle;
	FTimerHandle ReviveIndicatorTimerHandle;
	FTimerHandle InteractableUpdateTimerHandle;
	FTimerHandle WaitForPawnTimerHandle;

	/** Whether auto-movement is currently active (replicated) */
	UPROPERTY(Replicated)
	bool bIsAutoMoving = false;


	/** --- Revival Variables --- */

	/** Locally tracked revive state */
	bool bIsReviving = false;

	/** Replicated state of the revive key (E) */
	UPROPERTY(Replicated)
	bool bIsHoldingReviveKey = false;

	/** Weak reference to the revive target to prevent lifespan issues */
	TWeakObjectPtr<AGS_Seeker> ReviveTarget;

	/** Whether a valid revive target was recently identified in range */
	bool bNearbyDyingSeekerDetected = false;

	/** Tracking the specific teammate detected for HUD feedback consistency */
	TWeakObjectPtr<AGS_Seeker> LastDetectedDyingSeeker;

	/** Internal logic to find the best candidate for revival */
	AGS_Seeker* FindNearbyDyingSeeker() const;

	/** Max range allowed for starting or maintaining a revival sequence */
	UPROPERTY(EditDefaultsOnly, Category = "Seeker|Revive", meta = (ClampMin = "100.0", ClampMax = "500.0"))
	float ReviveDistance = 200.0f;

	/** UI Class for displaying revival cues on downed allies */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Seeker|Revive", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGS_ReviveIndicatorWidget> ReviveIndicatorWidgetClass;

	/** Instance of the revive status widget */
	UPROPERTY()
	TObjectPtr<UGS_ReviveIndicatorWidget> ReviveIndicatorWidget;

	/** UI Class for general object interactions */
	UPROPERTY(EditDefaultsOnly,
			  BlueprintReadOnly,
			  Category = "Seeker|Interaction",
			  meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGS_InteractionWidget> InteractionWidgetClass;

	/** Instance of the interaction feedback widget */
	UPROPERTY()
	TObjectPtr<UGS_InteractionWidget> InteractionWidget;


	/** --- Movement Input Optimization --- */

	FVector2D LastSentMoveInputValue = FVector2D::ZeroVector;
	float LastMoveInputSentTime = 0.0f;
	const float MoveInputSendThreshold = 0.05f;
	const float MoveInputMinSendInterval = 0.033f;

	/** Logic update for automated traversal */
	void AutoMoveTick();

	/** RPCs for controlling client-side camera/VFX during auto-move */
	UFUNCTION(Client, Reliable)
	void Client_StartAutoMoveForward();

	UFUNCTION(Client, Reliable)
	void Client_StopAutoMoveForward();

	/** Camera configuration methods */
	void ApplyChargeCameraSettings(bool bCharging);
	void SaveOriginalCameraSettings();
	void RestoreOriginalCameraSettings();

	/** Temporary storage for restoring camera state after skills */
	bool bOriginalUseControllerRotationYaw = true;
	bool bOriginalOrientRotationToMovement = false;
	bool bOriginalUsePawnControlRotation = true;
	bool bOriginalEnableCameraLag = true;
	bool bOriginalEnableCameraRotationLag = true;
	bool bOriginalInheritYaw = true;


	/** --- Interaction Variables --- */

	/** Locally tracked interaction state */
	bool bIsInteracting = false;

	/** Timestamp of when the current interaction began */
	float InteractionStartTime = 0.0f;

	/** Required time to hold the interaction for completion */
	float CurrentInteractionDuration = 0.0f;

	/** Targeted actor for the current ongoing interaction */
	TWeakObjectPtr<AActor> CurrentInteractTarget;

	/** Closest valid interactable actor cached to reduce per-frame overhead */
	TWeakObjectPtr<AActor> CachedInteractable;

	/** Internal flow control for interaction sequence */
	void StartInteraction(AActor* Target);
	void CancelInteraction();
	void CompleteInteraction();
	void UpdateInteractionProgress(float DeltaTime);

	// Creates and initializes the main HUD based on character type
	void CreateMainHUD();

	// Waits for pawn to be valid before creating player widget
	void TryCreatingPlayerWidget();
};
