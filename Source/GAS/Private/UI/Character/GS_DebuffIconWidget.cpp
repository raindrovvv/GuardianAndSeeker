// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Character/GS_DebuffIconWidget.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

UGS_DebuffIconWidget::UGS_DebuffIconWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	// CDO에서는 기본값만 설정 (UPROPERTY 기본값은 에디터에서 설정)
}

void UGS_DebuffIconWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 동적 머티리얼 생성 (게이지용)
	if (GaugeMaterial && GaugeImage)
	{
		GaugeMaterialInstance = UMaterialInstanceDynamic::Create(GaugeMaterial, this);
		if (GaugeMaterialInstance)
		{
			GaugeImage->SetBrushFromMaterial(GaugeMaterialInstance);
		}
	}

	// 초기에는 테두리 숨김
	if (BorderImage)
	{
		BorderImage->SetVisibility(ESlateVisibility::Hidden);
	}

	// 위젯 바인딩 확인
	if (!IconImage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebuffIconWidget] IconImage is not bound!"));
	}
	if (!GaugeImage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebuffIconWidget] GaugeImage is not bound!"));
	}
	if (!BorderImage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebuffIconWidget] BorderImage is not bound!"));
	}
}

void UGS_DebuffIconWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 최적화: 비활성 상태거나 위젯이 보이지 않으면 모든 연산 스킵
	if (CurrentDebuffType == EDebuffType::None || GetVisibility() != ESlateVisibility::HitTestInvisible)
	{
		return;
	}

	// 잔여 시간 감소 (클라이언트 측 예측)
	float PrevRemainingTime = CurrentRemainingTime;
	CurrentRemainingTime = FMath::Max(0.f, CurrentRemainingTime - InDeltaTime);

	// CC기 펄스 애니메이션 (CC일 때만)
	if (bIsCrowdControl)
	{
		UpdatePulseAnimation(InDeltaTime);
	}

	// 끝날 때 깜빡임 (임계값 이하일 때만 처리)
	if (CurrentRemainingTime <= BlinkThreshold)
	{
		UpdateBlinkEffect(CurrentRemainingTime, InDeltaTime);
	}

	// 게이지 업데이트 (값이 실제로 변경되었을 때만 GPU로 전달)
	if (TotalDebuffDuration > 0.f)
	{
		// 퍼센트 변화가 0.5% 이상일 때만 머티리얼 업데이트 (성능 최적화)
		float NewPercent = CurrentRemainingTime / TotalDebuffDuration;
		float OldPercent = PrevRemainingTime / TotalDebuffDuration;
		if (FMath::Abs(NewPercent - OldPercent) >= 0.005f || CurrentRemainingTime <= 0.f)
		{
			UpdateGaugePercent(NewPercent);
		}
	}
}

void UGS_DebuffIconWidget::SetDebuffInfo(EDebuffType Type, float RemainingTime, float TotalDuration, bool bIsCC)
{
	CurrentDebuffType = Type;
	CurrentRemainingTime = RemainingTime;
	TotalDebuffDuration = TotalDuration;
	bIsCrowdControl = bIsCC;

	// 아이콘 텍스처 설정
	UpdateIconTexture(Type);

	// CC 스타일 적용
	ApplyCCStyle(bIsCC);

	// 게이지 초기화
	if (TotalDuration > 0.f)
	{
		UpdateGaugePercent(RemainingTime / TotalDuration);
	}

	// 깜빡임/펄스 초기화
	PulseTime = 0.f;
	BlinkTimer = 0.f;
	bBlinkState = true;

	// 투명도 초기화
	if (IconImage)
	{
		IconImage->SetRenderOpacity(1.f);
	}
}

void UGS_DebuffIconWidget::HideIcon()
{
	SetVisibility(ESlateVisibility::Collapsed);
	CurrentDebuffType = EDebuffType::None;
}

void UGS_DebuffIconWidget::ShowIcon()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UGS_DebuffIconWidget::UpdateRemainingTime(float NewRemainingTime, float TotalDuration)
{
	CurrentRemainingTime = NewRemainingTime;
	TotalDebuffDuration = TotalDuration;
}

void UGS_DebuffIconWidget::UpdateIconTexture(EDebuffType Type)
{
	if (!IconImage)
	{
		return;
	}

	// 텍스처 맵에서 찾기
	TSoftObjectPtr<UTexture2D>* TexturePtr = DebuffIconTextures.Find(Type);
	if (!TexturePtr)
	{
		// 해당 타입의 텍스처가 없음
		return;
	}

	// 이미 로드됨
	if (TexturePtr->IsValid())
	{
		IconImage->SetBrushFromTexture(TexturePtr->Get());
		return;
	}

	// Null이 아니면 비동기 로드 시도
	if (!TexturePtr->IsNull())
	{
		// Soft Reference의 경로를 가져와서 비동기 로드 요청
		FSoftObjectPath TexturePath = TexturePtr->ToSoftObjectPath();

		// 비동기 로딩 - 로드 완료 시 텍스처 적용
		FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
		Streamable.RequestAsyncLoad(TexturePath, FStreamableDelegate::CreateWeakLambda(this, [this, Type]()
		                                                                               {
			// 로드 완료 - 현재 표시 중인 디버프 타입일 때만 적용
			if (CurrentDebuffType == Type && IconImage)
			{
				TSoftObjectPtr<UTexture2D>* LoadedTexturePtr = DebuffIconTextures.Find(Type);
				if (LoadedTexturePtr && LoadedTexturePtr->IsValid())
				{
					IconImage->SetBrushFromTexture(LoadedTexturePtr->Get());
				}
			} }));
	}
}

void UGS_DebuffIconWidget::UpdateGaugePercent(float Percent)
{
	if (!GaugeMaterialInstance)
	{
		return;
	}

	// 머티리얼 파라미터 업데이트 (원형 게이지)
	float ClampedPercent = FMath::Clamp(Percent, 0.f, 1.f);

	// 캐싱된 값과 비교하여 변경 시에만 GPU 업데이트
	if (FMath::Abs(ClampedPercent - CachedGaugePercent) >= 0.005f)
	{
		CachedGaugePercent = ClampedPercent;
		GaugeMaterialInstance->SetScalarParameterValue(TEXT("Percent"), ClampedPercent);
	}
}

void UGS_DebuffIconWidget::ApplyCCStyle(bool bEnable)
{
	if (bEnable)
	{
		// 1.3배 크기
		SetRenderScale(FVector2D(CCScaleMultiplier, CCScaleMultiplier));

		// 빨간 테두리 표시
		if (BorderImage)
		{
			BorderImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			BorderImage->SetColorAndOpacity(FLinearColor(1.f, 0.2f, 0.2f, 1.f)); // 빨간색
		}
	}
	else
	{
		// 기본 크기
		SetRenderScale(FVector2D(1.f, 1.f));

		// 테두리 숨김
		if (BorderImage)
		{
			BorderImage->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UGS_DebuffIconWidget::UpdatePulseAnimation(float DeltaTime)
{
	PulseTime += DeltaTime;

	// 사인파로 펄스 효과
	float PulseScale = 1.f + (FMath::Sin(PulseTime * PulseSpeed) * PulseIntensity);
	float FinalScale = CCScaleMultiplier * PulseScale;

	SetRenderScale(FVector2D(FinalScale, FinalScale));
}

void UGS_DebuffIconWidget::UpdateBlinkEffect(float RemainingTime, float DeltaTime)
{
	// 임계값 이하일 때만 깜빡임
	if (RemainingTime > BlinkThreshold)
	{
		// 깜빡임 아님 - 이전에 깜빡이고 있었다면 복구
		if (!bBlinkState)
		{
			bBlinkState = true;
			if (IconImage)
			{
				IconImage->SetRenderOpacity(1.f);
			}
		}
		// 타이머 리셋
		BlinkTimer = 0.f;
		return;
	}

	// 깜빡임 타이머
	BlinkTimer += DeltaTime;
	float BlinkInterval = 0.2f; // 0.2초 간격

	// 남은 시간이 적을수록 빠르게 깜빡임
	if (RemainingTime < 1.f)
	{
		BlinkInterval = 0.1f;
	}

	if (BlinkTimer >= BlinkInterval)
	{
		BlinkTimer = 0.f;
		bBlinkState = !bBlinkState;

		// 상태가 변경될 때만 투명도 적용
		if (IconImage)
		{
			IconImage->SetRenderOpacity(bBlinkState ? 1.f : 0.5f);
		}
	}
}
