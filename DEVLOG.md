# DEVLOG — Pastel Farm Engine

Vulkan 공부 겸 엔진 개발 기록.

---

## 구현 기록

### Vulkan 초기화 + 첫 삼각형
- GLFW 창 생성 (`Window` 클래스, RAII 방식)
- Vulkan Instance, Validation Layer, Surface, Physical/Logical Device 초기화
- Swapchain, Image Views, Render Pass, Graphics Pipeline 구성
- Framebuffer, Command Pool/Buffer, Sync Objects 생성
- 셰이더 하드코딩 삼각형 출력

### Vertex Buffer
- `Vertex` 구조체 정의 (`pos: vec2`, `color: vec3`)
- `kVertices` 데이터를 CPU → GPU 메모리로 업로드
- 파이프라인에 Vertex Input 바인딩 정보 등록
- `vkCmdBindVertexBuffers` + `vkCmdDraw`로 그라데이션 삼각형 출력

### World 클래스 분리
- `src/world/World.h` / `World.cpp` 신규 생성
- 타일 데이터(`m_grid[H][W]`)와 색상 테이블을 VulkanContext에서 분리
- `VulkanContext`가 `World&` 참조를 받아 `createInstanceBuffer()`에서 읽도록 변경
- `main.cpp`에서 `World` 생성 후 `VulkanContext`에 전달
- 역할 분리: World = 게임 데이터, VulkanContext = 렌더링

### 플레이어 이동
- 플레이어 전용 인스턴스 버퍼 1개 (persistently mapped) 별도 생성
- `processInput()`에서 WASD로 플레이어 이동, 매 프레임 버퍼 업데이트
- 카메라 각도(orbitAngle) 기준으로 forward/right 방향 계산
  - `forward = (-cos θ, -sin θ)` — 카메라 반대 방향(화면 안쪽)
  - `right = (-sin θ, cos θ)` — 화면 기준 오른쪽
- 플레이어 드로우콜을 타일 드로우콜과 별도로 호출 (같은 파이프라인, 다른 인스턴스 버퍼)
- `updateUniformBuffer()`에서 `m_orbitTarget = playerPos`로 카메라가 플레이어를 따라감
- 플레이어 색상: 주황색 `{1.0, 0.45, 0.1}`

### Player/GameState 분리
- `src/game/Player.h` 추가 — 플레이어 위치와 이동 속도 보관
- `src/game/GameState.h` / `GameState.cpp` 추가 — 입력 스냅샷(`PlayerInput`) 기반 플레이어 이동 계산
- `VulkanContext`에서 `m_playerPos`와 `processInput()` 제거
- `main.cpp`가 GLFW 입력을 읽고 `GameState::update(dt, input, orbitAngle)` 호출
- `VulkanContext::drawFrame(playerPosition)`으로 플레이어 위치만 전달
- 렌더러 역할 축소: 플레이어 상태를 소유하지 않고, 전달받은 위치로 카메라 타겟/플레이어 인스턴스 버퍼만 갱신
- 결과: 화면과 조작은 유지하면서 게임 로직과 렌더링 책임 분리

### 기본 이동 충돌
- `World::inBounds(x, y)` 추가 — 월드 범위 밖 좌표 차단
- `World::isWalkable(x, y)` 추가 — 현재는 `WATER` 타일을 이동 불가로 처리
- `World::worldToTile(position)` 추가 — 플레이어의 연속 좌표(float)를 타일 좌표(int)로 변환
- `World::tileCenter(x, y)` 추가 — 타일 렌더링 위치와 게임 로직 좌표 기준을 `World`에서 통일
- `GameState::update()`가 이동 후보 위치를 계산한 뒤 `World`에 보행 가능 여부를 질의
- X/Y축을 나눠서 이동 검사 — 한 축이 막혀도 다른 축 이동은 가능한 구조
- `VulkanContext::createInstanceBuffer()`가 직접 오프셋을 계산하지 않고 `World::tileCenter()` 사용
- 결과: 플레이어가 맵 밖으로 나가지 않고, 물 타일 위로 이동하지 않음

### 타일 그리드 시스템
- `TileType` enum 추가 (GRASS, DIRT, WATER, STONE)
- `Vertex`에서 color 제거 — 색상은 인스턴스에서 담당
- `InstanceData`에 `color` 추가 (`pos` + `color`)
- `kWorld[10][10]` 배열로 타일 맵 정의
- 타일 타입 → 색상 변환 후 인스턴스 버퍼에 업로드
- 셰이더: `instanceColor`를 base color로 사용, Lambert 조명 적용
- 결과: 잔디/흙/물/돌이 섞인 플랫 셰이딩 타일 맵

### 인스턴싱
- `InstanceData { glm::vec3 pos }` 구조체 추가
- 인스턴스 버퍼 생성 (10×10 그리드 위치 데이터)
- 파이프라인에 binding 1 추가 (`VK_VERTEX_INPUT_RATE_INSTANCE`)
- 셰이더: `layout(location=3) in vec3 instancePos` → `worldPos = inPosition + instancePos`
- `vkCmdDrawIndexed(indexCount, instanceCount=100, ...)` — 드로우콜 1번
- 그리드 원점 중심 정렬: 각 위치를 `x - (GRID-1)/2` 로 오프셋
- 경계 타일 옆면 saw-tooth 현상은 타일 시스템 구현 시 내부 면 제거로 해결 예정

### 플랫 셰이딩
- `Vertex`에 `normal` 필드 추가
- 큐브 정점 8개 → 24개 (면당 4정점, 법선 공유 불가)
- 셰이더에 `flat` qualifier — 삼각형 내 보간 없이 단일 색상
- Fragment shader: Lambert 조명 `ambient(0.3) + diffuse(0.7) × dot(normal, lightDir)`
- 면별 색상: 윗면 밝은 초록 / 옆면 중간·어두운 초록 / 아랫면 갈색
- 결과: 로우폴리 타일 스타일 큐브

### 궤도 카메라
- 자유 시점 카메라 → 궤도 카메라로 교체
- `m_orbitAngle`, `m_orbitDistance`, `m_orbitPitch` 로 카메라 위치 계산
- Q/E로 `m_orbitAngle` 증감 → 타겟 주위 공전
- `updateUniformBuffer()`에서 구면 좌표 → 데카르트 좌표 변환 후 `lookAt`
- 게임 방향: Don't Starve 스타일 고정 시점, 추후 플레이어 위치를 `m_orbitTarget`으로

### Depth Buffer + 3D 큐브
- `createDepthResources()` — depth image/memory/view 생성 (DEVICE_LOCAL, D32_SFLOAT)
- `createImage()` 헬퍼 추가
- Render Pass에 depth attachment 추가 (attachment 1번)
- Framebuffer에 depth image view 연결
- Pipeline에 `VkPipelineDepthStencilStateCreateInfo` 추가 (depthTest/Write = true, compareOp = LESS)
- `cleanupSwapchain` / `recreateSwapchain`에 depth 리소스 포함
- `Vertex.pos` vec2 → vec3, 셰이더도 동일하게 업그레이드
- `kVertices` 4개 → 큐브 8개, `kIndices` 6개 → 36개
- 결과: 원근감+depth 정렬이 정상 작동하는 회전 3D 큐브

### UBO + MVP 행렬
- `UniformBufferObject` 구조체 정의 (model / view / proj mat4)
- Descriptor Set Layout, Descriptor Pool, Descriptor Set 생성
- Uniform Buffer를 프레임마다 (MAX_FRAMES_IN_FLIGHT=2) 각각 생성, 영구 매핑
- `updateUniformBuffer()` — 매 프레임 GLM으로 MVP 계산 후 memcpy
  - model: 시간에 따라 Z축 회전
  - view: `glm::lookAt({2,2,2}, {0,0,0}, {0,0,1})`
  - proj: `glm::perspective(45°, aspect, 0.1, 10.0)` + Y 반전
- `recordCommandBuffer`에서 `vkCmdBindDescriptorSets`로 셰이더에 연결
- frontFace를 `COUNTER_CLOCKWISE`로 수정 (Y반전으로 winding order가 뒤집혀서)
- 결과: 원근감 있게 기울어진 사각형이 회전

### Index Buffer
- 정점 4개 + `kIndices {0,1,2, 0,2,3}`으로 사각형 구성
- `createIndexBuffer()` 추가
- `vkCmdDraw` → `vkCmdBindIndexBuffer` + `vkCmdDrawIndexed`
- 결과: 그라데이션 사각형 출력

### 마우스 피킹 (Raycasting) 및 사거리 제한
- 화면의 2D 마우스 좌표를 3D 월드 공간으로 변환하는 레이캐스팅(Raycasting) 구현
- 역산환(Unprojection) 과정:
  1. 마우스 픽셀 좌표를 NDC(-1.0 ~ 1.0) 공간으로 변환
  2. 투영(Proj)과 뷰(View) 행렬의 역행렬(`inverse(proj * view)`)을 계산
  3. NDC 좌표에 역행렬을 곱해 월드 공간의 Ray 방향 벡터(Direction) 도출
- 수학적 교차 판정: - 타일이 모두 $Z=0$ 평면에 있다는 점을 이용, 루프 없이 O(1) 수학 공식($t = -O_z / D_z$)으로 광선과 바닥이 만나는 정확한 3D 좌표를 단번에 계산
- 사거리 제한 (Clamp):
  - 마우스가 아무리 멀리 있어도 플레이어 주변 타일만 선택되도록 처리
  - `std::clamp`를 이용해 타겟 타일과 플레이어 타일 간의 거리(delta)를 X, Y 각각 -1 ~ 1 사이로 강제 고정
- 기존의 키보드 방향 기반 타겟팅(`updateTargetTile`) 로직을 완전 제거하고 마우스 조작으로 일원화

### 카메라 구조 리팩토링 (Camera 클래스 분리)
- `Camera` 클래스 신규 생성: 카메라의 위치, 타겟, 뷰(View) 및 투영(Projection) 행렬 계산 로직 캡슐화
- **DRY 원칙 적용:** `GameState`와 `VulkanContext` 양쪽에 중복되어 있던 행렬 계산 코드를 제거하고, `Camera` 객체의 참조를 받아 사용하도록 통합
- **의존성 분리:** - 렌더러(`VulkanContext`)는 카메라 상태 변수(`orbitAngle` 등)를 소유하지 않고 넘겨받은 행렬만 렌더링에 사용
  - 게임 로직(`GameState`)은 단순 각도가 아닌 카메라의 실제 3D 좌표를 기반으로 플레이어 이동 방향(`forward`, `right`)을 역산하도록 구조 개선
- `main.cpp`에서 카메라 객체를 소유하며, 창 리사이즈 이벤트 발생 시 카메라의 종횡비(Aspect Ratio)를 실시간 갱신하도록 처리하여 화면 찌그러짐 방지

### Staging Buffer 도입 (VRAM 최적화)
- 정적 데이터(큐브 정점, 큐브 인덱스, 셀렉터 정점/인덱스)를 CPU 접근 가능 메모리(`HOST_VISIBLE`)에서 GPU 전용 초고속 메모리(`DEVICE_LOCAL`)로 마이그레이션.
- `copyBuffer` 헬퍼 함수 구현: 임시 버퍼 생성 → 데이터 복사(`vkCmdCopyBuffer`) → 동기화(`vkQueueWaitIdle`) → 임시 버퍼 파괴로 이어지는 정석적인 Vulkan 전송 파이프라인 구축.
- 매 프레임 업데이트가 필요한 `m_instanceBuffer`, `m_playerInstBuffer` 등은 `HOST_VISIBLE`을 유지하여 불필요한 복사 오버헤드 최소화.

### 입력 시스템 분리 (InputManager)
- `src/platform/InputManager` 클래스를 신규 생성하여 플랫폼 종속적인 입력 처리(GLFW)를 전담하도록 구조 개선.
- `main.cpp`의 메인 루프에 하드코딩되어 있던 키보드, 마우스 좌표, 창 크기 계산 로직을 `pollInput()` 메서드 하나로 캡슐화.
- **메인 루프 다이어트:** 길고 지저분했던 입력 수집 코드를 1줄로 압축하여 가독성을 크게 높임.
- 추후 커스텀 단축키 설정(Key Binding)이나 UI 클릭 시 월드 클릭 무시 같은 기능을 추가할 때 확장이 용이해짐.

### copyBuffer 동기화 개선 (VkFence)
- `copyBuffer` 내 `vkQueueWaitIdle` → `VkFence` 기반 동기화로 교체.
- `vkQueueWaitIdle`은 큐에 제출된 **모든 작업**이 끝날 때까지 블로킹 — 렌더링 중 호출 시 GPU 완전 정지.
- `VkFence`를 `vkQueueSubmit`에 넘기고 `vkWaitForFences`로 **이 전송 하나**만 대기 → 다른 큐 작업 불간섭.
- 현재는 초기화 전용 호출이므로 동작 차이 없음. 청크 런타임 로드/언로드 시 이 구조가 필수.

### World 3D 그리드 전환
- `TileType::AIR = 0` 추가 — 0으로 메모리 초기화 시 전체 AIR가 되어 3D 배열 초기화에 편리.
- `m_grid[HEIGHT][WIDTH]` → `m_grid[DEPTH][HEIGHT][WIDTH]` (`DEPTH = 8`).
- 생성자: `memset`으로 전체 AIR 초기화 후, Z=0 레이어에만 기존 맵 데이터를 `memcpy`로 배치.
- `getTile / setTile / inBounds / isWalkable / tileCenter` 전부 Z 파라미터 추가.
- `worldToTile` 반환 타입 `glm::ivec2` → `glm::ivec3`. Z 계산: `round(position.z) - 1` — 플레이어가 타일 위 1유닛 높이에 서는 구조를 반영.
- `isWalkable`: AIR와 WATER 둘 다 이동 불가로 처리.
- `GameState` / `VulkanContext` 전파: `glm::ivec2` → `glm::ivec3`, 레이캐스팅 `delta.z = 0`으로 타겟은 플레이어와 같은 Z 레이어로 고정.
- `createInstanceBuffer`: Z 루프 추가, AIR 타일 스킵 — 비어있는 레이어는 자동으로 렌더링 제외.
- 현재 동작 변화 없음 — Z=1~7은 전부 AIR. 이후 `setTile(x, y, z, type)` 호출만으로 블록 배치/파괴가 바로 연결됨.

### 청크 시스템 (16×16)
- 고정 배열 `m_grid[DEPTH][H][W]` → `unordered_map<ivec2, Chunk>` — 무한 확장 가능한 구조로 전환.
- `Chunk.h` 신규 생성: `CHUNK_SIZE=16`, `CHUNK_DEPTH=8`, `Chunk` 구조체(`tiles`, `dirty` 플래그), `TileState`(농경지 성장 단계 등 미래 상태 예약), `IVec2Hash`.
- 타일 좌표 = 월드 좌표 직접 매핑 (`tileCenter(x,y,z) = {x,y,z}`) — 기존 중앙 정렬 오프셋 제거, 플레이어 시작 위치 `{5,5,1}`(맵 중앙)으로 변경.
- `World::getTile/setTile`: 청크 좌표(`chunkCoord`) + 로컬 좌표(`localCoord`)로 라우팅. 미로드 청크는 AIR 반환.
- `setTile` 호출 시 해당 청크 `dirty=true` 자동 마킹.
- **렌더러 청크 버퍼**: 단일 `m_instanceBuffer` → `unordered_map<ivec2, ChunkRenderData>` (청크당 버퍼 1개).
- `rebuildDirtyChunks()`: 매 프레임 dirty 청크만 버퍼 재빌드 → 블록 변경 시 전체가 아닌 해당 청크만 GPU 업로드.
- `recordCommandBuffer`: 청크 맵을 순회하며 청크당 draw call 1번 — 청크 단위 프러스텀 컬링 기반 마련.
- `inBounds`: X/Y 무한 확장 대응으로 Z 범위만 검사.
---

## 게임 설계 메모

### 게임 방향

스타듀밸리(농지/자원 수집) + 마인크래프트(블록 설치/파괴)를 고정 아이소메트릭 시점으로.
기본은 평지 한 층. 블록을 쌓으면 단차 발생 → 계단/사다리로 이동.

### 월드: 타일 기반

```
World[x][y][z] = TileType  // 3D 그리드
```

모든 블록은 **1×1×1 단위 큐브**로 통일.
같은 메시를 재사용하고 색상/텍스처만 바꾸면 되므로 인스턴싱과 궁합이 좋다.

```
월드 로직  → 그리드 좌표 (정수)  : 타일 종류, 충돌, 속성
렌더링/물리 → 연속 좌표 (float)  : 캐릭터 위치, 이동, 애니메이션
```

**타일 기반의 장점:**
- 충돌 판정: 캐릭터 위치 → 그리드 좌표 변환 → 타일 속성 조회 (O(1))
- 청크 시스템: N×N 타일 묶음으로 가까운 것만 로드/언로드
- 컬링: 청크 단위로 frustum 밖이면 통째로 제외

### 렌더링: 인스턴싱

타일을 하나하나 개별 드로우콜로 그리면 1000타일 = 드로우콜 1000번.
인스턴싱은 같은 메시를 위치/색상 데이터만 바꿔서 한 번에 그린다.

```
// 개별 드로우콜 방식 (느림)
for each tile: vkCmdDraw(tile)  // N번 호출

// 인스턴싱 방식 (빠름)
vkCmdDrawIndexed(tileMesh, instanceCount=N)  // 1번 호출
```

저사양 목표에서 타일 수백~수천 개를 그려야 하므로 인스턴싱은 필수.

### 구현 순서 (예정)
1. 플랫 셰이딩 (현재)
2. 인스턴싱
3. 타일 그리드 + 청크
4. 플레이어 이동

---

## Vulkan 개념 정리

### 왜 Vulkan인가

OpenGL은 드라이버가 렌더링 과정을 대부분 알아서 처리한다.
Vulkan은 드라이버가 아무것도 안 해준다. GPU가 뭘 어떻게 할지 **전부 직접 지정**해야 한다.

대신 얻는 것:
- CPU 오버헤드 최소화
- 멀티스레드 렌더링 지원
- 예측 가능한 퍼포먼스 (저사양에 특히 중요)

---

### Vulkan 초기화 순서

`VulkanContext` 생성자에서 이 순서로 호출된다:

| 순서 | 함수 | 설명 |
|------|------|------|
| 1 | `createInstance()` | Vulkan 자체를 켬. 앱 정보, 사용할 익스텐션 등록 |
| 2 | `setupDebugMessenger()` | Validation Layer 에러/경고를 콘솔에 출력하는 콜백 등록 |
| 3 | `createSurface()` | GLFW 창과 Vulkan을 연결하는 Surface 생성 |
| 4 | `pickPhysicalDevice()` | GPU 선택 (Discrete GPU 우선, 없으면 Integrated) |
| 5 | `createLogicalDevice()` | 선택한 GPU에 논리적 연결 생성, Queue 핸들 획득 |
| 6 | `createSwapchain()` | 화면 출력용 이미지 버퍼 시스템 생성 |
| 7 | `createImageViews()` | 스왑체인 이미지를 읽고 쓸 수 있는 "창구" 생성 |
| 8 | `createRenderPass()` | 어떤 포맷으로, 어떤 순서로 그릴지 명세 |
| 9 | `createGraphicsPipeline()` | 렌더링 파이프라인 전체 설계도 생성 |
| 10 | `createFramebuffers()` | 실제로 그릴 대상 프레임버퍼 생성 |
| 11 | `createCommandPool()` | GPU에 보낼 명령어를 담는 저장소 생성 |
| 12 | `createVertexBuffer()` | 정점 데이터를 GPU 메모리에 업로드 |
| 13 | `createIndexBuffer()` | 인덱스 데이터를 GPU 메모리에 업로드 |
| 14 | `createCommandBuffers()` | 명령어 버퍼 할당 |
| 15 | `createSyncObjects()` | CPU-GPU 동기화 객체 생성 |

---

### 그래픽스 파이프라인

"정점 데이터가 화면 픽셀로 바뀌는 과정의 설계도."
`createGraphicsPipeline()`에서 한 번에 전부 정의하고, **만들고 나면 변경 불가.**
설정이 달라지면 파이프라인을 새로 만들어야 한다.

```
[정점 데이터 (kVertices)]
        ↓
Vertex Input       → Vertex 구조체 레이아웃 등록
                     stride = sizeof(Vertex), location 0 = pos, location 1 = color
        ↓
Input Assembly     → 정점을 어떻게 묶을지
                     TRIANGLE_LIST = 3개씩 삼각형
        ↓
Vertex Shader      → 정점마다 실행 (triangle.vert.spv)
                     현재: pos 그대로 통과, color를 fragment로 전달
        ↓
Rasterization      → 삼각형 내부를 픽셀로 채움
                     cullMode = BACK → 뒷면(카메라 반대) 컬링
        ↓
Fragment Shader    → 픽셀마다 실행 (triangle.frag.spv)
                     현재: vertex shader에서 받은 color를 그대로 출력
        ↓
Color Blend        → 투명도 처리 (현재 블렌딩 없음, 그냥 덮어씌움)
        ↓
[화면]
```

---

### 스왑체인

화면에 바로 그리면 GPU가 그리는 도중 모니터가 읽어버려서 찢김(tearing)이 생긴다.
스왑체인은 이미지를 2~3개 만들어 교대로 사용해 이 문제를 해결한다.

```
이미지 A: GPU가 렌더링 중
이미지 B: 모니터에 출력 중
→ A 완성되면 swap → A가 화면에, B는 다음 프레임 렌더링 대상
```

- **FIFO** (vsync): 출력 완료 후 교체. 찢김 없음, 레이턴시 있음
- **Mailbox** (triple buffer): 새 프레임이 오면 대기 중인 것 교체. 레이턴시 낮음

현재 코드에서는 Mailbox 우선, 없으면 FIFO로 폴백한다.

**창 크기가 바뀌면** 스왑체인 전체를 재생성해야 한다 → `recreateSwapchain()`.

---

### 버텍스 버퍼

셰이더에 하드코딩하던 정점 데이터를 CPU → GPU 메모리로 옮기는 과정.

```cpp
vkMapMemory(...);                          // GPU 메모리를 CPU 주소 공간에 매핑
memcpy(data, kVertices.data(), size);      // 복사
vkUnmapMemory(...);                        // 매핑 해제
```

**메모리 종류:**

| 종류 | 설명 | 현재 사용 |
|------|------|-----------|
| `HOST_VISIBLE` | CPU가 직접 읽고 쓸 수 있음. 느림 | ✅ 현재 |
| `DEVICE_LOCAL` | GPU 전용 메모리. 빠르지만 CPU 직접 접근 불가 | 나중에 |

**현재 엔진 적용 상태:**
정적 데이터(메시, 인덱스)는 **Staging Buffer** 방식을 도입하여 `DEVICE_LOCAL`에 배치해 렌더링 성능을 극대화했고, 매 프레임 값이 변하는 동적 데이터(인스턴스 버퍼)는 `HOST_VISIBLE`에 남겨두어 CPU-GPU 간 데이터 전송 오버헤드를 최소화하는 투트랙 전략을 사용 중이다. 전송 완료 대기는 `vkQueueWaitIdle` 대신 `VkFence`를 사용해 해당 전송만 선택적으로 대기한다.

---

### 인스턴싱 개념

같은 메시를 여러 위치에 그릴 때, 드로우콜을 N번 호출하는 대신 인스턴스 데이터를 GPU에 한 번에 올리고 `instanceCount=N`으로 1번만 호출한다.

```
// 일반 방식: 드로우콜 N번
for each tile:
    updateUBO(tilePos)
    vkCmdDrawIndexed(..., 1, ...)

// 인스턴싱: 드로우콜 1번
uploadInstanceBuffer(allPositions)
vkCmdDrawIndexed(..., N, ...)
```

버텍스 버퍼(binding 0)는 `VERTEX`당 한 번 읽고,
인스턴스 버퍼(binding 1)는 `INSTANCE`당 한 번 읽는다.
셰이더에서 두 데이터를 합쳐 최종 위치를 계산한다.

타일 수가 늘어도 드로우콜은 1번이므로 CPU 부하가 거의 없다.

---

### 플랫 셰이딩 개념

스무스 셰이딩은 정점 사이 법선을 보간해서 면이 부드럽게 보인다.
플랫 셰이딩은 보간 없이 삼각형 전체가 하나의 색상 — 로우폴리 스타일의 핵심.

GLSL `flat` qualifier를 쓰면 provoking vertex(삼각형의 첫 번째 정점)의 값을 그대로 사용한다.

```
// 스무스: 정점마다 다른 법선 → 면 안에서 보간
out vec3 fragNormal;

// 플랫: 보간 없음 → 면 전체 동일
flat out vec3 fragNormal;
```

플랫 셰이딩에서는 정점을 면끼리 **공유할 수 없다.**
같은 꼭짓점이라도 면마다 법선이 다르기 때문에 별도 정점이 필요하다.
→ 큐브: 8 정점(스무스) → 24 정점(플랫, 면 6 × 4)

**Lambert 조명:**
```
float diff  = max(dot(normal, lightDir), 0.0);
float light = ambient + diff * diffuseStrength;
color       = baseColor * light;
```

---

### 궤도 카메라 개념

구면 좌표계로 카메라 위치를 계산한다:

```
camPos.x = target.x + distance * cos(pitch) * cos(angle)
camPos.y = target.y + distance * cos(pitch) * sin(angle)
camPos.z = target.z + distance * sin(pitch)
view = lookAt(camPos, target, up)
```

- `angle` (Q/E로 제어): 수평 공전 각도
- `pitch` (고정): 내려다보는 각도
- `distance` (고정): 타겟까지 거리

자유 시점 카메라와 달리 항상 타겟을 바라보므로,
플레이어 위치만 `m_orbitTarget`에 넘기면 카메라가 자동으로 따라간다.

---

### Depth Buffer

3D에서 여러 오브젝트가 겹칠 때 어느 픽셀이 앞에 있는지 판별하는 버퍼.
없으면 나중에 그려진 오브젝트가 무조건 앞에 나온다 (화가 알고리즘 문제).

```
픽셀을 그릴 때:
  새 픽셀의 depth < 저장된 depth  → 그리고 depth 갱신
  새 픽셀의 depth >= 저장된 depth → 버림
```

Depth Buffer는 스왑체인 이미지와 같은 크기여야 하므로 창 리사이즈 시 함께 재생성한다.

Render Pass에 depth attachment를 추가하고, Framebuffer에 depth image view를 연결하고,
Pipeline에 `VkPipelineDepthStencilStateCreateInfo`로 depth test를 활성화해야 한다.
세 군데 모두 연결해야 작동한다.

---

### UBO와 Descriptor

**UBO (Uniform Buffer Object)** 는 매 프레임 CPU에서 GPU 셰이더로 데이터를 넘기는 방법이다.
Vertex Buffer가 "정점마다 다른 데이터"라면, UBO는 "모든 정점에 공통으로 적용되는 데이터"다.

```
CPU (C++ 코드)          GPU (셰이더)
UniformBufferObject  →  layout(binding=0) uniform UniformBufferObject { ... } ubo;
  model matrix            gl_Position = ubo.proj * ubo.view * ubo.model * pos;
  view  matrix
  proj  matrix
```

Vulkan에서 UBO를 셰이더에 넘기려면 **Descriptor** 시스템을 거쳐야 한다:

```
Descriptor Set Layout  → "binding 0에 UBO가 있다"는 설계도
Descriptor Pool        → Descriptor Set을 찍어낼 메모리 풀
Descriptor Set         → 실제 버퍼와 셰이더 바인딩을 연결하는 객체
```

파이프라인 레이아웃에 Descriptor Set Layout을 등록하고,
`vkCmdBindDescriptorSets`로 드로우 전에 바인딩한다.

**MVP 행렬:**

| 행렬 | 역할 |
|------|------|
| Model | 오브젝트를 월드 공간에 배치 (이동, 회전, 스케일) |
| View | 카메라 위치/방향에 따라 월드를 카메라 공간으로 변환 |
| Projection | 카메라 공간을 클립 공간으로 변환 (원근감 적용) |

`gl_Position = proj * view * model * vertex` 순서로 곱한다 (오른쪽부터 적용).

**GLM과 Vulkan의 Y축 차이:**
GLM은 OpenGL 기준으로 만들어져서 Y축이 위가 양수다.
Vulkan은 Y축이 위가 음수(화면 아래가 +Y). 그래서 `proj[1][1] *= -1`로 Y를 뒤집는다.
이 Y반전으로 winding order가 뒤집히므로 `frontFace = COUNTER_CLOCKWISE`로 설정한다.

---

### 인덱스 버퍼

정점을 재사용해서 메모리를 절약하는 방식.

```
삼각형 2개로 사각형:
  정점 버퍼만 쓰면 → A B C A C D (6개, A·C 중복)
  인덱스 버퍼 쓰면 → 정점 A B C D (4개) + 인덱스 [0,1,2, 0,2,3]
```

메시가 복잡해질수록 절약량이 커진다. 3D 큐브(꼭짓점 8개, 삼각형 12개)부터 체감된다.

---

### 매 프레임 루프 (`drawFrame`)

```
1. vkWaitForFences          → 이전 프레임(같은 슬롯)이 끝날 때까지 대기
2. vkAcquireNextImageKHR    → 스왑체인에서 "지금 그릴 이미지" 가져오기
3. recordCommandBuffer       → GPU에 보낼 명령어 기록
      vkCmdBeginRenderPass   → 렌더 패스 시작, 배경색 클리어
      vkCmdBindPipeline      → 사용할 파이프라인 지정
      vkCmdBindVertexBuffers → 정점 버퍼 바인드
      vkCmdBindIndexBuffer   → 인덱스 버퍼 바인드
      vkCmdDrawIndexed       → "인덱스 기준으로 그려라"
      vkCmdEndRenderPass     → 렌더 패스 종료
4. vkQueueSubmit             → GPU에 명령 제출
5. vkQueuePresentKHR         → 완성된 이미지를 화면에 출력
6. m_currentFrame = (m_currentFrame + 1) % 2  → 다음 슬롯으로
```

**`MAX_FRAMES_IN_FLIGHT = 2`**

CPU와 GPU가 서로 다른 프레임을 동시에 처리한다.
CPU가 프레임 1 명령을 기록하는 동안 GPU는 프레임 0을 렌더링 중. GPU가 놀지 않아서 효율적.

**동기화 객체:**

| 객체 | 용도 |
|------|------|
| `Semaphore (imageAvailable)` | 스왑체인 이미지 준비 완료 신호 |
| `Semaphore (renderFinished)` | 렌더링 완료 신호 (present 전에 대기) |
| `Fence (inFlight)` | CPU가 GPU 완료를 기다릴 때 사용 |

세마포어는 GPU-GPU 동기화, 펜스는 CPU-GPU 동기화에 사용한다.
