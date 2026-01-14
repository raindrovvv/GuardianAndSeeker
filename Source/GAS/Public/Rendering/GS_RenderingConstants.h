// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 렌더링 최적화 상수 정의
 * Distance Culling, LOD 등에 사용되는 전역 상수값
 */
namespace GS_Rendering
{
// ========================================
// Skeletal Mesh Culling Distances
// ========================================

/** SmallClaw 몬스터 컬링 거리 */
constexpr float MONSTER_SMALL_CULL_DISTANCE = 5000.0f;

/** NeedleFang 몬스터 컬링 거리 */
constexpr float MONSTER_MEDIUM_CULL_DISTANCE = 5000.0f;

/** ShadowFang 몬스터 컬링 거리 */
constexpr float MONSTER_LARGE_CULL_DISTANCE = 5000.0f;

/** 다른 플레이어(시커) 컬링 거리 (70m) - 몬스터보다 중요하므로 더 멀리 설정 */
constexpr float PLAYER_CULL_DISTANCE = 7000.0f;

// ========================================
// Static Mesh Culling Distances
// ========================================

/** 작은 함정 컬링 거리 */
constexpr float TRAP_SMALL_CULL_DISTANCE = 3000.0f;

/** 중간 함정 컬링 거리 */
constexpr float TRAP_MEDIUM_CULL_DISTANCE = 3500.0f;

/** 큰 함정 컬링 거리 */
constexpr float TRAP_LARGE_CULL_DISTANCE = 4500.0f;

/** 방 모듈 컬링 거리 - Draw Call 최적화를 위해 축소 */
constexpr float ROOM_CULL_DISTANCE = 8500.0f;

/** 벽 모듈 컬링 거리 - 방보다 앞서 컬링되거나 가시성 제어를 위해 분리 */
constexpr float WALL_CULL_DISTANCE = 9000.0f;

/** 폴리지 컬링 거리 (15m) - Nanite 미지원으로 인한 공격적 컬링 */
constexpr float FOLIAGE_CULL_DISTANCE = 1500.0f;

/** 라이트 컬링 거리 (20m) - 던전 조명 유지 */
constexpr float LIGHT_CULL_DISTANCE = 1600.0f;

/** 무기 컬링 거리 (30m) */
constexpr float WEAPON_CULL_DISTANCE = 3000.0f;

/** 투사체 컬링 거리 (150m) - 원거리 가시성 확보 (특히 RTS 모드) */
constexpr float PROJECTILE_CULL_DISTANCE = 15000.0f;

/** HP 위젯 기본 컬링 거리 (60m) */
constexpr float HP_WIDGET_CULL_DISTANCE = 6000.0f;

/** 스팀 닉네임 위젯 컬링 거리 (60m) */
constexpr float STEAM_NAME_WIDGET_CULL_DISTANCE = 6000.0f;

// ========================================
// Culling Optimization Settings
// ========================================

/** TPS 시점 컬링 거리 배율 */
constexpr float TPS_CULL_DISTANCE_SCALE = 1.0f;

/** RTS 시점 컬링 거리 배율 (시야와 성능 간 균형) */
constexpr float RTS_CULL_DISTANCE_SCALE = 2.5f;

/**
 * 현재 시점(RTS/TPS)에 맞는 최적의 컬링 거리를 계산합니다.
 * @param WorldContext 계산 기준이 되는 월드 컨텍스트
 * @param BaseDistance 기본 컬링 거리
 * @return 시점 배율이 적용된 최종 컬링 거리
 */
float CalculateCullDistance(const UObject* WorldContext, float BaseDistance);

/**
 * 현재 시점에 맞는 최소 LOD 단계를 계산합니다.
 * RTS 모드에서는 성능 확보를 위해 0단계(최고 품질) 사용을 제한할 수 있습니다.
 * @param WorldContext 계산 기준이 되는 월드 컨텍스트
 * @return 강제할 최소 LOD 단계 (0: 제한 없음, 1 이상: 하위 단계 고정)
 */
int32 CalculateMinLOD(const UObject* WorldContext);

/** 현재 로컬 플레이어가 RTS 모드인지 확인합니다. */
bool IsRTSMode(const UObject* WorldContext);

// ========================================
// LOD Optimization Settings
// ========================================

/** TPS 시점 최소 LOD (고품질 유지) */
constexpr int32 TPS_MIN_LOD = 0;

/** RTS 시점 최소 LOD (공격적인 성능 최적화 - 메시가 4단계 이상의 LOD를 가질 때
 * 효과적) */
constexpr int32 RTS_MIN_LOD = 3;

/**
 * Bounds Scale for smooth culling (prevent popping)
 * 컬링 경계에서의 팝인 현상 방지를 위한 바운드 스케일 (25% 여유)
 */
constexpr float DEFAULT_BOUNDS_SCALE = 1.25f;

// ========================================
// Network Update Frequency Optimization
// ========================================

/** 근거리 네트워크 업데이트 빈도 (60Hz - 높은 정확도 및 TPS 대응) */
constexpr float NET_UPDATE_FREQ_CLOSE = 60.0f;

/** 중거리 네트워크 업데이트 빈도 (30Hz - 전투 안정성/대역폭 균형) */
constexpr float NET_UPDATE_FREQ_MEDIUM = 30.0f;

/** 원거리 네트워크 업데이트 빈도 (15Hz - 최소한의 동기화) */
constexpr float NET_UPDATE_FREQ_FAR = 15.0f;

/** 전투 중 최소 네트워크 업데이트 빈도 (60Hz - 동기화 품질 보장) */
constexpr float NET_UPDATE_FREQ_COMBAT = 60.0f;

/** 최소 네트워크 업데이트 빈도 */
constexpr float NET_UPDATE_FREQ_MIN = 5.0f;

/** 근거리 임계값 (80m - 전투 범위 확대) */
constexpr float NET_DISTANCE_CLOSE = 8000.0f;

/** 중거리 임계값 (150m - RTS 시야 고려) */
constexpr float NET_DISTANCE_MEDIUM = 15000.0f;

// ========================================
// Timer Optimization Settings (Adaptive)
// ========================================

/** 중요도가 높을 때 업데이트 간격 (0.1s) */
constexpr float TIMER_INTERVAL_HIGH = 0.1f;

/** 중요도가 낮을 때 업데이트 간격 (0.5s) */
constexpr float TIMER_INTERVAL_LOW = 0.5f;

/** 중요도가 매우 낮을 때 업데이트 간격 (1.0s) */
constexpr float TIMER_INTERVAL_MIN = 1.0f;

/**
 * 로컬 플레이어와의 거리에 따라 최적의 네트워크 업데이트 빈도를 계산합니다.
 * @param WorldContext 계산 기준이 되는 월드 컨텍스트
 * @param ActorLocation 액터의 위치
 * @return 거리 기반 최적 네트워크 업데이트 빈도
 */
float CalculateNetUpdateFrequency(const UObject* WorldContext,
                                  const FVector& ActorLocation);

// ========================================
// Shadow Casting Distance Optimization
// ========================================

/** 그림자 완전 비활성화 거리 (50m - 공격적 최적화) */
constexpr float SHADOW_DISABLE_DISTANCE = 5000.0f;

/** 동적 그림자 비활성화 거리 (25m - 정적 그림자만 유지) */
constexpr float DYNAMIC_SHADOW_DISABLE_DISTANCE = 2500.0f;

/** 몬스터 그림자 비활성화 거리 (35m - 던전 전투 범위 고려) */
constexpr float MONSTER_SHADOW_DISTANCE = 3500.0f;

// ========================================
// VFX Optimization (Niagara)
// ========================================

/** VFX 완전 비활성화 거리 (60m) */
constexpr float VFX_DISABLE_DISTANCE = 6000.0f;

/** VFX 저품질 전환 거리 (30m - 파티클 수 50% 감소) */
constexpr float VFX_LOW_QUALITY_DISTANCE = 3000.0f;

/** 혈흔 이펙트 최대 재생 거리 (20m) */
constexpr float BLOOD_VFX_MAX_DISTANCE = 2000.0f;

/** VFX 중품질 전환 거리 (15m - 파티클 수 75% 유지) */
constexpr float VFX_MEDIUM_QUALITY_DISTANCE = 1500.0f;

/** 저품질 VFX 파티클 배율 */
constexpr float VFX_LOW_QUALITY_SCALE = 0.5f;

/** 중품질 VFX 파티클 배율 */
constexpr float VFX_MEDIUM_QUALITY_SCALE = 0.75f;

// ========================================
// Combat Detection Settings
// ========================================

/** Seeker 전투 트리거 반경 기본값 (8m) - Monster HP 위젯 가시성 등에서 공유 */
constexpr float DEFAULT_COMBAT_TRIGGER_RADIUS = 800.0f;

/** TPS 모드에서 HP 위젯이 보이기 시작하는 최대 거리 (15m) */
constexpr float TPS_HP_WIDGET_VISIBLE_DISTANCE = 1500.0f;

/** HP 위젯 가시성 트레이스를 위한 허리 높이 오프셋 */
constexpr float HP_WIDGET_VISIBILITY_TRACE_OFFSET = 50.0f;

/** Significance Manager: UI 활성화 임계값 */
constexpr float SIGNIFICANCE_THRESHOLD_UI = 0.4f;

/** Significance Manager: UI 가시성 연산 스킵 임계값 */
constexpr float SIGNIFICANCE_THRESHOLD_UI_SKIP = 0.1f;

/** Significance Manager: 에셋 로딩 최소 임계값 (0.2 이하면 로드 스킵) */
constexpr float SIGNIFICANCE_THRESHOLD_ASYNC_LOAD = 0.2f;

/** Significance Manager: 중요도에 따른 타이머 주기 계산 */
inline float GetAdaptiveTimerInterval(float Significance)
{
	if (Significance > 0.5f)
		return TIMER_INTERVAL_HIGH;
	if (Significance > 0.1f)
		return TIMER_INTERVAL_LOW;
	return TIMER_INTERVAL_MIN;
}

// ========================================
// AI Perception Distance Optimization
// ========================================

/** TPS 모드 AI 인지 거리 (50m - 제한적) */
constexpr float AI_PERCEPTION_DISTANCE_TPS = 5000.0f;

/** RTS 모드 AI 인지 거리 (150m - 전략적 시야) */
constexpr float AI_PERCEPTION_DISTANCE_RTS = 15000.0f;

/**
 * 현재 시점에 맞는 AI 인지 거리를 계산(최적화)
 * @param WorldContext 계산 기준이 되는 월드 컨텍스트
 * @return 시점 기반 AI 인지 거리
 */
float CalculateAIPerceptionDistance(const UObject* WorldContext);

/**
 * 거리에 따라 그림자 캐스팅 여부를 업데이트(최적화)
 * @param Actor 대상 액터
 * @param MeshComp 대상 메시 컴포넌트
 */
void UpdateShadowCulling(const AActor* Actor, USceneComponent* MeshComp);
} // namespace GS_Rendering
