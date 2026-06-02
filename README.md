# Game Engine (C++ / Vulkan)

저사양 PC에서도 부드럽게 돌아가는 **로우폴리 / 플랫 셰이딩** 스타일 게임을 만들기 위한 커스텀 게임 엔진입니다.
상용 엔진(Unity·Unreal) 대신 C++과 Vulkan으로 처음부터 직접 구축하며, **게임 완성**과 **엔진 개발** 두 가지를 동시에 목표로 합니다.

---

## 문서 구성

| 문서 | 내용 | 갱신 시점 |
|------|------|-----------|
| `README.md` | 기능 스냅샷 + 빌드법 | 기능 추가 시 |
| `DEVLOG.md` | 변경 이력(시간순) | 작업마다 |
| `ARCHITECTURE.md` | 엔진 구조 + 기술 방향 | 구조 변경 시 |
| `DESIGN.md` | 게임 기획·비전·스코프 | 방향 변경 시 |
| `VULKAN_REFERENCES.md` | 외부 Vulkan 예제 레퍼런스 운용 노트 | 참고 기준 변경 시 |

> 각 사실은 **한 문서에만** 두고 나머지는 참조한다(중복·노후화 방지).

---

## 목표

- **저사양 친화적**: GTX 750 Ti / Intel UHD 급에서 60fps
- **로우폴리 + 플랫 셰이딩**: 단순하고 스타일리시한 그래픽 (텍스처 의존도 최소화)
- **Vulkan 직접 사용**: 낮은 CPU 오버헤드, 렌더링 제어권 확보
- **무거운 PBR / 포토리얼리즘은 하지 않음**

---

## 기술 스택

| 분류 | 사용 기술 |
|------|-----------|
| 언어 | C++20 |
| 빌드 | CMake 3.20+ |
| 그래픽 API | Vulkan |
| 윈도우/입력 | GLFW 3.4 |
| 수학 | GLM 1.0.1 |
| 개발 UI | Dear ImGui 1.89.9 (`PASTEL_DEV_BUILD` 전용) |
| 셰이더 | GLSL → SPIR-V (`glslc`) |
| 의존성 관리 | CMake FetchContent (vcpkg 불필요) |

> GLFW와 GLM은 빌드 시 자동으로 받아옵니다. 개발 빌드에서는 ImGui도 FetchContent로 받아옵니다. Vulkan SDK만 미리 설치하면 됩니다.

---

## 현재 기능 스냅샷

### App Flow / 입력
- `MainMenu / Settings / Loading / Gameplay / Pause` 상태 흐름
- 클릭형 `START`, `SETTINGS`, `RESUME`, `QUIT`, `BACK` UI
- VSync ON/OFF 실제 present mode 적용, AA OFF/FXAA/SMAA 선택 데이터/UI
- 메뉴/settings/loading/pause 중 게임 입력·월드 업데이트 차단
- `ESC`: Gameplay↔Pause, 인벤토리가 열려 있으면 인벤토리 닫기 우선

### 월드 / 게임플레이
- 32×32×8 청크 스트리밍(`LOAD_RADIUS=3`, `UNLOAD_RADIUS=4`)
- FBM 기반 절차 지형(height/biome), 물·잔디·흙·언덕·돌 정상 생성
- 지형 불변: Minecraft식 복셀 설치/파괴 은퇴
- 농사 루프: 경작 → 씨앗 심기 → 물주기 → 성장 → 낫 수확
- 인벤토리 27칸(핫바 9 + 보관함), 스택/개수, 드롭 아이템 자동 줍기
- 제작: 작업대, 울타리, 작업대 근처 고급 레시피(돌담)
- 오브젝트 채집/설치/철거: tree, rock, workbench, fence, stone fence
- 오브젝트 충돌: 나무·돌·울타리·작업대 등 통과 차단
- save v2: 수정 청크의 타일, TileState 일부, 오브젝트 직렬화

### 렌더링 / 비주얼
- Vulkan swapchain, depth, descriptor, sync, dynamic viewport/scissor
- 청크 메시 hidden face culling + per-vertex AO + top/side vertex color
- 청크 AABB frustum culling, shadow light frustum culling
- 제네릭 오브젝트 인스턴싱(`ObjectType`별 mesh + 청크별 instance group)
- 2048² shadow map, 3×3 PCF, 청크/오브젝트/플레이어 shadow caster
- day/night 기반 sky/fog/light 변화, hemisphere ambient
- offscreen scene → post pass tone/color grading(exposure/contrast/saturation/split-tone/vignette)
- grass alpha card 1차: 절차 RGBA grass texture + X자 card + alpha test + 청크별 dirty gate
- DevUI(ImGui, `PASTEL_DEV_BUILD`) + GPU timestamp(total/shadow/scene/post/imgui)

> 상세 구현 이력은 `DEVLOG.md`, 구조 판단과 장기 방향은 `ARCHITECTURE.md`를 기준으로 본다.

---

## 다음 방향 (중간점검 후) — 상세는 `ARCHITECTURE.md` Tier
- **Tier 1**: ✅ DevUI(ImGui) + GPU 프로파일링 · ✅ `FrameRenderData` 스냅샷 · ✅ `GpuBuffer` RAII · App-state(✅ MainMenu 클릭 UI · ✅ Settings 클릭 UI(+VSync 적용/AA 데이터) · ✅ Loading 1차 · ✅ Pause 클릭 메뉴 / 추가 옵션 예정)
- **Tier 2 (비주얼)**: ✅ 카메라 댐핑 · ✅ hemisphere ambient(warm/cool) · ✅ vegetation alpha card 1차(풀 clump, alpha test, shadow 제외) · grass density field/variant · height fog · organic dressing layer(잔돌/흙 패치) · wind · **AA(SMAA + FXAA fallback)** · LUT
- ✅ **즉시 작은 완성도**: 오브젝트 충돌(`canOccupy` 한 줄) — 완료
- **비목표**(당분간 X): ECS rewrite · render graph · asset DB · material graph · RTX/PBR/mesh shader/bindless

---

## 프로젝트 구조

```
pastelfarm/
├─ CMakeLists.txt
├─ README.md
├─ DEVLOG.md
├─ ARCHITECTURE.md
├─ DESIGN.md
├─ VULKAN_REFERENCES.md
├─ shaders/
│  ├─ triangle.vert/.frag       # player / selector / drop cubes
│  ├─ chunk.vert/.frag          # terrain chunk mesh + AO + shadow/fog
│  ├─ object.vert               # instanced StaticProp, chunk.frag 재사용
│  ├─ grass.vert/.frag          # alpha-card grass dressing
│  ├─ shadow*.vert              # chunk/object/player depth-only shadow pass
│  ├─ post.vert/.frag           # fullscreen post grading
│  └─ ui.vert/.frag             # 2D UI overlay
└─ src/
   ├─ main.cpp                  # AppMode 흐름, 입력 정책, 세션 시작/종료, 렌더 루프
   ├─ game/
   │  ├─ Camera.h               # orbit camera + follow damping
   │  ├─ Player.h
   │  ├─ GameState.h
   │  └─ GameState.cpp          # 이동/농사/채집/제작/설치/인벤토리 규칙
   ├─ platform/
   │  ├─ InputManager.h/.cpp
   │  └─ Window.h/.cpp
   ├─ renderer/
   │  ├─ Types.h                # TileType, ItemType, render/UI shared structs
   │  ├─ Frustum.h
   │  ├─ VulkanContext.h
   │  ├─ VulkanContext.cpp
   │  ├─ VulkanContext_Init.cpp
   │  ├─ VulkanContext_Frame.cpp
   │  ├─ VulkanContext_Chunk.cpp
   │  └─ VulkanContext_Private.h
   └─ world/
      ├─ Chunk.h                # Chunk, TileState, ObjectType/ObjectDef, Object
      ├─ World.h/.cpp           # 청크 라우팅, save/load v2, 오브젝트 API
      └─ TerrainGen.h/.cpp      # FBM terrain + deterministic tree/rock 배치
```

> `build/` 폴더는 CMake 생성물 + 자동으로 받은 GLFW/GLM 소스가 들어있어 git에서 제외됩니다.

---

## 게임 설계 방향

### 시점
- 고정 아이소메트릭 시점 (Q/E로 카메라 공전)
- 플레이어가 아닌 카메라가 돌아가는 방식

### 게임 성격
- 스타일라이즈드 로우폴리 농사·라이프심
- Stardew-style 농사/채집/제작/건축 + 고정 아이소메트릭 시점
- Minecraft식 복셀 설치/파괴는 은퇴. 지형은 불변이고 건축은 오브젝트 레이어에서 처리

### 월드 구조
- **청크 기반 3D 그리드** — `unordered_map<ivec2, Chunk>`, 청크 1개 = `32×32×8`
- `TileType`: `AIR / GRASS / DIRT / WATER / STONE / WOOD / LEAVES / FARMLAND / WHEAT`
- `TileState`: `growthStage`, `lastUpdatedDay`, `watered`
- terrain voxel은 1×1×1 큐브 기반이지만 플레이어 편집 대상은 아님
- 타일 좌표 = 월드 좌표 직접 매핑, 캐릭터는 연속 좌표(float)로 자유 이동
- Z=0: 기본 지면(GRASS/DIRT/WATER), Z=1: 언덕, Z=2: 돌 정상
- 절차적 생성: FBM noise 2채널(높이/바이옴), `TerrainGen::generate(cx, cy, chunk)` — 청크 좌표만 넘기면 결정론적 생성
- 미로드 청크는 AIR 반환 — 플레이어 주변 반경 3청크 로드, 4청크 밖 자동 언로드
- 오브젝트 레이어 — 나무/돌/작업대/울타리/돌담은 복셀이 아닌 `Chunk::objects` StaticProp으로 관리

### 렌더링 전략
- **청크 메시 생성** — 청크별로 보이는 면만 골라 버텍스+인덱스 버퍼 직접 생성 (Hidden Face Culling), 인스턴싱보다 GPU 부하 대폭 감소
- **오브젝트 인스턴싱** — `ObjectType`별 공유 메시 + 청크별 인스턴스 그룹
- **grass alpha card** — 절차 텍스처 + X자 카드 mesh + 청크별 인스턴스 버퍼
- **플랫 셰이딩** — 면마다 단색 + 디렉셔널 라이트로 명암
- **top/side 색상 분기** — 지형 윗면과 옆면 색상 분리
- **Ambient Occlusion** — 꼭짓점별 복셀 AO로 모서리·구석 음영, 청크 빌드 시 베이크 (추가 렌더패스 없음)
- **프러스텀 컬링** — 카메라/그림자 시야 밖 청크 draw call 차단
- **Post grading** — offscreen scene 후 fullscreen tone/color grading 적용

### 최적화 우선순위
1. ~~인스턴싱으로 드로우콜 감소~~ ✅
2. ~~청크 기반 구조 전환~~ ✅
3. ~~청크 단위 Frustum Culling~~ ✅
4. ~~시야 밖 청크 언로드~~ ✅

---

## 커밋 컨벤션

| 타입 | 설명 |
|------|------|
| `feat:` | 새 기능 |
| `fix:` | 버그 수정 |
| `docs:` | 문서만 변경 |
| `refactor:` | 동작 변경 없는 코드 정리 |
| `perf:` | 동작 변경 없는 성능 최적화 |
| `chore:` | 빌드·브랜치·릴리즈·개발환경 등 관리 작업 |

> 예: `dev` → `main` 병합처럼 기능/수정 자체가 아니라 브랜치 운영 성격의 커밋은 `chore:`를 사용합니다.

---

## 빌드 방법 (Windows / Visual Studio 2022)

**사전 요구사항**
- Visual Studio 2022 (C++ 데스크톱 개발 워크로드)
- CMake (VS 내장 사용 가능)
- Vulkan SDK

```cmd
cd "C:\Users\USER\Desktop\pastelfarm"
set VULKAN_SDK=C:\VulkanSDK\<version>
cmake -B build -S .
```

이후 `build/GameEngine.sln` 열고 F5, 또는:

```cmd
cmake --build build --config Debug
```

> 셰이더(`.spv`)는 빌드 시 자동 컴파일되어 실행 파일 옆 `shaders/` 폴더로 복사됩니다.
