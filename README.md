# Guardian & Seeker - Technical Audio Notes

4v1 비대칭 PvP 프로젝트 'Guardian & Seeker'에서 전투의 흐름을 읽는 오디오 신호, 공간 위협 탐지, 그리고 직관적인 UI 피드백을 구현하기 위해 설계된 기술적 솔루션을 정리한 문서입니다.

## 주요 구현 사항

이 프로젝트에는 Unreal Engine 5의 게임플레이 상태를 Wwise Event 및 RTPC와 정교하게 연결하는 C++ 기반 오디오 컴포넌트 레이어가 설계되어 있습니다.

**주요 파일:**

- `Source/GAS/Public/Sound/GS_AudioComponentBase.h`
- `Source/GAS/Public/Sound/GS_AudioManager.h`
- `Source/GAS/Public/Sound/GS_MonsterAudioComponent.h`
- `Source/GAS/Public/Sound/GS_SeekerAudioComponent.h`
- `Source/GAS/Public/Sound/GS_UIAudioSystem.h`

## 핵심 아키텍처

`UGS_AudioComponentBase`는 모든 게임플레이 오디오 로직의 근간이 되는 `UActorComponent` 확장 클래스입니다. 거리(Distance), 감쇠 모드(Attenuation Mode), 차폐(Occlusion) 제어에 필요한 Wwise RTPC 참조를 통합 관리하며, 이를 상속받는 몬스터 및 시커 오디오 컴포넌트가 확장 가능한 구조를 제공합니다.

**역할별 오디오 시스템:**

- **Monster Audio:** 위협 신호 강화, 공간적 거리감 및 차폐감 구현을 통한 인지 기능 극대화
- **Seeker Audio:** 플레이어 행동에 따른 즉각적인 피드백, 이동 및 상호작용 타이밍 최적화
- **UI Audio:** 치열한 전투와 BGM 환경에서도 명확하게 전달되는 2D 인터랙션 피드백

## Wwise / UE5 파이프라인

- **구조화된 관리:** Wwise Event, Switch, RTPC, Container를 게임플레이 목적에 맞춰 체계적으로 분류했습니다.
- **상태 기반 호출:** 오디오가 단순 연출을 넘어, C++ 및 Blueprint의 실제 게임 스테이트와 연동되어 호출되도록 설계했습니다.
- **실전 검증:** Physical Material 기반의 적응형 발소리, Anim Notify 정밀 제어, 오클루전 시스템을 실제 플레이 세션을 통해 검증하고 최적화했습니다.
- **피드백 루프:** 플레이테스트 피드백을 바탕으로 우선순위 재설정, 믹스 밸런스 조정, 시스템 개선 작업을 거쳐 완성도를 높였습니다.

## 프로젝트 성과

- **G-STAR 2025** 출품 프로젝트
- **299명**의 대규모 플레이테스트 수행
- 유저 만족도 평균 평점 **4.36 / 5** 달성
- 캐릭터, 몬스터, 환경, UI/System을 망라하는 **191개**의 오디오 에셋 구축 및 관리

## 주요 링크

- **게임플레이 시연 영상:** https://www.youtube.com/watch?v=MGSCKdCAgDY&t=19s
- **사운드 디자인 & 구현 쇼케이스:** https://www.youtube.com/watch?v=d7xon5fv-kg

<details>
<summary>English version</summary>

## Guardian & Seeker - Technical Audio Notes

4v1 asymmetrical PvP project focused on readable combat, spatial threat cues, and responsive feedback audio.

### Audio Implementation Focus

This project includes a C++ audio component layer that connects Unreal Engine 5 gameplay state with Wwise Events and RTPCs.

Key files:

- `Source/GAS/Public/Sound/GS_AudioComponentBase.h`
- `Source/GAS/Public/Sound/GS_AudioManager.h`
- `Source/GAS/Public/Sound/GS_MonsterAudioComponent.h`
- `Source/GAS/Public/Sound/GS_SeekerAudioComponent.h`
- `Source/GAS/Public/Sound/GS_UIAudioSystem.h`

### Core Structure

`UGS_AudioComponentBase` is a shared `UActorComponent` base for gameplay audio behavior. It manages Wwise RTPC references such as distance, attenuation mode, and occlusion control, then exposes a reusable structure for monster and seeker audio components.

Derived systems separate audio behavior by gameplay role:

- Monster audio: threat cues, distance, obstruction, and spatial readability
- Seeker audio: player-facing feedback, movement, and interaction timing
- UI audio: short 2D feedback that stays readable over combat and BGM

### Wwise / UE5 Workflow

- Wwise Events, Switches, RTPCs, and containers are organized by gameplay purpose.
- UE5 C++ / Blueprint logic triggers audio from real gameplay state instead of decorative playback.
- Physical Material footsteps, Anim Notify timing, and occlusion behavior are verified inside play sessions.
- Playtest feedback is translated into priority, mix, and system-level tasks.

### Evidence

- G-STAR 2025 project
- 299 player test participants
- 4.36 / 5 average rating
- 191 audio assets organized across character, monster, environment, and UI/System categories

### Portfolio Links

- Demo reel: https://www.youtube.com/watch?v=d7xon5fv-kg
- Audio debugging video: https://www.youtube.com/watch?v=dp_Kr3LQ6EE

</details>
