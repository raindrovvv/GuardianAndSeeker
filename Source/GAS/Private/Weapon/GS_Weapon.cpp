#include "Weapon/GS_Weapon.h"
#include "Weapon/Projectile/GS_WeaponProjectile.h"
#include "Rendering/GS_RenderingConstants.h"
#include "Components/MeshComponent.h"
#include "NiagaraComponent.h"
#include "AkGameplayStatics.h"
#include "AkRtpc.h"
#include "Character/GS_Character.h"
#include "Engine/HitResult.h"
#include "AkAudioEvent.h"
#include "AI/RTS/GS_RTSController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/GS_AudioMixingComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

// Sets default values
AGS_Weapon::AGS_Weapon()
{
	// Set this actor to call Tick() every frame.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	// 다이내믹 사운드 믹싱 컴포넌트 생성
	AudioMixingComponent = CreateDefaultSubobject<UGS_AudioMixingComponent>(TEXT("AudioMixingComponent"));
}

// Called when the game starts or when spawned
void AGS_Weapon::BeginPlay()
{
	Super::BeginPlay();

	// === 무기 메시 Distance Culling 설정 (클라이언트 전용) ===
	if (!IsRunningDedicatedServer())
	{
		// 투사체인지 확인 (투사체는 항상 보여야 하므로 최적화 완화)
		bool bIsProjectile = IsA(AGS_WeaponProjectile::StaticClass());

		int32 MinLOD = bIsProjectile ? 0 : GS_Rendering::CalculateMinLOD(this);
		float BaseCullDistance = bIsProjectile ? GS_Rendering::PROJECTILE_CULL_DISTANCE : GS_Rendering::WEAPON_CULL_DISTANCE;
		float FinalCullDistance = GS_Rendering::CalculateCullDistance(this, BaseCullDistance);

		// 1. 메시 컴포넌트 최적화
		TArray<UMeshComponent*> MeshComponents;
		GetComponents<UMeshComponent>(MeshComponents);

		for (UMeshComponent* MeshComp : MeshComponents)
		{
			if (MeshComp)
			{
				MeshComp->SetCullDistance(FinalCullDistance);
				MeshComp->SetCachedMaxDrawDistance(FinalCullDistance);
				// 투사체는 월드의 Cull Distance Volume 영향을 받지 않도록 설정 (원거리 가시성 보장)
				MeshComp->bAllowCullDistanceVolume = !bIsProjectile;
				MeshComp->SetBoundsScale(GS_Rendering::DEFAULT_BOUNDS_SCALE);

				if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(MeshComp))
				{
					StaticMesh->MinLOD = MinLOD;
				}
				else if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(MeshComp))
				{
					SkeletalMesh->MinLodModel = MinLOD;
				}
			}
		}

		// 2. 나이아가라(VFX) 컴포넌트 최적화
		TArray<UNiagaraComponent*> NiagaraComponents;
		GetComponents<UNiagaraComponent>(NiagaraComponents);

		for (UNiagaraComponent* NiagaraComp : NiagaraComponents)
		{
			if (NiagaraComp)
			{
				NiagaraComp->SetCullDistance(FinalCullDistance);
				NiagaraComp->SetCachedMaxDrawDistance(FinalCullDistance);
				// VFX도 투사체의 것은 볼륨 컬링에서 제외
				NiagaraComp->bAllowCullDistanceVolume = !bIsProjectile;
			}
		}
	}
}

void AGS_Weapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 타이머 정리 (레벨 전환 크래시 방지)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AudioFocusTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}


void AGS_Weapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

// Called every frame
void AGS_Weapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGS_Weapon::ApplyImpactFocus()
{
	if (!ImpactFocusRTPC)
		return;

	// 1. 서버라면 발사한 플레이어(Owner)에게 RPC를 보냄
	if (HasAuthority())
	{
		Client_ApplyImpactFocus();
	}
	// 2. 클라이언트(또는 리슨서버 호스트)라면 바로 실행
	else
	{
		Internal_ApplyImpactFocus();
	}
}

void AGS_Weapon::Client_ApplyImpactFocus_Implementation()
{
	// 🔴 RPC 유효성 검사 (레벨 전환 시 null 참조 방지)
	if (!IsValidForLevelTransition())
	{
		return;
	}

	// 서버로부터 받은 RPC를 로컬에서 실행
	Internal_ApplyImpactFocus();
}

void AGS_Weapon::Internal_ApplyImpactFocus()
{
	// 에디터/전용서버 체크 및 RTPC 유효성 검사
	UWorld* World = GetWorld();
	if (!World || IsRunningDedicatedServer() || !ImpactFocusRTPC)
	{
		return;
	}

	// Wwise RTPC 적용 (40ms 동안 부드럽게 볼륨 감소)
	UAkGameplayStatics::SetRTPCValue(ImpactFocusRTPC, DuckingValue, 40, nullptr);

	// 타켓 캐릭터(발사자 또는 소유자)를 찾아 타이머를 위임
	// 화살 액터가 파괴되어도 캐릭터가 살아있으면 타이머가 유지됨
	AGS_Character* OwnerCharacter = Cast<AGS_Character>(GetInstigator());
	if (!OwnerCharacter)
		OwnerCharacter = Cast<AGS_Character>(GetOwner());

	if (OwnerCharacter)
	{
		TWeakObjectPtr<UAkRtpc> WeakRTPC = ImpactFocusRTPC;
		float Duration = FocusDuration;

		FTimerManager& TimerManager = OwnerCharacter->GetWorldTimerManager();
		TimerManager.ClearTimer(OwnerCharacter->GetAudioFocusTimerHandle());

		// 람다를 사용하여 액터(this)가 파괴되어도 안전하게 사운드 복구 실행
		TimerManager.SetTimer(OwnerCharacter->GetAudioFocusTimerHandle(), [WeakRTPC]()
		                      {
			                    if (WeakRTPC.IsValid())
			                    {
				                    UAkGameplayStatics::SetRTPCValue(WeakRTPC.Get(), 1.0f, 100, nullptr);
			                    } }, Duration, false);
	}
}

void AGS_Weapon::RestoreImpactFocus()
{
	if (!ImpactFocusRTPC || IsRunningDedicatedServer())
	{
		return;
	}

	// 100ms 동안 부드럽게 원래 볼륨(1.0)으로 복구
	UAkGameplayStatics::SetRTPCValue(ImpactFocusRTPC, 1.0f, 100, nullptr);
}

void AGS_Weapon::PlayLayeredHitSound(const FHitResult& HitResult, AActor* HitActor)
{
	// 에디터/전용서버 체크 및 유효성 검사
	UWorld* World = GetWorld();
	if (!World || IsRunningDedicatedServer() || !AudioMixingComponent)
	{
		return;
	}

	// 1. 피격 레이어 선택 (살점 vs 갑옷 vs 기타)
	UAkAudioEvent* ImpactEvent = nullptr;

	if (AGS_Character* TargetChar = Cast<AGS_Character>(HitActor))
	{
		EImpactMaterialType MatType = TargetChar->GetImpactMaterialType();

		switch (MatType)
		{
		case EImpactMaterialType::Armor:
			ImpactEvent = ImpactArmorSoundEvent;
			break;
		case EImpactMaterialType::Stone:
			ImpactEvent = ImpactStoneSoundEvent;
			break;
		case EImpactMaterialType::Wood:
			ImpactEvent = ImpactWoodSoundEvent;
			break;
		case EImpactMaterialType::Flesh:
		default:
			ImpactEvent = ImpactFleshSoundEvent;
			break;
		}

		// 캐릭터를 때렸는데 해당 재질 이벤트가 없는 경우에만 Flesh로 폴백
		if (!ImpactEvent)
		{
			ImpactEvent = ImpactFleshSoundEvent;
		}
	}
	else
	{
		/**
		 * [중요] 환경 오브젝트(벽, 구조물 등) 처리
		 * 화살의 경우 ArrowFXComponent에서 이미 핵심 타격음(Stone 등)을 재생하므로,
		 * 여기서는 ImpactWallSoundEvent가 설정되어 있을 때만 추가로 재생하거나
		 * 설정되어 있지 않으면 아예 재생하지 않습니다. (Flesh 폴백 금지)
		 */
		ImpactEvent = ImpactWallSoundEvent;
	}

	// [재생] 컴포넌트를 통해 레이어드 사운드 재생 (ImpactEvent가 null이어도 Reverb는 재생됨)
	AudioMixingComponent->PostLayeredSound(ImpactEvent, ReverbSoundEvent, HitResult.ImpactPoint, HitActor);

	// [덕킹] 로컬 플레이어가 공격자(발사자)인 경우 사운드 포커스(Ducking) 적용
	if (ImpactFocusRTPC)
	{
		AGS_Character* InstigatorChar = Cast<AGS_Character>(GetInstigator());
		if (!InstigatorChar)
		{
			InstigatorChar = Cast<AGS_Character>(GetOwner());
		}

		if (InstigatorChar && InstigatorChar->IsLocallyControlled())
		{
			ApplyImpactFocus();
		}
	}
}

void AGS_Weapon::Multicast_PlayLayeredHitSound_Implementation(const FHitResult& HitResult, AActor* HitActor)
{
	PlayLayeredHitSound(HitResult, HitActor);
}

bool AGS_Weapon::IsValidForLevelTransition() const
{
	UWorld* World = GetWorld();
	return IsValid(this) && World && !World->bIsTearingDown;
}

void AGS_Weapon::Multicast_PlaySpecialHitVFX_Implementation(UNiagaraSystem* VFXToPlay, const FHitResult& HitResult)
{
	if (!IsValidForLevelTransition())
	{
		return;
	}

	if (AGS_Character* Character = Cast<AGS_Character>(GetOwner()))
	{
		if (Character->ShouldPlayVFXAtLocation(HitResult.ImpactPoint, 4000.0f))
		{
			if (UWorld* World = GetWorld())
			{
				if (VFXToPlay)
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					    World,
					    VFXToPlay,
					    HitResult.ImpactPoint,
					    HitResult.ImpactNormal.Rotation(),
					    FVector(1.0f),
					    true,
					    true);
				}
			}
		}
	}
}
