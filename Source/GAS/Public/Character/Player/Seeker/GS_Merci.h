// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GS_Seeker.h"
#include "Character/Interface/GS_AttackInterface.h"
#include "AkGameplayTypes.h"
#include "Weapon/Projectile/Seeker/GS_SeekerMerciArrowNormal.h"
#include "Animation/Character/E_SeekerAnim.h"
#include "GS_Merci.generated.h"

class AGS_SeekerMerciArrow;
class UAkComponent;
class UGS_ArrowTypeWidget;
class UNiagaraSystem;
class UGS_CrossHairImage;
class UGS_VisualPoolComp;

UCLASS()
class GAS_API AGS_Merci : public AGS_Seeker, public IGS_AttackInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AGS_Merci();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Attack Interface
	virtual void LeftClickPressed_Implementation() override;
	virtual void LeftClickRelease_Implementation() override;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	// 화살 발사 VFX
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayArrowShotVFX(FVector Location, FRotator Rotation, int32 NumArrows);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayArrowShotSound();

	// getter
	UFUNCTION(BlueprintCallable, Category = "Arrow")
	int32 GetMaxAxeArrows();

	UFUNCTION(BlueprintCallable, Category = "Arrow")
	int32 GetMaxChildArrows();

	// UI
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> WidgetCrosshairClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
	UGS_CrossHairImage* WidgetCrosshair;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* Quiver;

	// Attack
	UFUNCTION(BlueprintCallable)
	void DrawBow(UAnimMontage* DrawMontage);

	UFUNCTION(BlueprintCallable)
	void ReleaseArrow(TSubclassOf<AGS_SeekerMerciArrow> ArrowClass, float SpreadAngleDeg = 0.0f, int32 NumArrows = 1);

	UFUNCTION(Server, Reliable)
	void Server_DrawBow(UAnimMontage* DrawMontage);

	UFUNCTION(Server, Reliable)
	void Server_ReleaseArrow(TSubclassOf<AGS_SeekerMerciArrow> ArrowClass, float SpreadAngleDeg = 0.0f, int32 NumArrows = 1);

	UFUNCTION(Server, Reliable)
	void Server_FireArrow(TSubclassOf<AGS_SeekerMerciArrow> ArrowClass, float SpreadAngleDeg = 0.0f, int32 NumArrows = 1);

	// Arrow
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AGS_SeekerMerciArrow> NormalArrowClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AGS_SeekerMerciArrow> SmokeArrowClass;

	// Animation
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* ComboSkillDrawMontage;

	UFUNCTION(Server, Reliable)
	void Server_NotifyDrawMontageEnded();

	virtual void StateReset() override;
	void OnDrawMontageEnded();

	bool GetIsFullyDrawn() { return bIsFullyDrawn; }

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_DrawDebugLine(FVector Start, FVector End, FColor Color = FColor::Green);

	UFUNCTION(Server, Reliable)
	void Server_ChangeArrowType(int32 Direction);

	void SetArrowTypeWidget(UGS_ArrowTypeWidget* Widget) { ArrowTypeWidget = Widget; }

	// Auto Aiming
	void SetAutoAimTarget(AActor* Target);

	// Camera Control
	UFUNCTION(Client, Unreliable)
	void Client_StartZoom();

	UFUNCTION(Client, Unreliable)
	void Client_StopZoom(float Duration);

	//Crosshair
	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetCrosshairWidget(UGS_CrossHairImage* InCrosshairWidget);

	UFUNCTION(Client, Unreliable)
	void Client_UpdateCrosshairAim(bool bAiming);

	UFUNCTION(Client, Unreliable)
	void Client_ShowCrosshairHitFeedback();

	UFUNCTION(Client, Unreliable)
	void Client_PlayHitFeedbackSound();

	UFUNCTION(Client, Unreliable)
	void Client_PlayArrowEmptySound();

	UFUNCTION(Client, Unreliable)
	void Client_UpdateTargetUI(AActor* NewTarget, AActor* OldTarget);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimization")
	UGS_VisualPoolComp* VisualPool;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 화살 발사 VFX 에셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|Attack")
	UNiagaraSystem* ArrowShotVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|Attack")
	UNiagaraSystem* MultiShotVFX;

	// VFX 위치 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|Attack", meta = (AllowPrivateAccess = "true"))
	FVector ArrowShotVFXOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX|Attack", meta = (AllowPrivateAccess = "true"))
	FVector MultiShotVFXOffset = FVector::ZeroVector;

	// 타임라인 관련
	FTimeline ZoomTimeline;

	void ZoomTimelineReverse();

	UPROPERTY(EditAnywhere)
	UCurveFloat* ZoomCurve;

	UFUNCTION()
	void UpdateZoom(float Alpha);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	FTimerHandle ReverseTimerHandle;
	FTimerHandle DamageRecoveryTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Optimization")
	TSubclassOf<AGS_ArrowVisualActor> VisualArrowClass;

	UGS_ArrowTypeWidget* ArrowTypeWidget;

	bool bWidgetVisibility = false;

	void PlayDrawMontage(UAnimMontage* DrawMontage);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_StopDrawMontage();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDrawMontage(UAnimMontage* Montage);

	UFUNCTION(Client, Unreliable)
	void Client_SetWidgetVisibility(bool bVisible);

	UFUNCTION(Client, Unreliable)
	void Client_PlaySound(UAkComponent* SoundComp);

	bool bIsFullyDrawn = false;

	// [화살 관리]
	int32 MaxAxeArrows = 5;
	int32 MaxChildArrows = 3;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentArrowType)
	EArrowType CurrentArrowType;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentAxeArrows)
	int32 CurrentAxeArrows;
	UPROPERTY(ReplicatedUsing = OnRep_CurrentChildArrows)
	int32 CurrentChildArrows;

	UFUNCTION()
	void OnRep_CurrentArrowType();

	UFUNCTION()
	void OnRep_CurrentAxeArrows();

	UFUNCTION()
	void OnRep_CurrentChildArrows();

	FTimerHandle AxeArrowRegenTimer;
	FTimerHandle ChildArrowRegenTimer;

	float RegenInterval = 5.0f; // 5초마다 충전

	UFUNCTION()
	void RegenAxeArrow();

	UFUNCTION()
	void RegenChildArrow();

	// Auto Aiming
	UPROPERTY(ReplicatedUsing = OnRep_AutoAimTarget)
	AActor* AutoAimTarget;

	UFUNCTION()
	void OnRep_AutoAimTarget();

	UFUNCTION(NetMulticast, Reliable)
	void Client_DrawDebugSphere(FVector Loc, float Radius, FColor Color, float Duration);

	bool Zooming = false;
};
