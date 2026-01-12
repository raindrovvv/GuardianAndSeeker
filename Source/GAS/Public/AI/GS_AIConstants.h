// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

/**
 * AI 관련 공통 상수 정의
 * 
 * AI 시스템 전반에서 사용되는 거리, 시간, 임계값 등의 상수를 정의합니다.
 * 매직 넘버를 줄이고 일관성 있는 값 관리를 위해 사용됩니다.
 */
namespace GS_AI
{
// ========================================
// 마커(Marker) 시스템 관련 상수
// ========================================

/** AI 탐험 마커 배치 쿨다운 (초) */
static constexpr float MARKER_PLACEMENT_COOLDOWN = 60.0f;

/** AI 탐험 마커 배치 전방 거리 (cm) */
static constexpr float MARKER_PLACEMENT_FORWARD_DIST = 200.0f;

// ========================================
// 이동 및 성능(Movement & Performance) 관련 상수
// ========================================

/** 성능 LOD: 먼 거리 (cm) */
static constexpr float LOD_DISTANCE_FAR = 10000.0f;

/** 성능 LOD: 중간 거리 (cm) */
static constexpr float LOD_DISTANCE_MID = 5000.0f;

/** 몬스터 중첩 체크 간격 (초) */
static constexpr float MONSTER_OVERLAP_INTERVAL = 0.5f;

/** 몬스터 중첩 체크 반경 (cm) */
static constexpr float MONSTER_OVERLAP_RADIUS = 150.0f;

/** 트랩 감지 체크 간격 (초) */
static constexpr float TRAP_CHECK_INTERVAL = 0.2f;

/** 트랩 위에서의 끼임 판정 시간 (초) */
static constexpr float STUCK_THRESHOLD_ON_TRAP = 1.0f;

/** 일반적인 끼임 판정 시간 (초) */
static constexpr float STUCK_THRESHOLD_NORMAL = 3.0f;

// ========================================
// 퍼셉션(Perception) 관련 상수
// ========================================

/** 기본 시야 반경 (cm) */
static constexpr float DEFAULT_SIGHT_RADIUS = 2000.0f;

/** 기본 시야 상실 반경 (cm) */
static constexpr float DEFAULT_LOSE_SIGHT_RADIUS = 2500.0f;

/** 마지막 인지 위치에서의 자동 감지 성공 범위 (cm) */
static constexpr float AUTO_SUCCESS_RANGE_FROM_LAST_SEEN = 500.0f;

// ========================================
// 전투 및 탐험(Combat & Exploration) 설정
// ========================================

/** 기본 근접 공격 사거리 (cm) */
static constexpr float DEFAULT_MELEE_RANGE = 200.0f;

/** 기본 탐험 반경 (cm) */
static constexpr float DEFAULT_EXPLORATION_RADIUS = 3000.0f;

/** 최소 탐험 거리 (cm) */
static constexpr float MIN_EXPLORATION_DISTANCE = 500.0f;

/** 방문한 위치 판정 반경 (cm) */
static constexpr float VISITED_LOCATION_RADIUS = 150.0f;

// ========================================
// 유틸리티 AI 점수(Utility Score) 상수
// ========================================

/** 힐(Heal) 유틸리티 임계값 */
static constexpr float HEAL_THRESHOLD_DEFAULT = 0.3f;
static constexpr float HEAL_THRESHOLD_CRITICAL = 0.15f;

/** 유틸리티 점수 계산 공통 */
static constexpr float UTILITY_SCORE_MAX_REVIVE = 3.0f;
static constexpr float UTILITY_SCORE_MAX_COMBAT = 2.2f;
static constexpr float UTILITY_SCORE_MAX_HEAL = 3.0f;
static constexpr float UTILITY_SCORE_MAX_EVADE = 3.5f;
static constexpr float UTILITY_SCORE_MAX_TACTICAL = 2.5f;

/** 탐험(Explore) 유틸리티 */
static constexpr float EXPLORE_UTILITY_BASE = 0.9f;
static constexpr float EXPLORE_UTILITY_REACHED_GOAL = 0.7f;
static constexpr float EXPLORE_UTILITY_ALLY_IN_COMBAT_PENALTY_THREAT = 0.3f;
static constexpr float EXPLORE_UTILITY_ALLY_IN_COMBAT_PENALTY_NO_THREAT = 0.6f;
static constexpr float EXPLORE_UTILITY_TARGET_BONUS = 0.3f;

/** 전투(Combat) 유틸리티 */
static constexpr float COMBAT_UTILITY_BASE = 1.5f;
static constexpr float COMBAT_UTILITY_THREAT_SCALE = 1.33f;
static constexpr float COMBAT_UTILITY_MERCI_OPTIMAL_BONUS = 0.5f;
static constexpr float COMBAT_UTILITY_MERCI_CLOSE_PENALTY = -0.3f;
static constexpr float COMBAT_UTILITY_MELEE_TRAP_PENALTY = -2.0f;

/** 회피(Evade) 유틸리티 */
static constexpr float EVADE_UTILITY_STATIONARY_THRESHOLD = 1.5f;
static constexpr float EVADE_UTILITY_STATIONARY_SCALE = 1.5f;
static constexpr float EVADE_UTILITY_RANGED_THREAT_UNIT = 0.3f;
static constexpr float EVADE_UTILITY_EMERGENCY_BONUS = 0.5f;

/** 회피(Evade) 도착 판정 및 허용 오차 (cm) */
static constexpr float EVADE_ARRIVAL_THRESHOLD = 50.0f;
static constexpr float EVADE_ACCEPTANCE_RADIUS = 100.0f;

/** 회피 동작 타임아웃 (초) */
static constexpr float EVADE_TASK_TIMEOUT = 5.0f;

/** 구조(Revive) 유틸리티 */
static constexpr float REVIVE_UTILITY_HEALTH_MIN_REQUIRED = 0.3f;
static constexpr float REVIVE_UTILITY_DISTANCE_MAX = 2000.0f;
static constexpr float REVIVE_UTILITY_URGENCY_TIME_LIMIT = 8.0f;
static constexpr float REVIVE_UTILITY_COMBAT_PENALTY = 0.5f;
static constexpr float REVIVE_UTILITY_THREAT_PROXIMITY_PENALTY = 0.7f;
static constexpr float REVIVE_UTILITY_THREAT_DISTANCE_LIMIT = 800.0f;

/** 전술(Tactical) 유틸리티 */
static constexpr float TACTICAL_UTILITY_MERCI_AGGRO_BONUS = 1.5f;
static constexpr float TACTICAL_UTILITY_MELEE_TRAP_BONUS = 2.0f;
static constexpr float TACTICAL_UTILITY_STATIONARY_BONUS = 0.5f;
static constexpr float TACTICAL_UTILITY_STATIONARY_TIME_LIMIT = 3.0f;

// ========================================
// 위협 평가(Threat Assessment) 상수
// ========================================

/** 시야선 없음 시 위협 감쇄 (가까울 때) */
static constexpr float NO_LOS_THREAT_MULTIPLIER_NEAR = 0.7f;

/** 시야선 없음 시 위협 감쇄 (멀 때, 800 이상) */
static constexpr float NO_LOS_THREAT_MULTIPLIER_FAR = 0.1f;

/** 시야선 체크 거리 임계값 (cm) */
static constexpr float LOS_DISTANCE_THRESHOLD = 800.0f;
} // namespace GS_AI
