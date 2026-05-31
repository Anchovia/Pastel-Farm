# DESIGN — Pastel Farm (Game Design Document)

> 이 문서는 **게임으로서 Pastel Farm이 무엇인지**(경험·비전·기둥·스코프)를 정의한다.
> 엔진 구조는 `ARCHITECTURE.md`, 변경 이력은 `DEVLOG.md`, 빌드·기능 스냅샷은 `README.md`.
>
> **갱신 시점:** 디자인 방향이 바뀔 때만(거의 안정적으로 유지). 프로젝트 방향을 흔들리지 않게 잡는 닻 역할.

---

## High Concept

Pastel Farm은 커스텀 Vulkan 엔진 위에 만드는 **스타일라이즈드 로우폴리 농사·라이프심** 게임이다.

> 기억에 남는 스타일라이즈드 세계에 살면서, 농사와 상호작용으로 땅을 가꾸고,
> "authored이면서 살아있는" 세계를 경험한다.

지향하지 **않는** 것:
- 무한 샌드박스 / 절차 생존 게임 / 마인크래프트식 탐험 게임

지향하는 것:
- 고정된 세계 정체성 · 분위기 · 농사 루프 · 세계 지속성 · 가벼운 서사 · 코지 진행

---

## Core Design Pillars

### 1. Remembered World (기억되는 세계)
세계 지리가 기억에 남아야 한다 — 숲·강·마을 랜드마크·자원 위치·길. 세계가 **authored**로 느껴질 것.

### 2. Stylized Atmosphere (스타일라이즈드 분위기)
사실성보다 비주얼 정체성. 로우폴리 / 플랫 셰이딩 / grounded lighting / 따뜻한 분위기 / 읽히는 실루엣 / 강한 구도. 레퍼런스: 코지한 스타일라이즈드 아이소메트릭 월드.

### 3. Low-Spec Friendly (저사양 친화)
성능은 디자인 기둥이다. 스펙터클용 비싼 렌더링 회피, 똑똑한 조명·구도·효율적 지오메트리·확장 가능한 렌더링 선호.

### 4. Living World (살아있는 세계)
정적이지 않게 — day/night, 재생 자원, 식생 변화, wildlife, 계절 활동, 모션·앰비언스.
> 목표: **기억되는 세계 + 살아있는 세계**

---

## World Philosophy

**방향 (결정됨): Fixed World + Procedural Layer**

| 영구(Permanent) | 가변(Variable) |
|---|---|
| 지리·랜드마크·길·절벽·마을·authored 공간 | 자원·식생·wildlife·forage·계절 변화 |

서사 · 리플레이성 · 세계 기억성 · 살아있는 시스템의 균형.

> 참고: 현재 엔진 구현은 절차 생성 기반이며(스트리밍 청크), 고정맵은 맵 데이터/콘텐츠 준비 후 단계적으로 전환한다. (`ARCHITECTURE.md` 월드 모델 참조)

---

## World Structure (장기 모델)

```
World
├─ Terrain    지형·고도·절벽·고정 베이스 (voxel)
├─ TileState  가변 타일 상태: tilled / watered / fertility / moisture → 농경지
├─ StaticProp 환경물: tree / rock / bush / flower / fence (대부분 정적, 인스턴스)
├─ Crop       살아있는 농작물: stage / time / growth
└─ Entity     시뮬레이션: player / animal / NPC / tractor (이동·AI·상태·애니)
```

> 현재 구현 상태: Terrain·TileState·StaticProp(나무)·save/load 존재. 작물은 임시로 voxel 타일(`WHEAT`)로 처리 중이며, 장기적으로 `Crop` 레이어로 분리 검토.

---

## Gameplay Loop (장기)

```
Explore → Gather → Farm/Build → Progress → Unlock → Return to world
```

지향: 차분함 · 시스템적 · 반복 가능 · 보람. 회피: 전투 중심 · 속도 중심 · 생존 압박.

---

## Farming Direction
땅 준비 → 경작(tilling) → 심기 → 물주기 → 성장 → 수확.
탐험·자원 수집·제작·진행과 자연스럽게 연결.

## Resource Philosophy
자원은 재생한다. **영구 세계 + 지속 변경 + 재생 자원 레이어**. (돌 재생 / 계절 forage / wildlife 이동 / 식생 변화) → 세계가 고갈되지 않음.

## Visual Direction
타깃: **스타일라이즈드 grounded 로우폴리**. 비주얼 퀄리티는 조명·그림자·분위기·구도·scene dressing·color grading에서 나온다. 사실성·PBR·AAA 충실도 아님.

---

## Long-Term Feature Direction
작물 · 날씨 · 계절 · 동물 · tractor/도구 · 마을/NPC · 가벼운 서사 · 환경 스토리텔링.
> 모든 기능이 확정은 아니며 스코프는 의도적으로 통제한다.

## MVP Direction
초기 플레이 가능 버전 우선순위: **농사 · 고정/안정 맵 · save/load · 오브젝트 상호작용 · 분위기 · 기초 진행**.
이후에: 거대한 세계 · 대량 콘텐츠 · 고급 시뮬레이션.

---

## Final Design Statement

> 커스텀 엔진 위에, authored 지리와 살아있는 시스템이 공존하는,
> **스타일라이즈드하고 기억에 남는 저사양 농사 세계.**
