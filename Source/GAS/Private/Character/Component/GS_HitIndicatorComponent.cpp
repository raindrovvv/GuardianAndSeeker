// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Component/GS_HitIndicatorComponent.h"
#include "UI/Character/GS_HitIndicatorWidget.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Blueprint/UserWidget.h"

UGS_HitIndicatorComponent::UGS_HitIndicatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGS_HitIndicatorComponent::BeginPlay()
{
	Super::BeginPlay();

	// 로컬 플레이어만 HUD 위젯 생성
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[HitIndicator] BeginPlay - Owner: %s, IsLocallyControlled: %d"),
		       *OwnerPawn->GetName(), OwnerPawn->IsLocallyControlled());

		if (OwnerPawn->IsLocallyControlled())
		{
			CreateHitIndicatorWidget();
		}
	}
}

void UGS_HitIndicatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HitIndicatorWidget)
	{
		HitIndicatorWidget->RemoveFromParent();
		HitIndicatorWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UGS_HitIndicatorComponent::CreateHitIndicatorWidget()
{
	if (!HitIndicatorWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[HitIndicator] CreateWidget FAILED - WidgetClass is NULL!"));
		return;
	}

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			HitIndicatorWidget = CreateWidget<UGS_HitIndicatorWidget>(PC, HitIndicatorWidgetClass);
			if (HitIndicatorWidget)
			{
				HitIndicatorWidget->AddToViewport(-1); // 메인 HUD(0)보다 뒤에 표시
				HitIndicatorWidget->SetOwnerComponent(this);
				UE_LOG(LogTemp, Warning, TEXT("[HitIndicator] Widget Created Successfully!"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[HitIndicator] CreateWidget returned NULL!"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[HitIndicator] No PlayerController found!"));
		}
	}
}

void UGS_HitIndicatorComponent::NotifyDamageDirection(const FVector& WorldHitDirection, float DamageAmount)
{
	UE_LOG(LogTemp, Warning, TEXT("[HitIndicator] NotifyDamageDirection called - Dir: %s, Damage: %.1f"),
	       *WorldHitDirection.ToString(), DamageAmount);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	// 서버에서 실행 중이면 클라이언트로 RPC 전송
	if (GetOwner()->HasAuthority())
	{
		// 리슨 서버에서 로컬 플레이어인 경우 직접 처리
		if (OwnerPawn->IsLocallyControlled())
		{
			UE_LOG(LogTemp, Warning, TEXT("[HitIndicator] Server: Local player - processing directly"));
			ShowHitIndicatorInternal(WorldHitDirection, DamageAmount);
		}
		else
		{
			// 클라이언트 소유 Pawn이면 Client RPC 전송
			UE_LOG(LogTemp, Warning, TEXT("[HitIndicator] Server: Sending Client RPC"));
			Client_ShowHitIndicator(WorldHitDirection, DamageAmount);
		}
	}
	else
	{
		// 클라이언트에서 직접 실행 (드문 경우)
		if (OwnerPawn->IsLocallyControlled())
		{
			ShowHitIndicatorInternal(WorldHitDirection, DamageAmount);
		}
	}
}

void UGS_HitIndicatorComponent::Client_ShowHitIndicator_Implementation(const FVector& WorldHitDirection, float DamageAmount)
{
	UE_LOG(LogTemp, Warning, TEXT("[HitIndicator] Client RPC received - Dir: %s, Damage: %.1f"),
	       *WorldHitDirection.ToString(), DamageAmount);
	ShowHitIndicatorInternal(WorldHitDirection, DamageAmount);
}

void UGS_HitIndicatorComponent::ShowHitIndicatorInternal(const FVector& WorldHitDirection, float DamageAmount)
{
	// 방향 계산
	EHitDirection Direction = CalculateHitDirectionFromCamera(WorldHitDirection);

	UE_LOG(LogTemp, Warning, TEXT("[HitIndicator] Calculated Direction: %d"), (int32)Direction);

	if (Direction != EHitDirection::None)
	{
		// 위젯에 직접 호출
		if (HitIndicatorWidget)
		{
			HitIndicatorWidget->ShowHitIndicator(Direction, DamageAmount);
			UE_LOG(LogTemp, Warning, TEXT("[HitIndicator] ShowHitIndicator called on widget!"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[HitIndicator] Widget is NULL!"));
		}

		// 델리게이트도 브로드캐스트 (다른 곳에서 구독할 수 있도록)
		OnDamageDirectionReceived.Broadcast(Direction, DamageAmount);
	}
}

EHitDirection UGS_HitIndicatorComponent::CalculateHitDirectionFromCamera(const FVector& WorldHitDirection) const
{
	if (WorldHitDirection.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("[HitIndicator] HitDirection is nearly zero! Returning Omni direction."));
		return EHitDirection::Omni;
	}

	// 카메라 방향 가져오기
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HitIndicator] OwnerPawn is NULL!"));
		return EHitDirection::None;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HitIndicator] PC or CameraManager is NULL!"));
		return EHitDirection::None;
	}

	// 카메라 방향 가져오기
	FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();

	// 1. 수평(XY 평면) 판정을 위한 카메라 축 (Pitch를 제거하여 수평으로 유지)
	FRotator HorizontalCameraRot = CameraRotation;
	HorizontalCameraRot.Pitch = 0.f;
	HorizontalCameraRot.Roll = 0.f;

	FVector CamForward = HorizontalCameraRot.Vector();
	FVector CamRight = FRotationMatrix(HorizontalCameraRot).GetScaledAxis(EAxis::Y);

	// 2. 공격자가 나를 기준으로 월드 맵 상 어디에 있는지 (Victim -> Attacker)
	FVector DirToAttacker = WorldHitDirection.GetSafeNormal();

	// 3. 수직 판정: 순수 월드 Z축 차이 사용 (카메라가 어디를 보든 상관없음)
	// DirToAttacker.Z가 크면 머리 위 함정, 작으면 발 밑 함정
	float UpDot = DirToAttacker.Z;

	// 4. 수평 판정: 방향 벡터를 수평 평면에 투영하여 계산
	FVector HorizontalDir = DirToAttacker;
	HorizontalDir.Z = 0.f;

	float ForwardDot = 0.f;
	float RightDot = 0.f;

	// 수평 방향 벡터 크기가 유효할 때만 정규화 및 내적 계산
	if (!HorizontalDir.IsNearlyZero())
	{
		HorizontalDir.Normalize();
		ForwardDot = FVector::DotProduct(HorizontalDir, CamForward);
		RightDot = FVector::DotProduct(HorizontalDir, CamRight);
	}
	// else: 순수 수직 공격(Z만 있음) - ForwardDot/RightDot은 0으로 유지되어 상하 판정만 사용됨

	UE_LOG(LogTemp, Warning, TEXT("[HitIndicator] WorldZ: %.3f, Fwd: %.3f, Right: %.3f"),
	       UpDot, ForwardDot, RightDot);

	return DetermineDirection(ForwardDot, RightDot, UpDot);
}

EHitDirection UGS_HitIndicatorComponent::DetermineDirection(float ForwardDot, float RightDot, float UpDot) const
{
	float AbsUp = FMath::Abs(UpDot);
	float AbsForward = FMath::Abs(ForwardDot);
	float AbsRight = FMath::Abs(RightDot);
	float MaxHorizontal = FMath::Max(AbsForward, AbsRight);

	// 1. 수직 방향 체크 - 수직 성분이 임계값 이상이고, 수평 성분보다 확실히 클 때만 상하 판정
	// 이렇게 하면 프로젝타일이 약간 아래에서 맞아도 수평 방향으로 판정됨
	if (AbsUp > VerticalThreshold && AbsUp > MaxHorizontal * 1.5f)
	{
		if (UpDot > 0.f)
		{
			return EHitDirection::Up;
		}
		else
		{
			return EHitDirection::Down;
		}
	}

	// 2. 수평 방향 판별 (전후좌우 중 가장 강한 방향 선택)
	if (AbsForward > AbsRight)
	{
		// 전방 또는 후방
		return (ForwardDot > 0.f) ? EHitDirection::Front : EHitDirection::Back;
	}
	else
	{
		// 좌측 또는 우측
		return (RightDot > 0.f) ? EHitDirection::Right : EHitDirection::Left;
	}
}
