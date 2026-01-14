// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GS_HitIndicatorComponent.generated.h"

class UGS_HitIndicatorWidget;

/**
 * 피격 방향을 나타내는 6방향 열거형
 * 상하 공격은 수평 공격과 다른 시각적 표현 사용
 */
UENUM(BlueprintType)
enum class EHitDirection : uint8
{
	None,
	Front,
	Back,
	Left,
	Right,
	Up, // 위에서 공격 (낙석, 천장 함정 등)
	Down, // 아래에서 공격 (바닥 함정 등)
	Omni // 전 방향 (도트 데미지, 장판 등 방향 특정 불가)
};

/**
 * 피격 방향 정보를 전달하는 델리게이트
 * @param Direction 피격 방향
 * @param DamageAmount 데미지 양 (인디케이터 강도 결정에 사용)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnDamageDirectionReceived,
    EHitDirection, Direction,
    float, DamageAmount);

/**
 * 피격 방향 HUD 표시를 담당하는 컴포넌트
 * 
 * TakeDamage에서 HitDirection을 받아 카메라 기준 상대 방향으로 변환 후
 * UI 위젯에 브로드캐스트합니다.
 * 
 * 로컬 플레이어만 HUD를 표시하므로, AI 시커에서는 비활성화됩니다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GAS_API UGS_HitIndicatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGS_HitIndicatorComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 데미지 방향 수신 시 발생하는 이벤트
	 * UI 위젯에서 바인딩하여 인디케이터 표시
	 */
	UPROPERTY(BlueprintAssignable, Category = "Hit Indicator")
	FOnDamageDirectionReceived OnDamageDirectionReceived;

	/**
	 * 피격 방향 알림 (TakeDamage에서 호출)
	 * 서버에서 호출 시 클라이언트로 RPC 전송
	 * 
	 * @param WorldHitDirection 월드 좌표계 피격 방향 (데미지 소스 → 플레이어)
	 * @param DamageAmount 데미지 양
	 */
	UFUNCTION(BlueprintCallable, Category = "Hit Indicator")
	void NotifyDamageDirection(const FVector& WorldHitDirection, float DamageAmount);

protected:
	/** 클라이언트 RPC - 소유 클라이언트에서 인디케이터 표시 */
	UFUNCTION(Client, Unreliable)
	void Client_ShowHitIndicator(const FVector& WorldHitDirection, float DamageAmount);

	/**
	 * HitIndicator 위젯 클래스 (블루프린트에서 설정)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Indicator|UI")
	TSubclassOf<UGS_HitIndicatorWidget> HitIndicatorWidgetClass;

	/**
	 * 월드 방향을 카메라 기준 상대 방향으로 변환
	 * 
	 * @param WorldHitDirection 월드 좌표계 피격 방향
	 * @return 카메라 기준 6방향 중 하나
	 */
	UFUNCTION(BlueprintCallable, Category = "Hit Indicator")
	EHitDirection CalculateHitDirectionFromCamera(const FVector& WorldHitDirection) const;

	/**
	 * 내적값을 기반으로 6방향 판별
	 * 상하 방향 우선 체크 후 수평 방향 체크
	 * 
	 * @param ForwardDot 카메라 Forward와의 내적
	 * @param RightDot 카메라 Right와의 내적  
	 * @param UpDot 카메라 Up과의 내적
	 * @return 판별된 방향
	 */
	EHitDirection DetermineDirection(float ForwardDot, float RightDot, float UpDot) const;

private:
	/** 상하 방향 판별 임계값 (이 값 이상이면 상하로 판정) */
	UPROPERTY(EditDefaultsOnly, Category = "Hit Indicator|Settings")
	float VerticalThreshold = 0.5f;

	/** 현재 표시 중인 HitIndicator 위젯 */
	UPROPERTY()
	TObjectPtr<UGS_HitIndicatorWidget> HitIndicatorWidget;

	/** 위젯 생성 및 화면에 추가 */
	void CreateHitIndicatorWidget();

	/** 실제 인디케이터 표시 처리 (클라이언트에서 실행) */
	void ShowHitIndicatorInternal(const FVector& WorldHitDirection, float DamageAmount);
};
