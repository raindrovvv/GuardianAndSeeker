#include "Character/Player/Guardian/GS_Guardian.h"
#include "Character/Player/Guardian/GS_DrakharAnimInstance.h"
#include "Animation/AnimInstance.h"
#include "Character/Component/GS_StatComp.h"
#include "Character/Skill/GS_SkillComp.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Character/Component/GS_CameraShakeComponent.h"
#include "Character/Component/GS_VFXComponent.h"
#include "Props/Interactables/GS_BridgePiece.h"
#include "Components/WidgetComponent.h"
#include "System/Subsystem/GS_ActorRegistrySubsystem.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Misc/App.h"
#include "Character/Component/GS_DebuffIndicatorComponent.h"


AGS_Guardian::AGS_Guardian(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	CameraShakeComponent = ObjectInitializer.CreateDefaultSubobject<UGS_CameraShakeComponent>(this, TEXT("CameraShakeComponent"));

	NormalMoveSpeed = GetCharacterMovement()->MaxWalkSpeed;
	SpeedUpMoveSpeed = 850.f;

	//boss monster tag for user widget
	Tags.Add("Guardian");

	// VFX 컴포넌트 생성 (디버프 등 모든 VFX)
	// NOTE: 자식 클래스(Drakhar 등)에서 FObjectInitializer::SetDefaultSubobjectClass 를 통해 클래스를 변경할 수 있음.
	VFXComponent = ObjectInitializer.CreateDefaultSubobject<UGS_VFXComponent>(this, TEXT("VFXComponent"));

	// 디버프 아이콘 표시 컴포넌트 생성 (시커/가디언 시점에서 보이는 머리 위 아이콘)
	DebuffIndicatorComponent = ObjectInitializer.CreateDefaultSubobject<UGS_DebuffIndicatorComponent>(this, TEXT("DebuffIndicatorComponent"));

	// 컴포넌트 생성 및 초기화
	TargetedUIComponent = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("TargetedUI"));
	TargetedUIComponent->SetupAttachment(RootComponent);
	TargetedUIComponent->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	TargetedUIComponent->SetWidgetSpace(EWidgetSpace::Screen);
	TargetedUIComponent->SetDrawSize(FVector2D(100.f, 100.f));
	TargetedUIComponent->SetVisibility(false);
}

void AGS_Guardian::BeginPlay()
{
	Super::BeginPlay();

	// Register to Subsystem for optimization
	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->RegisterGuardian(this);
		}
	}

	// === 애니메이션 틱 최적화 설정 ===
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (!FApp::CanEverRender())
		{
			// 서버는 화면이 없으므로 항상 틱을 수행하여 판정(AnimNotify) 누락 방지
			MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
		else
		{
			// 클라이언트는 최적화를 하되, 공격(몽타주) 중에는 화면 밖이라도 틱을 유지하여 노티파이 보장
			MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;

			// 추가 최적화: URO(Update Rate Optimization) 활성화
			MeshComp->bEnableUpdateRateOptimizations = true;

			// === 거리 기반 컬링 및 LOD 설정 ===
			float CullDistance = GS_Rendering::CalculateCullDistance(this, GetOptimalCullDistance());
			MeshComp->SetCullDistance(CullDistance);
			MeshComp->SetCachedMaxDrawDistance(CullDistance);
			MeshComp->MinLodModel = GS_Rendering::CalculateMinLOD(this);
			MeshComp->SetBoundsScale(GS_Rendering::DEFAULT_BOUNDS_SCALE);

			// === 그림자 컬링 타이머 시작 ===
			GetWorldTimerManager().SetTimer(
			    ShadowCullingTimerHandle,
			    this,
			    &AGS_Guardian::UpdateShadowCulling,
			    0.1f,
			    true,
			    FMath::RandRange(0.0f, 0.1f));
		}
	}
}

void AGS_Guardian::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ShadowCullingTimerHandle);

	// Unregister from Subsystem
	if (UWorld* World = GetWorld())
	{
		if (UGS_ActorRegistrySubsystem* Registry = World->GetSubsystem<UGS_ActorRegistrySubsystem>())
		{
			Registry->UnregisterGuardian(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AGS_Guardian::UpdateShadowCulling()
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		GS_Rendering::UpdateShadowCulling(this, MeshComp);
	}
}

void AGS_Guardian::OnSignificanceChanged(float NewSignificance)
{
	Super::OnSignificanceChanged(NewSignificance);

	// 가변 타이머 주기 조정 (Adaptive Timer)
	// 중요도에 따라 타이머 주기를 동적으로 변경하여 CPU 부하 분산
	if (FApp::CanEverRender())
	{
		float NewInterval = GS_Rendering::GetAdaptiveTimerInterval(NewSignificance);

		if (ShadowCullingTimerHandle.IsValid())
		{
			float Remaining = GetWorldTimerManager().GetTimerRemaining(ShadowCullingTimerHandle);
			GetWorldTimerManager().ClearTimer(ShadowCullingTimerHandle);
			GetWorldTimerManager().SetTimer(ShadowCullingTimerHandle, this, &AGS_Guardian::UpdateShadowCulling, NewInterval, true, FMath::Min(Remaining, NewInterval));
		}
	}
}

void AGS_Guardian::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	GuardianAnim = Cast<UGS_DrakharAnimInstance>(GetMesh()->GetAnimInstance());
}

void AGS_Guardian::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, GuardianState);
	DOREPLIFETIME(ThisClass, GuardianDoSkillState);
	DOREPLIFETIME(ThisClass, MoveSpeed);
}

void AGS_Guardian::LeftMouse()
{
}

void AGS_Guardian::Ctrl()
{
}

void AGS_Guardian::CtrlStop()
{
}

void AGS_Guardian::RightMouse()
{
}

void AGS_Guardian::StartCtrl()
{
}

void AGS_Guardian::StopCtrl()
{
}

void AGS_Guardian::OnRep_MoveSpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
}

void AGS_Guardian::MeleeAttackCheck()
{
	if (HasAuthority())
	{
		GuardianState = EGuardianCtrlState::CtrlEnd;

		const FVector Start = GetActorLocation() + GetActorForwardVector() * GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float MeleeAttackRange = 200.f;
		const float MeleeAttackRadius = 200.f;

		TSet<AGS_Character*> DamagedCharacters;
		DetectPlayerInRange(DamagedCharacters, Start, MeleeAttackRange, MeleeAttackRadius);
		ApplyDamageToDetectedPlayer(DamagedCharacters, 0.f);
	}
}

void AGS_Guardian::DetectPlayerInRange(TSet<AGS_Character*>& OutDamagedCharacters, const FVector& Start, float SkillRange, float Radius)
{
	OutDamagedCharacters.Reset();

	TArray<FHitResult> OutHitResults;
	FCollisionQueryParams Params(NAME_None, false, this);
	Params.AddIgnoredActor(this);

	FVector End = Start + GetActorForwardVector() * SkillRange;

	bool bIsHitDetected = GetWorld()->SweepMultiByChannel(OutHitResults, End, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params);

	//FOR DEBUGGING
	//MulticastRPCDrawDebugSphere(bIsHitDetected, End, Radius);

	if (bIsHitDetected)
	{
		for (auto const& OutHitResult : OutHitResults)
		{
			if (OutHitResult.GetComponent() && OutHitResult.GetComponent()->GetCollisionProfileName() == FName("SoundTrigger"))
			{
				continue;
			}
			AGS_Character* DamagedCharacter = Cast<AGS_Character>(OutHitResult.GetActor());
			if (IsValid(DamagedCharacter))
			{
				OutDamagedCharacters.Add(DamagedCharacter);
			}
			//break bridge
			if (OutHitResult.GetActor()->IsA<AGS_BridgePiece>())
			{
				AGS_BridgePiece* BridgePiece = Cast<AGS_BridgePiece>(OutHitResult.GetActor());
				if (BridgePiece)
				{
					BridgePiece->BrokeBridge(100.f);
				}
			}
		}
	}
}

void AGS_Guardian::ApplyDamageToDetectedPlayer(const TSet<AGS_Character*>& DamagedCharacters, float PlusDamge)
{
	for (auto const& DamagedCharacter : DamagedCharacters)
	{
		//[TODO] only damage logic in server
		//ServerRPCMeleeAttack(DamagedCharacter);

		UGS_StatComp* DamagedCharacterStat = DamagedCharacter->GetStatComp();
		if (IsValid(DamagedCharacterStat))
		{
			float Damage = DamagedCharacterStat->CalculateDamage(this, DamagedCharacter);
			FDamageEvent DamageEvent;
			DamagedCharacter->TakeDamage(Damage + PlusDamge, DamageEvent, GetController(), this);

			//hit stop
			MulticastRPCApplyHitStop(DamagedCharacter, HitStopDurtaion);

			// 피버 게이지 업데이트 (각 가디언이 자신의 방식으로 처리)
			OnFeverGaugeUpdate(10.f);

			// 공격 히트 처리 (각 가디언이 자신의 방식으로 처리)
			OnAttackHit(DamagedCharacter);
		}
	}
}

// === 가상 함수 기본 구현 (비어있음, 파생 클래스에서 필요시 오버라이드) ===
void AGS_Guardian::OnAttackHit(AGS_Character* HitCharacter)
{
	// 기본 구현은 비어있음 - 각 가디언이 필요시 오버라이드
}

void AGS_Guardian::OnFeverGaugeUpdate(float DeltaGauge)
{
	// 기본 구현은 비어있음 - 각 가디언이 필요시 오버라이드
}

void AGS_Guardian::OnQuitSkill()
{
	// 기본 구현은 비어있음 - 각 가디언이 필요시 오버라이드
}


// void AGS_Guardian::OnRep_GuardianState()
// {
// 	ClientGuardianState = GuardianState;
// }

// void AGS_Guardian::OnRep_GuardianDoSkillState()
// {
// 	ClientGuardianDoSkillState = GuardianDoSkillState;
// }

void AGS_Guardian::QuitGuardianSkill()
{
	//reset skill state
	GuardianState = EGuardianCtrlState::CtrlEnd;
	GuardianDoSkillState = EGuardianDoSkill::None;

	// 각 가디언의 스킬 종료 처리
	OnQuitSkill();

	//fly end
	GetSkillComp()->Server_TrySkillCanceledByDebuff(ESkillSlot::Ready);
}

void AGS_Guardian::FinishCtrlSkill()
{
	StopCtrl();
}

void AGS_Guardian::ShowTargetUI(bool bIsActive)
{
	if (TargetedUIComponent)
	{
		TargetedUIComponent->SetVisibility(bIsActive);
	}
}

FName AGS_Guardian::GetManualRowName_Implementation() const
{
	return ManualRowName;
}

float AGS_Guardian::GetFlySpeed()
{
	return SpeedUpMoveSpeed;
}

float AGS_Guardian::GetOptimalCullDistance() const
{
	// 대형 보스 캐릭터이므로 가장 먼 거리에서 컬링되도록 설정
	return GS_Rendering::MONSTER_LARGE_CULL_DISTANCE;
}

void AGS_Guardian::MulticastRPCApplyHitStop_Implementation(AGS_Character* InDamagedCharacter, float Duration)
{
	if (HasAuthority())
	{
		if (CameraShakeComponent)
		{
			CameraShakeComponent->PlayCameraShake(HitStopShakeInfo);
		}
	}
	if (!HasAuthority())
	{
		if (!IsValid(InDamagedCharacter))
		{
			return;
		}

		// 멀티플레이어 환경에서 아주 미세한 움직임은 유지 (0.1)
		CustomTimeDilation = 0.1f;
		InDamagedCharacter->CustomTimeDilation = 0.1f;

		FTimerDelegate HitStopTimerDelegate;
		FTimerHandle HitStopTimerHandle;
		HitStopTimerDelegate.BindUFunction(this, FName("MulticastRPCEndHitStop"), InDamagedCharacter);

		float FinalDuration = (Duration > 0.0f) ? Duration : HitStopDurtaion;
		GetWorld()->GetTimerManager().SetTimer(HitStopTimerHandle, HitStopTimerDelegate, FinalDuration, false);
	}
}

void AGS_Guardian::MulticastRPCEndHitStop_Implementation(AGS_Character* InDamagedCharacter)
{
	if (!HasAuthority())
	{
		CustomTimeDilation = 1.f;
		InDamagedCharacter->CustomTimeDilation = 1.f;
	}
}

void AGS_Guardian::MulticastRPCDrawDebugSphere_Implementation(bool bIsOverlap, const FVector& Location, float CapsuleRadius)
{
	FColor DebugColor = bIsOverlap ? FColor::Green : FColor::Red;
	DrawDebugSphere(GetWorld(), Location, CapsuleRadius, 16, DebugColor, false, 2.0f, 0, 1.0f);
}
