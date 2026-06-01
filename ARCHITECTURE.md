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
- **DevUI / 프로파일링**: `PASTEL_DEV_BUILD`에서 Dear ImGui F3 패널을 post pass 위에 렌더링. `VkQueryPool` timestamp로 total/shadow/scene/post/imgui GPU 구간 시간을 표시

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

> **중간점검 (아크 ①~⑥ 완료 후 재설정).** 핵심 판단: 기술 기반은 이미 충분 — 이제 "기능 추가"보다 **iteration speed · 비주얼 정체성 · app-flow**가 ROI가 높다. 비주얼은 **다시 만들기가 아니라 조율하기**(grading·fog·shadow·AO 이미 존재).

### 우선순위 Tier (현재)
**Tier 1 — 가속기 + 부채 상환 (app-flow 토대)**
- ✅ DevUI(ImGui, `PASTEL_DEV_BUILD` 게이트) + Dev 빌드 구성 + GPU timestamp 프로파일링 — 비주얼 튜닝의 전제조건. **완료**
- ✅ `FrameRenderData` 스냅샷 — `drawFrame` 인자 10개 → 구조체 1개(`VulkanContext.h`). 렌더러는 public 경계에서 스냅샷만 소비. **완료**
- ✅ `GpuBuffer` RAII 래퍼 — `VkBuffer+VkDeviceMemory`(+mapped) move-only RAII로 통합, `createBuffer` 반환형화. `operator VkBuffer()`로 읽기 무변경. **완료**
- App-state 머신(Boot/MainMenu/Settings/Loading/Gameplay/Pause) + 입력 컨텍스트 + 설정(해상도/vsync/볼륨/AA) + world load/unload. **MainMenu 1차 + Settings 클릭 UI(+VSync 적용/AA 데이터) + Loading 1차 + Pause 1차 완료**: 시작 시 MainMenu 표시, `S`로 Settings 진입, 설정 row 클릭으로 VSync ON/OFF(swapchain present mode 재생성 적용)·AA OFF/FXAA/SMAA(데이터/UI) 변경, `ESC`/`BACK`으로 MainMenu 복귀, `Enter`로 Loading을 한 프레임 표시한 뒤 save 로드 + 초기 청크 로드 후 Gameplay 진입, `ESC`로 Gameplay/Paused 토글. 메뉴/settings/loading/pause 중 게임 업데이트·카메라 회전·월드 입력 차단, 메뉴/settings/loading 중 청크 스트리밍·저장 차단, dim overlay + pause 아이콘 + `PAUSED` 문구 표시. 입력 정책 helper와 로컬 `AppFlow`, Tiny UI Text 기반으로 DevUI 캡처/app mode 차단/edge-detect/기초 문구 표시 접합면을 정리. AA 실제 렌더 적용·해상도 등 추가 옵션은 예정

**Tier 2 — 비주얼 정체성 (DevUI로 실시간 튜닝)**
- height fog · hemisphere/colored ambient(조명단 warm/cool) · 카메라 follow 댐핑
- vegetation/object variation(스케일/회전/tint) · wind field · AA(SMAA 주력 + FXAA fallback) · LUT(선택)
- 비고: grading/split-tone·fog·shadow·AO는 **이미 구현** → 격차는 튜닝 + 위 추가뿐

**Tier 3 — 확장 (rule of 3 도달 시)**
- 오디오(ambient/발걸음/SFX, 가벼운 single-header 미들웨어) · 경량 `IRenderPass` · 콘텐츠 데이터화(JSON 등) · 상호작용 인터페이스(`IInteractable`) · 순수 로직 테스트(inventory/save/craft)

> ✅ 즉시 가능한 작은 완성도: **오브젝트 충돌** — `collidable`이 이미 `ObjectDef` 데이터로 존재 → `World::isCollidableAt` + `canOccupy` 한 줄로 배선 완료. 건축이 장식에서 기능으로.

### 그림자 / 비주얼 로드맵 (Tier 2 세부)
- ✅ color grading / tone mapping (post 1패스: exposure/contrast/saturation/split-tone/vignette)
- ✅ 그림자 접지 튜닝 (피터패닝 — bias 축소 + cull 조정 완료)
- contact / blob shadow (접지감 추가, 거의 무료)
- terrain breakup (vertex color hue / dirt 패치)
- height fog / hemisphere ambient / vegetation variation / wind / sky tint

### 스타듀식 오브젝트 경제 — **결정**(우선순위 ↑, 복셀 블록 편집은 은퇴)
순서: ✅① 인벤토리/작물 경제 → ✅② 제네릭 오브젝트 시스템 → ✅③ 자원 채집 → ✅④ 지형 불변화 → ⑤⑥ 제작·건축(아래 분할) → ⑦ 이후.
- **제작·건축 분할(마크식 2단계, 의존성 순서):** ✅⑤a 인벤 제작(기본 레시피, 클릭형) → ✅⑥ 오브젝트 설치/철거(작업대·울타리를 월드에) → ✅⑤b 작업대 근처 고급 레시피(`requiresWorkbench`) 해금. **(아크 ①~⑥ 전체 완료 ✅)**
> ① 완료: 스택+개수 인벤토리, 숫자 렌더러, 낫 수확 + 드롭/줍기 레이어(`DroppedItem`). ② 완료: `m_objectMeshes` 메시 레지스트리 + 청크별 타입 그룹 + `ObjectDef` 테이블. ③ 완료: `tryHarvestObject`(도끼→나무/곡괭이→돌 → 드롭 → 줍기). ④ 완료: 복셀 설치/파괴 제거(지형 불변). ⑤a 완료: `Recipe` 테이블 + 클릭형 인벤 제작. ⑥ 완료: 오브젝트 설치/철거 + save v2 영속성. ⑤b 완료: `isObjectTypeNear` 작업대 근접 판정 → 고급 레시피 해금(돌담).
- **인벤토리**: 슬롯+개수(스택) 모델 + add/remove API + 숫자 렌더러(비트맵 digit quad, 텍스처 없음). 모든 드롭/소모가 여기로.
- `ObjectType → MeshRegistry` + `ObjectDefinition` 데이터 주도(mesh / collidable / castShadow / **harvestTool / dropItem / placeable**). tree 전용 → generic **StaticProp**으로 승격.
- 채집: 도끼→나무, 곡괭이→돌 = 오브젝트 레이캐스트 → 제거 + 드롭 아이템 → 인벤토리.
- **지형 불변**: 좌클릭 복셀 파괴 제거, 건축은 제작 오브젝트 설치/철거(플레이어 설치물만).
- variant 시스템(울타리 등 연결 구조)은 그 이후.

### 월드 모델 — **결정: 고정맵 + 절차 레이어**
- 핵심 통찰: **chunk = 스트리밍 단위(생성 방식이 아님)** → 절차/고정 둘 다 가능
- `WorldSource` 추상화 (ProceduralWorldSource / FixedWorldSource), 청크 시스템은 유지
- 3계층: **Permanent**(hand-authored 지형/마을/랜드마크) + **Persistent**(플레이어 변경, save) + **Renewable**(자원 재생 레이어)
- 장기 4-layer world: `Terrain / TileState / StaticProp / Crop / Entity`
- 현재 구현은 **절차생성 유지**. 고정맵 전환은 맵 데이터 포맷·콘텐츠 준비 후 단계적으로.

### 멀티플레이 — **방향: post-prototype v2** (지금 구현 안 함, 구조만 안 망침)
- **타이밍**: 싱글 프로토타입(농사·채집·건축·save·엔티티)이 설계상 굳은 직후, **콘텐츠가 불어나기 전**. 너무 늦으면 거대 코드베이스에 넷코드 retrofit이 더 비쌈.
- **장르 이점**: 코지 협동(소수 인원, 비경쟁) → 롤백/렉보상/예측 넷코드 불필요. 서버(또는 호스트) 권위 + 클라 입력/액션 전송 + 원격 플레이어 보간이면 충분(스타듀식).
- **이미 유리한 자산**: ① 시뮬(`World`/`GameState`) ↔ 렌더(`VulkanContext`) 분리, ② **결정론 지형**(시드만 공유하면 클라가 각자 생성 — 지형 전송 불필요), ③ `modified` 청크 추적 = 서버 델타와 매핑, ④ 클릭→액션이 이미 이산 명령에 가까움.
- **새로 필요**: 전송 계층(ENet/UDP) + 직렬화 / 서버 권위 상태 / 엔티티 복제(네트워크 ID·보간) / 시간·날짜 동기화 / 소유권 규칙.
- **지금부터 지킬 제약 3가지(비용 0, 이미 준수 중)**: ① 랜덤은 시드·좌표 기반 결정론 유지, ② 모든 상태 변경은 `World`/`GameState` 경유(렌더러에 권위 상태 두지 않기), ③ 게임 동작을 직렬화 가능한 이산 명령으로 모델링.
- **규모감**: 협동 멀티 = 수 주~수 개월 독립 서브시스템(지금까지 작업 전체와 맞먹는 별도 챕터). 렌더링은 거의 불변, 일은 로직 재배선·동기화·테스트에 집중.

### 보류 (월드 대규모화 시점에만)
- 청크 메모리 풀링(VMA/서브할로케이터) — 할당 개수 한계 대비
- 청크 메시 DEVICE_LOCAL — 디스크리트 GPU 정점 페치 가속 (GTX 1050급엔 체감 미미라 보류)

### 명시적 비목표 (Anti-goals) — rule of 3 / 실제 병목 전엔 **안 함**
개인 Vulkan 엔진이 "AAA 체크리스트"에 빠져 게임을 못 내는 함정 방어선. 아래는 매력적이지만 현재 스코프(저사양·플랫셰이딩·소수 콘텐츠)에서 ROI가 낮거나 철학과 충돌:
- **ECS 전면 전환** — 오브젝트가 sparse, OOP로 충분. (필요 시 hybrid SoA만 국소 적용)
- **Render Graph / FrameGraph** — 풀 그래프 X. 경량 `IRenderPass`까지만.
- **Asset DB / Material 시스템 / Material 노드그래프** — 텍스처 거의 없음(메시 절차생성, vertex color). 추상화 역순.
- **Job/Async 시스템** — 청크 리빌드는 `MAX_CHUNK_BUILDS_PER_FRAME` throttle로 이미 완화. 식생 대량화 시점에.
- **VulkanContext 빅뱅 분할** — 한 번에 쪼개지 말 것. 작은 것부터(GpuBuffer→FrameRenderData→파이프라인 생성 점진 추출).
- **RTX/GI · mesh shader · bindless · full PBR** — 저사양·스타일라이즈드 목표와 정반대.
> 기준: **rule of 3**(세 번 반복되면 추상화) + **전면 rewrite 금지, 국소 리팩토링만**. (`CLAUDE.md` 단순함·수술적 변경과 일치)

### 불변식 (Invariants) — 깨지 말 것
- **결정론 생성**: 지형/배치는 해시·좌표 기반(`TerrainGen`). 랜덤은 항상 시드/좌표 기반.
- **레이어 분리**: `World`(데이터/시뮬) ↔ `GameState`(규칙) ↔ `VulkanContext`(렌더, **스냅샷만 소비**). 렌더러에 권위 상태 두지 않기.
- **상태 변경은 `World`/`GameState` 경유** (멀티·세이브 친화).
- **지형 불변** (복셀 편집 은퇴 — resolved).
- **세이브 버전 정책**: 개발 중엔 자유롭게 깸(버전 불일치=새 월드). pre-release 후엔 마이그레이션 추가.

---

## 알려진 이슈 / 메모

- **그림자 피터패닝**: 파이프라인 depthBias + 셰이더 bias 이중 적용으로 절벽·블록 edge에서 그림자가 떠 보일 수 있음 — bias 튜닝 진행 중.
- **작물**: 현재 voxel 타일(`WHEAT` + `TileState`)로 처리. 장기적으로 별도 `Crop` 인스턴스 레이어 분리 검토.
- 그림자 최소 밝기 `max(shadow, 0.4)` 는 파스텔 톤 유지를 위한 **의도된 스타일**(버그 아님).
