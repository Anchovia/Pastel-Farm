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
| 셰이더 | GLSL → SPIR-V (`glslc`) |
| 의존성 관리 | CMake FetchContent (vcpkg 불필요) |

> GLFW와 GLM은 빌드 시 자동으로 받아옵니다. Vulkan SDK만 미리 설치하면 됩니다.

---

## 구현된 기능

- Vulkan 초기화 (Instance, Validation Layer, Surface, Device, Swapchain, Pipeline)
- 창 리사이즈 시 Swapchain 자동 재생성
- Vertex Buffer — 정점 데이터 CPU → GPU 업로드
- Index Buffer — 정점 재사용
- UBO + Descriptor Set — MVP 행렬을 셰이더에 전달
- GLM MVP 행렬 — 모델 회전, 카메라(view), 원근 투영(proj)
- Depth Buffer — 3D 앞뒤 판별
- 3D 큐브 — 정점 8개, 면 6개, 색상 그라데이션
- 궤도 카메라 — Q/E로 타겟 주위 공전, 고정 피치/거리
- 플랫 셰이딩 — 면법선 기반 Lambert 조명 (윗면 밝고 옆면 어둡게)
- 인스턴싱 — 10×10 타일 그리드를 드로우콜 1번으로 렌더링
- 타일 그리드 시스템 — TileType(GRASS/DIRT/WATER/STONE) + 인스턴스 색상
- 플레이어 — WASD 이동 (카메라 방향 기준), 카메라가 플레이어를 따라감
- GameState/Player 분리 — 플레이어 상태와 이동 계산을 렌더러 밖으로 이동
- 기본 이동 충돌 — 맵 밖 이동 차단, 물 타일 이동 불가
- 마우스 피킹 (Raycasting) — 3D 역산환 및 O(1) 평면 교차 판정을 통한 마우스 기반 타일 선택
- 상호작용 사거리 제한 — 플레이어 기준 반경 1칸(3x3) 내에서만 마우스 타겟팅이 되도록 Clamp 적용
- Staging Buffer 도입 — 정적 메시 데이터를 임시 버퍼를 거쳐 GPU 전용 메모리(DEVICE_LOCAL)로 이전하여 렌더링 성능 최적화
- 입력 시스템 분리 (InputManager) — 메인 루프에서 GLFW 입력 로직을 분리하여 확장성 및 가독성 향상
- copyBuffer 동기화 개선 — `vkQueueWaitIdle`(큐 전체 블로킹) → `VkFence`(해당 전송만 대기)로 교체, 청크 런타임 로드 대비
- World 3D 그리드 전환 — `m_grid[H][W]` → `m_grid[DEPTH][H][W]` (DEPTH=8), `TileType::AIR` 추가, 블록 배치/파괴 기반 마련
- 청크 시스템 (32×32) — 고정 배열을 `unordered_map<ivec2, Chunk>`로 전환, dirty 플래그 기반 청크별 GPU 버퍼 재빌드, 무한 월드 확장 기반 마련
- Frustum Culling — Gribb & Hartmann 방법으로 viewProj에서 6평면 추출, 청크 AABB 테스트로 시야 밖 draw call 완전 차단
- 윗면/옆면 색상 분기 — `InstanceData`에 `topColor + sideColor` 두 채널, 셰이더에서 법선 방향으로 분기. GRASS 윗면 초록/옆면 흙 갈색 등 타일별 적용. 나중에 텍스처 아틀라스로 교체 가능한 구조
- Hidden Face Culling + 청크 메시 생성 — 인스턴싱 폐기, 청크별 보이는 면만 골라 `ChunkVertex` 버텍스+인덱스 버퍼 직접 생성. 청크 전용 파이프라인(`m_chunkPipeline`) 추가, 플레이어/셀렉터는 기존 인스턴싱 파이프라인 유지
- 다단계 지형 — Z=1 언덕 + Z=2 정상, 행별 범위 테이블로 불규칙 지형 정의. 절벽 옆면 갈색/정상 초록 자동 적용
- 3D 충돌 — `canOccupy`에서 발 아래(Z) walkable + 몸통 높이(Z+1) AIR 이중 체크. 높이 차이 있는 블록 측면 통과 방지
- 청크 로드/언로드 + 절차적 지형 — FBM noise 기반 `TerrainGen`, load radius=3/unload radius=4, 플레이어 이동 시 자동 생성/해제. 무한 월드 실현
- 블록 설치/파괴 — 좌클릭=파괴(AIR), 우클릭=설치(STONE). 타겟 Z 자동 탐색(topmost non-AIR), dirty 청크 즉시 GPU 재빌드
- 지형 다양화 — 물(저지대 호수), 바이옴(GRASS/DIRT)
- 핫바 UI — 스크린 좌표 전용 파이프라인(depth off, alpha blend), 9칸 슬롯 + 선택 강조, 각 슬롯에 블록 색상 아이콘
- 블록 타입 선택 — 숫자키 1~9 / 스크롤 휠로 슬롯 선택, 우클릭이 선택된 타입 설치 (GRASS/DIRT/STONE/WOOD/LEAVES/WATER)
- Ambient Occlusion — 꼭짓점별 복셀 AO(0fps 방식), 모서리·구석 음영으로 입체감. 청크 패딩 버퍼 캐싱으로 빌드 비용 최소화
- 오브젝트 레이어 — 나무를 복셀에서 분리한 로우폴리 모델(박스 트렁크 + 3단 콘)로, 인스턴싱 + 청크별 버퍼 + 크기/회전 변형. 작물·바위 등에 재사용 예정
- 인게임 시간 시스템 — 하루 120초 주기(`DAY_DURATION`), `timeOfDay`(0→1, 자정→정오→자정) + `day` 카운터. 하늘 배경색이 자정/새벽/낮/노을 4 키프레임 선형 보간으로 변화
- 아이템/도구 시스템 — `ItemType` enum(블록 6종 + 도구 2종)으로 핫바 통합. 도구 선택 시 블록 미설치(동작은 이후 단계). `I`키로 인벤토리 창 토글, 슬롯 클릭으로 핫바에 아이템 할당
- 농사 시스템 — 호미로 GRASS/DIRT → FARMLAND 경작. SEED_WHEAT로 씨앗 심기 → WHEAT 성장(단계별 색상 변화: 연초록→황금). `m_day` 기반 2일마다 1단계 성장. 완숙(stage 3) 좌클릭 수확
- 세계 저장/로드 — 플레이어가 수정한 청크만 `save.dat`에 바이너리로 저장(타일 + TileState). Ctrl+S 저장, 시작 시 자동 로드. 언로드된 수정 청크는 메모리에 보관했다가 재방문 시 복원
- 동적 태양 조명 — UBO에 `lightDir(xyz) + dayFactor(w)` 추가. `timeOfDay` 기반으로 태양 고도/방위각 계산, 낮엔 Lambert diffuse 최대, 밤엔 ambient 0.15(달빛)만 남도록 셰이더 전체 적용
- Shadow Map 인프라 — 1024×1024 depth-only `VkImage` + shadow 전용 `VkRenderPass` / `VkFramebuffer` 생성. 스왑체인과 독립적으로 한 번만 생성. Shadow pass 실행 및 셰이더 샘플링은 다음 단계
- Shadow Pass 파이프라인 — depth-only 파이프라인 + `shadow.vert`(push constant lightMVP). `drawFrame`에서 태양 방향 기반 orthographic light matrix 계산, main pass 이전에 청크 메시를 태양 시점으로 렌더링해 shadow map을 채움
- Shadow Sampler + Descriptor 연결 — comparison sampler(`LESS_OR_EQUAL`) 생성. descriptor layout binding 1에 `combinedImageSampler` 추가, descriptor set에 shadow image view + sampler 바인딩. fragment shader에서 shadow map을 읽을 준비 완료
- Shadow Rendering — UBO에 `lightMVP` 추가. `chunk.vert`/`object.vert`/`triangle.vert`에서 `fragPosLightSpace` 출력, `chunk.frag`/`triangle.frag`에서 `sampler2DShadow`로 shadow 비교. NdotL 기반 가변 bias로 shadow acne 억제. shadow map 2048×2048, ortho ±80
- Shadow 깊이 범위 수정 — `GLM_FORCE_DEPTH_ZERO_TO_ONE` 추가. GLM ortho가 Vulkan 기준 [0,1] 깊이로 매핑되도록 수정. 플레이어 이동 시 그림자가 잘리던 문제 해결
- 태양 방향 수정 — azimuth 부호 반전(`-timeOfDay × 2π`). 그림자 회전 방향이 더 자연스러워짐
- 안개 (Fog) — view-space 깊이 기반 선형 안개. `FOG_START=27, FOG_END=57`. 먼 거리 지형이 하늘색으로 자연스럽게 희미해지고 청크 경계가 가려짐. 안개 색은 UBO `fogColor`로 sky color와 동기화되어 시간대에 따라 자동 변화
- Shadow PCF — `chunk.frag`/`triangle.frag`에 3×3 수동 PCF 추가. 하드웨어 2×2 bilinear PCF(LINEAR 샘플러)와 결합해 shadow map 샘플링 경계 개선
- 리사이즈 viewport 수정 — 파이프라인 viewport/scissor를 dynamic state로 전환, 창 크기 변경 시 렌더 영역이 새 swapchain 크기에 맞게 갱신 (이전엔 옛 크기로 고정돼 화면이 쏠림)
- 프레임별 동적 버퍼 분리 — player/selector/UI 인스턴스 버퍼를 frame-in-flight 수만큼 복제해, GPU가 읽는 도중 덮어쓰던 data race 제거
- present semaphore 이미지별 분리 — `m_renderFinished`를 스왑체인 이미지별로 두고 `imagesInFlight` 추적 추가, present semaphore 재사용 위반 제거
- Frustum near 평면 수정 — `GLM_FORCE_DEPTH_ZERO_TO_ONE`에 맞춰 near 평면을 `row(2)`로 (Vulkan [0,1] depth)
- 밤 shadow pass 스킵 — 태양이 지평선 아래일 땐 shadow map 청크 렌더를 건너뛰어 야간 GPU 부하 감소 (clear·레이아웃 전환은 유지)
- object 인스턴스 버퍼 1회 빌드 — 나무 인스턴스 버퍼를 청크 로드당 한 번만 생성, 메시 리빌드 때마다 재생성하던 비용 제거
- 청크 리빌드 프레임 분할 — dirty 청크를 프레임당 N개로 제한해 청크 스트리밍·작물 성장 시 프레임 스파이크 완화
- 디스크 로드 청크 나무 복원 — 저장된 수정 청크의 나무를 좌표 결정론으로 재생성(오브젝트는 미저장), 저장/재방문 시 나무가 사라지던 버그 수정
- shadow 라이트 프러스텀 컬링 — shadow pass에서 태양 직교 박스 밖 청크를 컬링해 주간 shadow draw call 감소 (그림자 손실 없음)
- 나무 그림자 캐스팅 — 나무도 shadow map에 깊이를 기록해 지면에 그림자를 드리움 (인스턴스 변환 적용 shadow 셰이더/파이프라인 추가)
- 플레이어 그림자 캐스팅 — 플레이어 큐브도 shadow map에 기록해 바닥에 그림자를 드리움
- 그림자 접지 튜닝 — bias 축소 + chunk·player shadow cull NONE으로 그림자가 geometry에 딱 붙도록 교정(피터패닝 제거)
- 포스트 프로세스 그레이딩 — 씬을 오프스크린에 렌더 후 풀스크린 패스로 톤/색 보정(노출·대비·채도·warm/cool split-tone·비네트) 적용
- 물주기 — 물뿌리개로 경작지에 물을 주면 짙어지고, 물 준 날에만 작물이 성장(매일 재급수). 물이 성장을 gate
- Day HUD / 숫자 렌더러 — 텍스처 없이 3×5 도트matrix 숫자를 UI quad로 그려 좌상단에 날짜 표시 (인벤토리 개수 표시에 재사용)
- 인벤토리(스택+개수) — `ItemStack{type,count}` 27칸(핫바 9 + 보관함), 핫바/인벤토리 창에 개수 숫자 표시. 씨앗은 심으면 소모, 도구는 무한 (재배치는 추후)
---

## 프로젝트 구조

```
game project/
├─ CMakeLists.txt          # 빌드 설정 (FetchContent, 셰이더 컴파일 포함)
├─ README.md
├─ DEVLOG.md               # Vulkan 개념 정리 및 구현 기록
├─ .gitignore
├─ shaders/
│  ├─ triangle.vert/.frag  # 플레이어·셀렉터 (인스턴싱)
│  ├─ chunk.vert/.frag     # 청크 메시 (top/side 색상 + AO)
│  ├─ object.vert          # 오브젝트 인스턴싱 (나무, frag는 chunk.frag 재사용)
│  └─ ui.vert/.frag        # 2D UI 오버레이 (핫바)
└─ src/
   ├─ main.cpp             # 진입점: 창 생성 + 렌더 루프
   ├─ game/
   │  ├─ Camera.h          # 카메라 상태 및 행렬 계산 캡슐화
   │  ├─ Player.h          # 플레이어 위치/이동 속도
   │  ├─ GameState.h       # 게임 상태 및 입력 스냅샷 선언
   │  └─ GameState.cpp     # 플레이어 이동/충돌 규칙 업데이트
   ├─ platform/
   │  ├─ InputManager.h    # GLFW 입력 상태 폴링 및 캡슐화
   │  ├─ InputManager.cpp  # 입력 처리 구현
   │  ├─ Window.h          # GLFW 창 래퍼 (RAII, 리사이즈 콜백) 선언
   │  └─ Window.cpp
   ├─ renderer/
   │  ├─ Types.h                   # Vertex/ChunkVertex/UIVertex, InstanceData/ObjectInstance, TileType
   │  ├─ Frustum.h                 # Frustum 구조체 (6평면 추출 + AABB 테스트)
   │  ├─ VulkanContext.h           # Vulkan 렌더러 선언
   │  ├─ VulkanContext.cpp         # 생성자/소멸자, 공유 헬퍼 (createBuffer, copyBuffer 등)
   │  ├─ VulkanContext_Init.cpp    # 모든 초기화 create* 함수
   │  ├─ VulkanContext_Frame.cpp   # drawFrame, recordCommandBuffer, updateHotbar 등
   │  ├─ VulkanContext_Chunk.cpp   # buildChunkBuffer, rebuildDirtyChunks
   │  └─ VulkanContext_Private.h  # 파일 간 공유 상수/타입 (비공개)
   └─ world/
      ├─ Chunk.h           # Chunk 구조체, TileState, Object, IVec2Hash, 청크 상수
      ├─ World.h           # 청크 맵 기반 월드 인터페이스 선언
      ├─ World.cpp         # 청크 라우팅, 로드/언로드, 타일 색상
      ├─ TerrainGen.h      # 절차적 지형 생성 선언
      └─ TerrainGen.cpp    # FBM noise 지형 + 나무 오브젝트 배치
```

> `build/` 폴더는 CMake 생성물 + 자동으로 받은 GLFW/GLM 소스가 들어있어 git에서 제외됩니다.

---

## 게임 설계 방향

### 시점
- 고정 아이소메트릭 시점 (Q/E로 카메라 공전)
- 플레이어가 아닌 카메라가 돌아가는 방식

### 게임 성격
- 스타듀밸리 (농지, 자원 수집) + 마인크래프트 (블록 설치/파괴) + 고정 아이소메트릭 시점
- 기본은 평지 한 층, 블록을 쌓아 단차 표현 가능 (계단/사다리로 이동)

### 월드 구조
- **청크 기반 3D 그리드** — `unordered_map<ivec2, Chunk>`, 청크 1개 = `32×32×8`
- `TileType`: `AIR(0) / GRASS / DIRT / WATER / STONE / WOOD / LEAVES` — AIR는 렌더링 제외, 블록 없음을 의미
- `TileState`: 성장 단계(`growthStage`), 마지막 업데이트 날짜(`lastUpdatedDay`) 예약 — 농경지 Time-based catch-up 방식 대비
- 모든 블록은 **1×1×1 단위 큐브**로 통일 (메시 하나, 색상/텍스처만 다름)
- 타일 좌표 = 월드 좌표 직접 매핑, 캐릭터는 연속 좌표(float)로 자유 이동
- Z=0: 항상 고체 지면(GRASS/DIRT), Z=1: 언덕(height > 0.45), Z=2: 돌 정상(height > 0.65)
- 절차적 생성: FBM noise 2채널(높이/바이옴), `TerrainGen::generate(cx, cy, chunk)` — 청크 좌표만 넘기면 결정론적 생성
- 미로드 청크는 AIR 반환 — 플레이어 주변 반경 3청크 로드, 4청크 밖 자동 언로드
- 오브젝트 레이어 — 나무 등 유기적 프롭은 복셀이 아닌 별도 모델로 타일 위에 배치 (`Chunk::objects`), 복셀 지형과 분리

### 렌더링 전략
- **청크 메시 생성** — 청크별로 보이는 면만 골라 버텍스+인덱스 버퍼 직접 생성 (Hidden Face Culling), 인스턴싱보다 GPU 부하 대폭 감소
- **오브젝트 인스턴싱** — 나무 등 프롭은 공유 메시 1개를 인스턴스 데이터(위치/스케일/회전)로 대량 렌더링
- **플랫 셰이딩** — 면마다 단색 + 디렉셔널 라이트로 명암
- **top/side 색상 분기** — 윗면과 옆면 색상 분리 (GRASS: 윗면 초록 / 옆면 흙 갈색 등), 나중에 텍스처 아틀라스로 교체 가능
- **Ambient Occlusion** — 꼭짓점별 복셀 AO로 모서리·구석 음영, 청크 빌드 시 베이크 (추가 렌더패스 없음)
- **프러스텀 컬링** — 카메라 시야 밖 청크 draw call 차단

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

---

## 빌드 방법 (Windows / Visual Studio 2022)

**사전 요구사항**
- Visual Studio 2022 (C++ 데스크톱 개발 워크로드)
- CMake (VS 내장 사용 가능)
- Vulkan SDK

```cmd
cd "C:\Users\USER\Desktop\game project"
set VULKAN_SDK=C:\VulkanSDK\1.3.275.0
cmake -B build -S .
```

이후 `build/GameEngine.sln` 열고 F5, 또는:

```cmd
cmake --build build --config Debug
```

> 셰이더(`.spv`)는 빌드 시 자동 컴파일되어 실행 파일 옆 `shaders/` 폴더로 복사됩니다.
