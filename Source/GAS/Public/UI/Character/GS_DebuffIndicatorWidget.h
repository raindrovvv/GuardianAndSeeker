// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Character/Debuff/EDebuffType.h"
#include "Character/Component/GS_DebuffComp.h" // FDebuffRepInfo 완전한 정의 필요
#include "GS_DebuffIndicatorWidget.generated.h"

class UHorizontalBox;
class UGS_DebuffIconWidget;

/**
 * 디버프 아이콘을 표시하는 컨테이너 위젯
 * 
 * 캐릭터 머리 위에 표시되며, 최대 4개의 디버프 아이콘을 
 * 수평으로 나열하여 보여줍니다.
 * 
 * 주요 기능:
 * - 아이콘 풀링으로 재사용
 * - CC기는 크게 + 빨간 테두리 + 펄스 애니메이션
 * - 원형 게이지로 잔여 시간 표시
 */
UCLASS()
class GAS_API UGS_DebuffIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGS_DebuffIndicatorWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/**
	 * 디버프 목록 업데이트
	 * @param DebuffList 표시할 디버프 정보 배열 (이미 우선순위 정렬됨)
	 * @param MaxIcons 최대 표시 개수
	 */
	UFUNCTION(BlueprintCallable, Category = "Debuff Indicator")
	void UpdateDebuffIcons(const TArray<FDebuffRepInfo>& DebuffList, int32 MaxIcons = 4);

	/** 모든 아이콘 숨기기 */
	UFUNCTION(BlueprintCallable, Category = "Debuff Indicator")
	void HideAllIcons();

protected:
	/** 아이콘을 담는 수평 박스 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> IconContainer;

	/** 개별 아이콘 위젯 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff Indicator|UI")
	TSubclassOf<UGS_DebuffIconWidget> DebuffIconWidgetClass;

	/** 아이콘 간 간격 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff Indicator|Settings")
	float IconSpacing = 4.f;

private:
	/** 아이콘 위젯 풀 (재사용) */
	UPROPERTY()
	TArray<TObjectPtr<UGS_DebuffIconWidget>> IconPool;

	/** 현재 활성화된 아이콘 수 */
	int32 ActiveIconCount = 0;

	/** 풀에서 아이콘 가져오기 (없으면 생성) */
	UGS_DebuffIconWidget* GetOrCreateIcon(int32 Index);

	/** 디버프 타입이 CC기인지 확인 */
	bool IsCrowdControlDebuff(EDebuffType Type) const;
};
