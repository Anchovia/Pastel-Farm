# ARCHITECTURE — Pastel Farm Engine

> 이 문서는 **엔진이 어떻게 구성돼 있고 왜 그렇게 했는지 + 기술적 장기 방향**을 다룬다.
> 게임 기획은 `DESIGN.md`, 변경 이력은 `DEVLOG.md`, 기능 스냅샷·빌드는 `README.md`.
>
> **갱신 시점:** 구조가 바뀔 때만(새 시스템 / 리팩토링 / 파이프라인 변경). 매 작업마다 갱신하지 않는다.
> **태그:** `[구현됨]` = 현재 코드에 존재 · `[계획]` = 합의된 방향(아직 구현 안 됨).

---

## 설계 철학

- 로우폴리 / 플랫 셰이딩 / 스타일라이즈드 — 포토리얼·PBR 안 함
- 저사양 친화 (GTX 750 Ti~1050급 / 통합 GPU 고려), 낮은 CPU 오버헤드, 렌더링 낭비 최소
- Vulkan 직접 제어, 게임과 엔진 동시 개발
- 렌더러 원칙: **단순함 + 명시적 제어 + 유지보수성**

---

## 모듈 구조 [구현됨]

```
src/
├─ platform/   Window(GLFW RAII), InputManager (입력 폴링 일원화)
├─ game/       Camera, Player, GameState (이동·충돌·시간·농사 규칙)
├─ world/      Chunk, World(청크 라우팅·save/load), TerrainGen(FBM)
└─ renderer/   VulkanContext(_Init/_Frame/_Chunk), Types, Frustum
```

- 책임 분리: **World = 게임 데이터, VulkanContext = 렌더링**. 렌더러는 "월드가 주는 것을 그린다".

---

## 렌더러 [구현됨]

- Vulkan: instance / device / swapchain / render pass / pipelines / sync
- **파이프라인**: 공유 빌더 `createPipeline(PipelineConfig)`로 메인패스 4개(player·chunk·ui·object) 생성. shadow 계열(청크/나무/플레이어)은 depth-only 별도 파이프라인.
- viewport/scissor = **dynamic state** (리사이즈 시 파이프라인 재생성 불필요)
- **청크 메시**: Hidden Face Culling, 청크별 vertex/index 버퍼, dirty만 리빌드(프레임당 N개 제한)
- **컬링**: 청크 AABB frustum culling (메인패스 + shadow 라이트 프러스텀)
- **오브젝트**: 공유 메시 + 청크별 인스턴스 버퍼(나무), 인스턴스 버퍼는 청크 로드당 1회 빌드
- **조명 스택**: ambient + sun diffuse(dayFactor) + shadow + fog (4-layer)
- **그림자**: 2048² shadow map, 3×3 PCF, 캐스터=청크+나무+플레이어, 밤엔 shadow pass 스킵
- **day/night**: `timeOfDay`로 태양 방향/하늘색/안개색/조도 변화
- 색: top/side vertex color (텍스처 미사용, 추후 아틀라스 호환 구조), per-vertex AO 베이크

---

## 동기화 모델 [구현됨]

- `MAX_FRAMES_IN_FLIGHT = 2`
- `imageAvailable` / `inFlight` = 프레임별, `renderFinished` = **스왑체인 이미지별** + `imagesInFlight` 추적 (semaphore 재사용 위반 방지)
- player / selector / UI 버퍼 + UBO = **프레임별 복제** (frame-in-flight data race 방지)
- **deferred deletion 큐**: GPU가 아직 읽는 청크 버퍼를 N프레임 뒤 안전 해제

---

## 월드 [구현됨]

- 청크 `unordered_map<ivec2, Chunk>`, 32×32×8, load/unload radius 기반 **스트리밍**
- 절차 지형: FBM noise 2채널(height/biome), 좌표 기반 **결정론적**
- `TileState`: `growthStage`, `lastUpdatedDay` (작물 time-based catch-up)
- 오브젝트 레이어: **terrain(voxel) ≠ object(prop)** 분리, 나무 = 인스턴스 프롭
- save/load: **수정 청크만** 바이너리 저장. 나무 등 오브젝트는 미저장 → 좌표 결정론으로 재생성

---

## 기술 방향 [계획]

### 그림자 / 비주얼 로드맵
1. 그림자 bias 균형(피터패닝 교정) ← 진행 중
2. contact / blob shadow (접지감, 거의 무료)
3. color grading / tone mapping (fullscreen post 1패스)
4. terrain breakup (vertex color hue / dirt 패치)
5. vegetation variation (HSV ±, 종 2~3개) / micro height / sky tint / wind

### 오브젝트 시스템 일반화
- `ObjectType → MeshRegistry` (2번째 프롭 추가 시)
- `ObjectDefinition` 데이터 주도(collidable / castShadow / baseScale)
- variant 시스템(울타리 등 연결 구조)
- tree 전용 → generic **StaticProp** 시스템으로 승격

### 월드 모델 — **결정: 고정맵 + 절차 레이어**
- 핵심 통찰: **chunk = 스트리밍 단위(생성 방식이 아님)** → 절차/고정 둘 다 가능
- `WorldSource` 추상화 (ProceduralWorldSource / FixedWorldSource), 청크 시스템은 유지
- 3계층: **Permanent**(hand-authored 지형/마을/랜드마크) + **Persistent**(플레이어 변경, save) + **Renewable**(자원 재생 레이어)
- 장기 4-layer world: `Terrain / TileState / StaticProp / Crop / Entity`
- 현재 구현은 **절차생성 유지**. 고정맵 전환은 맵 데이터 포맷·콘텐츠 준비 후 단계적으로.

### 보류 (월드 대규모화 시점에만)
- 청크 메모리 풀링(VMA/서브할로케이터) — 할당 개수 한계 대비
- 청크 메시 DEVICE_LOCAL — 디스크리트 GPU 정점 페치 가속 (GTX 1050급엔 체감 미미라 보류)

---

## 알려진 이슈 / 메모

- **그림자 피터패닝**: 파이프라인 depthBias + 셰이더 bias 이중 적용으로 절벽·블록 edge에서 그림자가 떠 보일 수 있음 — bias 튜닝 진행 중.
- **작물**: 현재 voxel 타일(`WHEAT` + `TileState`)로 처리. 장기적으로 별도 `Crop` 인스턴스 레이어 분리 검토.
- 그림자 최소 밝기 `max(shadow, 0.4)` 는 파스텔 톤 유지를 위한 **의도된 스타일**(버그 아님).
