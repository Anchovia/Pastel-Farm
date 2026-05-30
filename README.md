# Game Engine (C++ / Vulkan)

저사양 PC에서도 부드럽게 돌아가는 **로우폴리 / 플랫 셰이딩** 스타일 게임을 만들기 위한 커스텀 게임 엔진입니다.
상용 엔진(Unity·Unreal) 대신 C++과 Vulkan으로 처음부터 직접 구축하며, **게임 완성**과 **엔진 개발** 두 가지를 동시에 목표로 합니다.

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
---

## 프로젝트 구조

```
game project/
├─ CMakeLists.txt          # 빌드 설정 (FetchContent, 셰이더 컴파일 포함)
├─ README.md
├─ DEVLOG.md               # Vulkan 개념 정리 및 구현 기록
├─ .gitignore
├─ shaders/
│  ├─ triangle.vert        # 정점 셰이더 (GLSL)
│  └─ triangle.frag        # 프래그먼트 셰이더 (GLSL)
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
   │  ├─ Types.h           # Vertex, InstanceData, TileType 정의
   │  ├─ Frustum.h         # Frustum 구조체 (6평면 추출 + AABB 테스트)
   │  ├─ VulkanContext.h   # Vulkan 렌더러 선언
   │  └─ VulkanContext.cpp # Vulkan 렌더러 구현
   └─ world/
      ├─ Chunk.h           # Chunk 구조체, TileState, IVec2Hash, 청크 상수
      ├─ World.h           # 청크 맵 기반 월드 인터페이스 선언
      └─ World.cpp         # 청크 라우팅, 타일 색상, 좌표 변환 구현
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
- `TileType`: `AIR(0) / GRASS / DIRT / WATER / STONE` — AIR는 렌더링 제외, 블록 없음을 의미
- `TileState`: 성장 단계(`growthStage`), 마지막 업데이트 날짜(`lastUpdatedDay`) 예약 — 농경지 Time-based catch-up 방식 대비
- 모든 블록은 **1×1×1 단위 큐브**로 통일 (메시 하나, 색상/텍스처만 다름)
- 타일 좌표 = 월드 좌표 직접 매핑, 캐릭터는 연속 좌표(float)로 자유 이동
- Z=0 레이어: 기본 지면, Z=1 이상: 블록 설치 영역
- 미로드 청크는 AIR 반환 — 로드된 청크만 렌더링, 이동 판정에도 반영

### 렌더링 전략
- **청크 메시 생성** — 청크별로 보이는 면만 골라 버텍스+인덱스 버퍼 직접 생성 (Hidden Face Culling), 인스턴싱보다 GPU 부하 대폭 감소
- **플랫 셰이딩** — 면마다 단색 + 디렉셔널 라이트로 명암
- **top/side 색상 분기** — 윗면과 옆면 색상 분리 (GRASS: 윗면 초록 / 옆면 흙 갈색 등), 나중에 텍스처 아틀라스로 교체 가능
- **프러스텀 컬링** — 카메라 시야 밖 청크 draw call 차단

### 최적화 우선순위
1. ~~인스턴싱으로 드로우콜 감소~~ ✅
2. ~~청크 기반 구조 전환~~ ✅
3. ~~청크 단위 Frustum Culling~~ ✅
4. 시야 밖 청크 언로드

---

## 커밋 컨벤션

| 타입 | 설명 |
|------|------|
| `feat:` | 새 기능 |
| `fix:` | 버그 수정 |
| `docs:` | 문서만 변경 |
| `refactor:` | 동작 변경 없는 코드 정리 |

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
