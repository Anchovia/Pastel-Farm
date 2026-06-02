# ARCHITECTURE — Pastel Farm Engine

> 이 문서는 **엔진이 어떻게 구성돼 있고 왜 그렇게 했는지 + 기술적 장기 방향**을 다룬다.
> 게임 기획은 `DESIGN.md`, 변경 이력은 `DEVLOG.md`, 기능 스냅샷·빌드는 `README.md`.
> 외부 Vulkan 예제 레퍼런스 운용 기준은 `VULKAN_REFERENCES.md`.
>
> **갱신 시점:** 구조가 바뀔 때만(새 시스템 / 리팩토링 / 파이프라인 변경). 매 작업마다 갱신하지 않는다.
> **태그:** `[구현됨]` = 현재 코드에 존재 · `[계획]` = 합의된 방향(아직 구현 안 됨).

---

## 설계 철학

- 고품질 스타일라이즈드 / 로우폴리 / 플랫 셰이딩 — 로우폴리는 저품질 제약이 아니라 미학적 선택
- 성능 기준: 최소 GTX 1050 Ti / 1080p / 60fps, 권장 GTX 1660 Super급. 초저사양 데모가 아니라 **최적화된 고품질 상용 게임**이 목표
- 최신 기법은 선별적으로 사용한다: 텍스처 매핑, material-lite, 고품질 식생, AA, shadow 품질 옵션, post 효과를 성능 예산 안에서 적극 도입
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
- **파이프라인**: 공유 빌더 `createPipeline(PipelineConfig)`로 scene 계열(player/selector/drop·chunk·object·grass)과 UI overlay pipeline을 생성하고, post는 별도 fullscreen pipeline으로 처리. `AA OFF/FXAA`는 기존 post pipeline, `AA SMAA`는 `smaa_edge → smaa_blend → smaa_neighborhood` 3-pass 후처리 체인을 사용한다. UI pipeline은 post render pass에 맞춰 생성되어 AA/color grading 이후 스왑체인 위에 그려진다. shadow 계열(청크/오브젝트/플레이어)은 depth-only 별도 파이프라인.
- viewport/scissor = **dynamic state** (리사이즈 시 파이프라인 재생성 불필요)
- **청크 메시**: Hidden Face Culling, 청크별 vertex/index 버퍼, dirty만 리빌드(프레임당 N개 제한)
- **컬링**: 청크 AABB frustum culling (메인패스 + shadow 라이트 프러스텀)
- **오브젝트**: `ObjectType`별 공유 메시 + 청크별 타입 그룹 인스턴스 버퍼(tree/rock/workbench/fence/stone fence). 오브젝트 변경 시에만 `objectsDirty`로 재빌드
- **식생/지면 dressing**: 절차 grass alpha texture + X자 card mesh + 청크별 grass instance buffer + 좌표 기반 density field + shader 기반 grass tint/card variation. 별도 ground dressing buffer로 잔돌/패치 placement도 검증 중. 둘 다 저장하지 않는 시각 dressing layer이며 shadow caster는 아님
- **조명 스택**: ambient + sun diffuse(dayFactor) + shadow + fog (4-layer)
- **그림자**: 2048² shadow map, 3×3 PCF, 캐스터=청크+`ObjectDef.castShadow` 오브젝트+플레이어, 밤엔 shadow geometry draw 스킵
- **day/night**: `timeOfDay`로 태양 방향/하늘색/안개색/조도 변화
- 색/재질: 현재 지형/오브젝트는 top/side vertex color + per-vertex AO 베이크, grass는 alpha texture. 장기적으로 terrain/object texture mapping과 material-lite(albedo + tint + roughness/specular 계열 상수)를 도입
- **DevUI / 프로파일링**: `PASTEL_DEV_BUILD`에서 Dear ImGui F3 패널을 post pass 위에 렌더링. 자체 게임 UI도 post AA 이후에 렌더링해 픽셀 폰트가 AA에 의해 깨지지 않게 한다. `VkQueryPool` timestamp로 total/shadow/scene/post/imgui GPU 구간 시간을 표시

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
- `TileState`: `growthStage`, `lastUpdatedDay`, `watered`(물주기 상태는 현재 transient)
- 오브젝트 레이어: **terrain(voxel) ≠ object(prop)** 분리. 자연물(tree/rock)과 설치물(workbench/fence/stone fence)을 같은 StaticProp 경로로 처리
- save/load: **수정 청크만** 바이너리 저장(v2). 타일 + `TileState` 일부 + 청크 오브젝트를 직렬화해 설치물 유지와 채집 자연물 respawn 방지를 처리. 미수정 청크는 좌표 결정론으로 재생성

---

## 기술 방향 [계획]

> **렌더링 목표 재정렬.** Pastel Farm은 초저사양 로우폴리 데모가 아니라, GTX 1050 Ti 최소 / GTX 1660 Super 권장 기준에서 고품질 스타일라이즈드 화면을 목표로 한다. 최적화·모듈성은 계속 핵심이지만, 품질을 낮추기 위한 보수주의는 피한다.

### 우선순위 Tier (현재)
**Tier 1 — 가속기 + 부채 상환 (app-flow 토대)**
- ✅ DevUI(ImGui, `PASTEL_DEV_BUILD` 게이트) + Dev 빌드 구성 + GPU timestamp 프로파일링 — 비주얼 튜닝의 전제조건. **완료**
- ✅ `FrameRenderData` 스냅샷 — `drawFrame` 인자 10개 → 구조체 1개(`VulkanContext.h`). 렌더러는 public 경계에서 스냅샷만 소비. **완료**
- ✅ `GpuBuffer` RAII 래퍼 — `VkBuffer+VkDeviceMemory`(+mapped) move-only RAII로 통합, `createBuffer` 반환형화. `operator VkBuffer()`로 읽기 무변경. **완료**
- App-state 머신(MainMenu/Settings/Loading/Gameplay/Pause) + 입력 정책 + world session start/end. **MainMenu 클릭 UI + Settings 클릭 UI(+VSync 적용/AA 데이터) + Loading 1차 + Pause 클릭 메뉴 완료**: 시작 시 MainMenu 표시, `START` / `SETTINGS` row 클릭으로 Gameplay 시작 또는 Settings 진입(키보드 백업 없이 클릭 전용), 설정 row 클릭으로 VSync ON/OFF(swapchain present mode 재생성 적용)·AA OFF/FXAA/SMAA(UI/데이터) 변경, FXAA 실제 post AA 적용 완료, SMAA 1x 1차 적용 완료, 시작 시 Loading을 한 프레임 표시한 뒤 save 로드 + 초기 청크 로드 후 Gameplay 진입, `ESC`로 Gameplay/Paused 토글, Pause에서는 `RESUME` / `SETTINGS` / `QUIT`(세션 정리 후 타이틀 복귀) row 클릭 제공. Settings는 진입 위치(MainMenu/Pause)에 따라 `BACK`/`ESC` 복귀 위치가 달라짐. 인벤토리가 열린 Gameplay에서는 `ESC`가 Pause보다 인벤토리 닫기를 우선. 메뉴/settings/loading/pause 중 게임 업데이트·카메라 회전·월드 입력 차단, 메뉴/settings/loading 중 청크 스트리밍·저장 차단. 해상도·볼륨 등 추가 옵션은 예정

**Tier 2 — 비주얼 정체성 (DevUI로 실시간 튜닝)**
- ✅ 카메라 follow 댐핑 — `Camera` 내부 `m_followTarget` 지수 보간 + Loading 후 `snapToTarget`으로 저장 위치 스냅. 플레이어 추적감 개선, 회전은 기존 즉시 반응 유지
- ✅ hemisphere/colored ambient — `chunk.frag`/`triangle.frag`에서 법선 방향 기반 warm/cool ambient tint 적용. 밤 ambient 바닥값은 0.10으로 낮춰 야간을 더 어둡게 조율
- height fog
- ✅ vegetation alpha card 1차 — 절차 grass texture + X자 card clump + alpha test + shadow 제외 + 청크별 dirty gate. **완료**
- ✅ density field 기반 grass dressing 1차 — 균등 확률 대신 patch density + open grass bias + density 기반 offset/scale variation 적용. **완료**
- 다음 비주얼 후보: terrain/object texture mapping · material-lite · high-quality grass(wind/LOD/density 옵션) · ground dressing 텍스처화 · shadow quality options · height fog · SMAA diagonal/T2x/S2x 또는 Ultra 튜닝
- 비고: grading/split-tone·fog·shadow·AO는 **이미 구현** → 격차는 튜닝 + 위 추가뿐

**Tier 3 — 확장 (rule of 3 도달 시)**
- 오디오(ambient/발걸음/SFX, 가벼운 single-header 미들웨어) · 경량 `IRenderPass` · 콘텐츠 데이터화(JSON 등) · 상호작용 인터페이스(`IInteractable`) · 순수 로직 테스트(inventory/save/craft)

> ✅ 즉시 가능한 작은 완성도: **오브젝트 충돌** — `collidable`이 이미 `ObjectDef` 데이터로 존재 → `World::isCollidableAt` + `canOccupy` 한 줄로 배선 완료. 건축이 장식에서 기능으로.

### 렌더링 품질 로드맵 (Tier 2 세부)
- ✅ color grading / tone mapping (post 1패스: exposure/contrast/saturation/split-tone/vignette)
- ✅ 그림자 접지 튜닝 (피터패닝 — bias 축소 + cull 조정 완료)
- ✅ FXAA 실제 적용: post pass에서 화면 edge를 완화. 자체 게임 UI는 FXAA 이후에 그려 픽셀 폰트 선명도 유지
- ✅ SMAA 1x 1차 적용: `AreaTex/SearchTex` LUT 기반 `edge detection → blend weight → neighborhood blending` 3-pass. 현재 값은 High preset 계열(`threshold=0.10`, `search=16`, corner rounding 25)에 가깝지만 diagonal detection/reprojection/T2x/S2x는 아직 제외
- texture mapping: terrain atlas/object albedo texture 경로 추가. vertex color는 tint/스타일 보정으로 유지
- material-lite: full PBR 전환 전, albedo + tint + roughness/specular 상수로 재질 차이를 표현
- shadow quality options: shadow map 해상도/PCF 샘플/거리 옵션, contact/blob shadow, 넓은 맵 이후 CSM 검토
- terrain breakup은 타일별 vertex color 랜덤이 아니라 비격자 dressing layer로 처리(풀 clump, 잔돌, 흙/마른 풀 패치, 길 가장자리)
- height fog / vegetation variation / wind / sky tint / LUT

### 다음 실행 순서 — 결정
다음 세션/작업은 아래 순서를 따른다. 큰 material/PBR 시스템이나 render graph로 먼저 가지 않는다.

1. 현재 SMAA 1x 1차 변경분을 커밋한다.
2. **TextureResource 기반**: grass texture 생성/upload 경로, SMAA LUT 업로드 경로, 이후 terrain/object texture mapping을 포괄할 작은 texture helper를 도입한다.
3. **Terrain/object texture mapping + material-lite**: terrain atlas 또는 단순 albedo texture부터 시작하고, vertex color는 tint/스타일 보정으로 유지한다.
4. **High-quality grass pass**: card 수/variant, wind, distance fade/LOD, density DevUI 튜닝을 1050 Ti 60fps 예산 안에서 적극적으로 올린다.
5. **SMAA 품질 확장(선택)**: 1x 체감이 부족하면 diagonal detection, Ultra 계열 threshold/search, T2x/S2x를 별도 성능 예산으로 검토한다.

이 순서는 "품질을 올리되, 매 단계가 화면에 바로 기여하고 기존 구조와 자연스럽게 맞물리는" 경로다.

### Vegetation Alpha Card — **방향**
- 참고 이미지 수준의 자연스러운 풀밭은 단순 삼각형 기하 clump보다 alpha card 방식이 맞다. 기하 blade는 멀리서 삐쭉한 바늘처럼 보이기 쉽다.
- 목표: 풀 텍스처 1장 + X자/부채꼴 card clump + instancing + 좌표 기반 결정론 배치. GRASS 전체 균등 배치가 아니라 숲 가장자리/물가/빈 잔디 영역 등 density rule로 조절.
- 성능 기준: GTX 1050 Ti 최소 60fps 안에서 grass는 핵심 비주얼 투자처다. clump당 card 수, density, texture 품질, wind, LOD/fade를 DevUI/GPU timing으로 보며 적극적으로 올린다.
- 이후 확장: DevUI density/거리/scale 튜닝, wind sway(vertex shader), card/texture variant, 거리 LOD 또는 원거리 density fade. 필요하면 단순 X-card를 넘어 부채꼴 card나 다중 card clump도 검토.
- 현재 상태: 절차 RGBA grass texture, alpha-test grass pipeline, X자 card mesh, 청크별 instance buffer, 좌표 기반 density field, shader 기반 tint/card variation까지 1차 연결 완료. ground dressing은 저장하지 않는 좌표 기반 placement layer로 유효하지만, geometry placeholder는 과했기 때문에 cleanup에서 거의 안 보이는 수준으로 축소했다. 다음 개선은 texture/card detail, wind, 낮은 대비의 ground texture/card detail 방향.

### Grid 규칙 vs Organic 표현 — **결정**
- 농사·설치/철거·충돌·저장 좌표는 grid 기반으로 유지한다. 플레이어 규칙은 예측 가능해야 한다.
- 자연 환경은 grid를 그대로 드러내지 않는다. 큰 잔디/흙 면을 타일별 색 랜덤으로 흔들면 격자감이 더 강해지므로 금지.
- 자연스러운 breakup은 렌더/월드의 별도 dressing layer로 만든다. 후보: alpha card 풀 clump, 잔돌, 흙/마른 풀 패치, 길 가장자리, 덤불/꽃/forage.
- dressing layer는 좌표 기반 결정론을 유지하고, 가능하면 저장 대상이 아닌 재생 가능한 시각 레이어로 시작한다.

### 스타듀식 오브젝트 경제 — **결정**(우선순위 ↑, 복셀 블록 편집은 은퇴)
순서: ✅① 인벤토리/작물 경제 → ✅② 제네릭 오브젝트 시스템 → ✅③ 자원 채집 → ✅④ 지형 불변화 → ⑤⑥ 제작·건축(아래 분할) → ⑦ 이후.
- **제작·건축 분할(2단계 의존성 순서):** ✅⑤a 인벤 제작(기본 레시피, 클릭형) → ✅⑥ 오브젝트 설치/철거(작업대·울타리를 월드에) → ✅⑤b 작업대 근처 고급 레시피(`requiresWorkbench`) 해금. **(아크 ①~⑥ 전체 완료 ✅)**
> ① 완료: 스택+개수 인벤토리, 숫자 렌더러, 낫 수확 + 드롭/줍기 레이어(`DroppedItem`). ② 완료: `m_objectMeshes` 메시 레지스트리 + 청크별 타입 그룹 + `ObjectDef` 테이블. ③ 완료: `tryHarvestObject`(도끼→나무/곡괭이→돌 → 드롭 → 줍기). ④ 완료: 복셀 설치/파괴 제거(지형 불변). ⑤a 완료: `Recipe` 테이블 + 클릭형 인벤 제작. ⑥ 완료: 오브젝트 설치/철거 + save v2 영속성. ⑤b 완료: `isObjectTypeNear` 작업대 근접 판정 → 고급 레시피 해금(돌담).
- **인벤토리**: 슬롯+개수(스택) 모델 + add/remove API + 숫자 렌더러(비트맵 digit quad, 텍스처 없음). 모든 드롭/소모가 여기로.
- `ObjectType → MeshRegistry` + `ObjectDefinition` 데이터 주도(mesh / collidable / castShadow / **harvestTool / dropItem / placeable**). 현재 tree/rock/workbench/fence/stone fence를 generic **StaticProp** 경로로 처리.
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

### 보류 (실제 필요가 생긴 뒤)
- 청크 메모리 풀링(VMA/서브할로케이터) — 할당 개수 한계 대비
- 청크 메시 DEVICE_LOCAL — 현재도 staging path가 존재하나, 동적 청크 메시/식생 규모가 커지고 GPU vertex fetch 병목이 보이면 우선순위 상승

### 명시적 비목표 (Anti-goals) — rule of 3 / 실제 병목 전엔 **안 함**
개인 Vulkan 엔진이 "AAA 체크리스트"에 빠져 게임을 못 내는 함정 방어선. 단, 이것은 저품질을 목표로 하자는 뜻이 아니다. 품질에 직접 기여하는 텍스처, AA, shadow 옵션, material-lite는 적극 검토한다.
- **ECS 전면 전환** — 오브젝트가 sparse, OOP로 충분. (필요 시 hybrid SoA만 국소 적용)
- **Render Graph / FrameGraph** — 풀 그래프 X. 경량 `IRenderPass`까지만.
- **Asset DB / Material 노드그래프** — 에셋 수가 늘기 전엔 보류. 단, TextureResource/helper와 material-lite는 반복이 생기는 즉시 도입 가능.
- **Job/Async 시스템** — 청크 리빌드는 `MAX_CHUNK_BUILDS_PER_FRAME` throttle로 이미 완화. 식생 대량화 시점에.
- **VulkanContext 빅뱅 분할** — 한 번에 쪼개지 말 것. 작은 것부터(GpuBuffer→FrameRenderData→파이프라인 생성 점진 추출).
- **RTX/GI · mesh shader · bindless 대규모 시스템 · full PBR 전면 전환** — 지금 당장 필요하지 않다. PBR은 에셋/재질 수요가 실제로 생긴 뒤 material-lite에서 단계적으로 판단.
> 기준: **rule of 3**(세 번 반복되면 추상화) + **전면 rewrite 금지, 국소 리팩토링만**. (`CLAUDE.md` 단순함·수술적 변경과 일치)

### 불변식 (Invariants) — 깨지 말 것
- **결정론 생성**: 지형/배치는 해시·좌표 기반(`TerrainGen`). 랜덤은 항상 시드/좌표 기반.
- **레이어 분리**: `World`(데이터/시뮬) ↔ `GameState`(규칙) ↔ `VulkanContext`(렌더, **스냅샷만 소비**). 렌더러에 권위 상태 두지 않기.
- **상태 변경은 `World`/`GameState` 경유** (멀티·세이브 친화).
- **지형 불변** (복셀 편집 은퇴 — resolved).
- **세이브 버전 정책**: 개발 중엔 자유롭게 깸(버전 불일치=새 월드). pre-release 후엔 마이그레이션 추가.

---

## 알려진 이슈 / 메모

- **grass/ground dressing**: alpha card + density field + shader 기반 tint/card variation 1차는 완료. ground dressing 1차는 구조 검증에는 성공했고, 과했던 갈색 patch/pebble placeholder는 cleanup에서 크게 축소했다. 현재는 거의 안 보이는 기준 화면으로 두고, 다음 핵심은 텍스처/알파 기반 디테일 전환과 wind.
- **작물**: 현재 voxel 타일(`WHEAT` + `TileState`)로 처리. 장기적으로 별도 `Crop` 인스턴스 레이어 분리 검토.
- 그림자 최소 밝기 `max(shadow, 0.4)` 는 파스텔 톤 유지를 위한 **의도된 스타일**(버그 아님).
