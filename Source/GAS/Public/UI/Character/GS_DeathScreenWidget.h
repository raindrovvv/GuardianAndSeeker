// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Character/GS_Character.h"
#include "GS_DeathScreenWidget.generated.h"

class UTextBlock;
class UImage;
class AGS_Seeker;

/**
 * 사망 화면 UI 위젯
 * - 죽었습니다 타이틀 표시
 * - 킬러 정보 표시 (이름, 타입)
 * - 페이드 인 애니메이션 지원
 */
UCLASS()
class GAS_API UGS_DeathScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	/** 사망 화면 초기화 및 표시 */
	UFUNCTION(BlueprintCallable, Category = "DeathScreen")
	void ShowDeathScreen(const FLastKillerInfo& KillerInfo);

	/** 사망 화면 숨김 */
	UFUNCTION(BlueprintCallable, Category = "DeathScreen")
	void HideDeathScreen();

protected:
	// =========================================
	// UI 바인딩
	// =========================================

	/** 메인 타이틀 텍스트 ("죽었습니다") */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* DeathTitleText;

	/** 킬러 정보 텍스트 ("IronFang에게 처치당함") */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* KillerInfoText;

	/** 킬러 타입 아이콘 (선택사항) */
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* KillerTypeIcon;

	// =========================================
	// 설정
	// =========================================

	/** 메인 타이틀 텍스트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathScreen|Text")
	FText DeathTitleDefault = FText::FromString(TEXT("죽었습니다"));

	/** 킬러 정보 포맷 ({0}에 킬러 이름이 들어감) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathScreen|Text")
	FText KillerInfoFormat = FText::FromString(TEXT("{0}에게 처치당함"));

	/** 알 수 없는 킬러 텍스트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathScreen|Text")
	FText UnknownKillerText = FText::FromString(TEXT("???"));

	// =========================================
	// 타입별 색상
	// =========================================

	/** 몬스터 킬러 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathScreen|Colors")
	FLinearColor MonsterColor = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);

	/** 시커(플레이어/AI) 킬러 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathScreen|Colors")
	FLinearColor SeekerColor = FLinearColor(0.3f, 0.6f, 1.0f, 1.0f);

	/** 가디언 킬러 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathScreen|Colors")
	FLinearColor GuardianColor = FLinearColor(0.8f, 0.4f, 0.8f, 1.0f);

	/** 기본 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathScreen|Colors")
	FLinearColor DefaultColor = FLinearColor::White;

	// =========================================
	// 애니메이션
	// =========================================

	/** 페이드 인 애니메이션 (블루프린트에서 설정) */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UWidgetAnimation* FadeInAnimation;

private:
	/** 킬러 타입에 따른 색상 반환 */
	FLinearColor GetColorForKillerType(ECharacterType KillerType) const;

	/** 킬러 타입이 몬스터인지 확인 */
	bool IsMonsterType(ECharacterType Type) const;

	/** 킬러 타입이 시커인지 확인 */
	bool IsSeekerType(ECharacterType Type) const;

	/** 킬러 타입이 가디언인지 확인 */
	bool IsGuardianType(ECharacterType Type) const;
};
