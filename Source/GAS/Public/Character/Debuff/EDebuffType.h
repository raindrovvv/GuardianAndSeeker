#pragma once

#include "CoreMinimal.h"
#include "EDebuffType.generated.h"

UENUM(BlueprintType)
enum class EDebuffType : uint8
{
	None UMETA(DisplayName = "None"),
	Stun UMETA(DisplayName = "Stun"),
	Aggro UMETA(DisplayName = "Aggro"),
	Obscure UMETA(DisplayName = "Obscure"),
	Confuse UMETA(DisplayName = "Confuse"),
	Mute UMETA(DisplayName = "Mute"),
	Slow UMETA(DisplayName = "Slow"),
	Burn UMETA(DisplayName = "Burn"),
	Bleed UMETA(DisplayName = "Bleed"),
	Lava UMETA(DisplayName = "Lava")
};

/**
 * 디버프 타입 관련 유틸리티 함수 모음
 * 
 * CC 타입 체크, 표시 우선순위 등 디버프 관련 공통 로직을 중앙 집중화
 */
struct FDebuffTypeUtils
{
	/**
	 * 해당 디버프가 군중제어(CC) 효과인지 확인
	 * CC: Stun, Confuse, Mute (행동 불능/제한 효과)
	 */
	FORCEINLINE static bool IsCrowdControl(EDebuffType Type)
	{
		switch (Type)
		{
		case EDebuffType::Stun:
		case EDebuffType::Confuse:
		case EDebuffType::Mute:
			return true;
		default:
			return false;
		}
	}

	/**
	 * 디버프 표시 우선순위 반환
	 * 숫자가 낮을수록 높은 우선순위 (먼저 표시)
	 * CC기 > 이동 방해 > DoT > 기타
	 */
	FORCEINLINE static int32 GetDisplayPriority(EDebuffType Type)
	{
		switch (Type)
		{
		case EDebuffType::Stun:
			return 1; // 최우선 - CC (완전 행동 불가)
		case EDebuffType::Confuse:
			return 2; // CC (조작 역전)
		case EDebuffType::Mute:
			return 3; // CC (스킬 사용 불가)
		case EDebuffType::Slow:
			return 4; // 이동 방해
		case EDebuffType::Burn:
			return 5; // DoT
		case EDebuffType::Bleed:
			return 6; // DoT
		case EDebuffType::Lava:
			return 7; // 환경 DoT
		case EDebuffType::Aggro:
			return 8; // 어그로
		case EDebuffType::Obscure:
			return 9; // 시야 방해
		default:
			return 99; // 알 수 없는 타입
		}
	}
};
