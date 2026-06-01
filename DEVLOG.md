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

### Frustum Culling (청크 단위)
- `renderer/Frustum.h` 신규 생성 — GLM만 의존하는 독립 구조체.
- **Gribb & Hartmann 방법**: `viewProj` 행렬 행 조합으로 6개 평면(left/right/bottom/top/near/far)을 O(1)에 추출. 정규화 생략 — 부호 판정만 필요하므로 불필요한 sqrt 없음.
- `containsAABB(min, max)`: 각 평면에 대해 "positive vertex"(법선 방향으로 가장 먼 꼭짓점)를 구해 평면 바깥이면 즉시 `false` 반환 — 최악 6번 dot product.
- 청크 AABB: `min={cx*16, cy*16, 0}`, `max={(cx+1)*16, (cy+1)*16, 8}`.
- `drawFrame`에서 `camera.viewProj()`로 매 프레임 frustum 갱신, `recordCommandBuffer`에서 각 청크 draw call 전 AABB 테스트 → 시야 밖 청크는 draw call 자체가 발생하지 않음.
- 현재 청크가 (0,0) 하나뿐이라 실측 효과 없음 — 청크 수가 늘어나면 즉시 작동.

### 청크 사이즈 확장 (16→32) 및 맵 확대
- `CHUNK_SIZE 16 → 32` — 청크당 타일 수 256 → 1024 (4배). 로딩 반경 3청크 기준 96×96 → 192×192 타일.
- 마인크래프트 청크(16×16) 대비 4배 크기 — 농사 게임 특성상 플레이어가 넓은 농장 안에서 활동하므로 청크당 타일 수가 많을수록 청크 경계를 덜 넘어 dirty 재빌드 빈도 감소.
- 초기 맵 10×10 → 32×32 전체로 확대. 좌상단 물 코너, 두 곳 흙 패치(잠재적 농지), 두 곳 돌 패치(채굴 영역), 나머지 잔디.
- 플레이어 시작 위치 `{5,5,1}` → `{15,15,1}` (32×32 맵 중앙).

### 윗면/옆면 색상 분기 (top/side color)
- `InstanceData`에 `sideColor` 추가 — 기존 `color` → `topColor + sideColor` 두 채널.
- `World::tileSideColor(TileType)` 추가 — GRASS는 흙 갈색, DIRT는 짙은 갈색, STONE은 짙은 회색, WATER는 짙은 파랑.
- Vulkan pipeline vertex input: attribute 4개 → 5개 (`instanceSideColor` location=4 추가).
- `buildChunkBuffer`: `tileColor` + `tileSideColor` 함께 인스턴스 버퍼에 기록.
- vertex shader: `instanceTopColor / instanceSideColor` 두 색상 fragment로 전달.
- fragment shader: `step(0.9, fragNormal.z)`로 윗면 판별 → `mix(sideColor, topColor, isTop)`으로 색상 선택.
- 텍스처 전환 시 `topColor/sideColor` → `topTexId/sideTexId`로 교체만 하면 되는 구조 — 호환성 확보.

### Hidden Face Culling + 청크 메시 생성
- **인스턴싱 → 청크별 동적 메시**로 전환 — 인접 타일이 있는 면은 버텍스 버퍼에 추가하지 않음.
- `ChunkVertex { pos, normal, color }` 신규 구조체 — color는 윗면이면 topColor, 옆/아랫면이면 sideColor로 빌드 시 구워짐.
- 6면 정의 테이블(`kFaces`) — 각 면의 4개 로컬 정점 오프셋, 법선, 이웃 오프셋(neighbor check 방향), isTop 플래그를 정적 배열로 정의.
- `buildChunkBuffer`: 각 타일 6방향 이웃 타일을 `World::getTile`로 조회 → AIR이면 해당 면의 버텍스 4개+인덱스 6개 추가, AIR이 아니면 스킵.
- 인덱스 타입 `uint16` → `uint32` — 32×32×8 청크 최악 케이스 294,912 인덱스로 uint16 한계(65535) 초과.
- `chunk.vert` / `chunk.frag` 신규 셰이더 — 인스턴스 속성 없이 per-vertex color 사용, fragment는 Lambert만.
- `m_chunkPipeline` 신규 파이프라인 — binding 1개(ChunkVertex), no instancing. 기존 `m_pipeline`(플레이어/셀렉터 인스턴싱)과 공존.
- `recordCommandBuffer`: 청크는 `m_chunkPipeline`으로 draw, 이후 `m_pipeline`으로 전환하여 플레이어/셀렉터 draw.
- 청크 `ChunkRenderData`: 단일 버퍼 → `vertexBuffer + indexBuffer` 쌍으로 교체.

### 다단계 지형 초기 맵
- `World.cpp` 생성자에 Z=1, Z=2 레이어 추가 — 나머지 시스템(Hidden Face Culling, top/side 색상, 3D 그리드) 변경 없음.
- 행별 x 범위 테이블(`Row { y, x0, x1 }`)로 불규칙한 타원형 언덕 정의 — Z=1 13개 행, Z=2 7개 행.
- Z=1 언덕: 맵 상단 중앙부, Z=2 정상: Z=1 안쪽 더 작은 면적. Z=2 아래엔 반드시 Z=1이 있어 부유 타일 없음.
- 결과: 절벽 옆면 갈색(sideColor), 정상 초록(topColor), Hidden Face Culling으로 내부 면 자동 제거.

### 3D-Aware Collision
- `canOccupy` in `GameState.cpp`: added body-height check — destination tile at `z+1` must be AIR in addition to `isWalkable(x, y, z)`.
- Previously only the ground tile (Z=0) was checked; player could walk through elevated blocks at Z=1/Z=2.
- Step-up logic (auto-climb 1 block) and slope movement deferred to later gameplay pass.

### Chunk Load/Unload + Procedural Terrain
- `TerrainGen.h/cpp` (new): deterministic hash-based FBM noise (4 octaves, no external library). `generate(cx, cy, chunk)` fills a chunk procedurally.
- Terrain rules: Z=0 always solid (GRASS or DIRT by biome noise), Z=1 hill where height > 0.45, Z=2 stone peak where height > 0.65.
- Two noise channels: `HEIGHT_SCALE=1/32` for elevation shape, `BIOME_SCALE=1/24` for GRASS/DIRT distribution.
- `World::loadChunksAround(cx, cy, radius)` — generates unloaded chunks within radius on demand.
- `World::unloadChunksOutside(cx, cy, radius)` — erases chunks beyond radius from `m_chunks`.
- `VulkanContext::rebuildDirtyChunks()` — before rebuilding, frees GPU buffers for chunks no longer in `m_world.chunks()`.
- `main.cpp` — detects player chunk change each frame; load radius=3, unload radius=4.
- Handcrafted initial map removed; `World` constructor is now empty — all terrain generated on demand.

### Block Placement/Destruction
- `PlayerInput::rightClick` added; `InputManager` polls `GLFW_MOUSE_BUTTON_RIGHT`.
- `GameState::update` signature changed from `const World&` to `World&` to allow tile mutation.
- Left click → `world.setTile(target, TileType::AIR)` (destroy). Right click → `world.setTile(target.xy, z+1, TileType::STONE)` (place on top).
- Target tile Z improved: after XY raycast hit, scans from `CHUNK_DEPTH-1` down to find topmost non-AIR tile — elevated blocks are now selectable. `delta.z` clamped ±1 instead of forced 0.
- GPU sync fix: `vkDeviceWaitIdle` called before destroying chunk buffers in `buildChunkBuffer` and before unloaded-chunk cleanup in `rebuildDirtyChunks` — prevents VUID-vkDestroyBuffer-buffer-00922 when tile changes trigger immediate buffer rebuild.

### Terrain Variety (Water / Trees / Biome)
- `TileType` extended: `WOOD`, `LEAVES` added (indices 5, 6); color + side-color tables updated to match.
- Water: tiles where height noise < 0.28 become `WATER` at Z=0 with nothing above (low basins / lakes).
- Biome channel (`BIOME_SCALE = 1/24`): drives GRASS vs DIRT ground and tree density. Dry biome → DIRT patches.
- Trees (`placeTrees`): on flat GRASS in forest biome (b > 0.58), sparse hash threshold (> 0.90). Trunk WOOD at Z=1/Z=2, 3x3 LEAVES canopy at Z=3, single LEAVES top at Z=4.
- Trunk kept ≥2 tiles from chunk edge so the canopy fits inside the chunk — avoids cross-chunk writes (chunks generate independently).
- NOTE: voxel trees are a placeholder. Organic props (trees/crops/rocks) will move to a separate low-poly model layer; terrain stays voxel.

### Hotbar UI (custom quad rendering)
- Dedicated UI pipeline (`m_uiPipeline`) — separate from 3D: depth test/write off (always on top), alpha blending on, cull none, no descriptor sets (vertices already in NDC).
- `UIVertex { vec2 pos; vec4 color }`, `ui.vert`/`ui.frag` (passthrough + Lambert-free flat color).
- `m_uiPipelineLayout` is empty (no descriptors) — UI is fully screen-space.
- `m_uiBuffer`: persistently mapped, rebuilt each frame in `updateHotbar`. Capacity 256 verts (panel + slots + highlight ~66).
- `updateHotbar`: pixel→NDC conversion using current swapchain extent → slots stay correct size regardless of aspect. Centered bottom: panel bg, yellow highlight behind selected slot, 9 slot fills.
- Drawn last in `recordCommandBuffer`, before `vkCmdEndRenderPass`.

### Hotbar Block Selection
- `Window`: scroll callback + `consumeScrollY()` (accumulate yoffset, read-and-reset).
- `InputManager` refactored from `GLFWwindow*` to `Window&` so it can read scroll. Polls number keys 1..9 (`GLFW_KEY_1 + i`) and scroll direction.
- `PlayerInput`: `selectSlot` (-1 or 0..8) and `scrollDelta` (±1) added.
- `GameState`: owns `m_selectedSlot` + `m_palette[9]` (GRASS/DIRT/STONE/WOOD/LEAVES/WATER, rest AIR). Number key sets slot, scroll wraps modulo. Right-click places `palette[selected]` instead of hardcoded STONE (AIR slots = no-op).
- `HOTBAR_SLOTS = 9` moved to `Types.h` as shared constant.
- `drawFrame` takes `(hotbarSelected, palette)`; `updateHotbar` draws each slot's block color as an inner icon via `World::tileColor`.

### Ambient Occlusion (per-vertex voxel AO)
- Standard 0fps voxel AO: each face vertex checks its 3 corner neighbors (2 edge-adjacent + 1 diagonal) in the air layer adjacent to the face; more occluders → darker.
- AO level 0..3 maps to brightness `{0.5, 0.7, 0.85, 1.0}`, multiplied into the vertex color at build time — no new vertex attribute, no extra render pass.
- Shader change: `fragColor` switched from `flat` to smooth interpolation so per-vertex AO blends across the face; `fragNormal` stays `flat` (constant per face, used for Lambert). Vertices aren't shared across faces, so no cross-face color bleed.
- Generic per-face computation: tangent axes derived from the face normal (the two axes where normal is 0), corner direction from the vertex's local position sign.
- **Caching (perf):** AO sampling would otherwise call `World::getTile` (hashmap lookup) ~hundreds of thousands of times per chunk → startup hitch. Fixed by building a padded `(CHUNK_DEPTH+2)×(CHUNK_SIZE+2)²` local copy once per chunk: interior copied straight from `chunk.tiles`, only the 1-tile border ring hits `getTile` (~3.4k vs ~hundreds of thousands). Face-cull and AO then index the local array. Confined to `buildChunkBuffer`; `World` untouched.

### Object Layer (low-poly tree models)
- Trees moved off the voxel grid into a separate **object/prop layer** — terrain stays voxel, organic props become low-poly models. Reused later for crops/rocks/items.
- `Chunk` gains `std::vector<Object> objects` (`Object { pos, scale, rot, type }`); `TerrainGen::placeTrees` now pushes tree objects instead of writing WOOD/LEAVES voxels. WOOD/LEAVES tile types kept as player-placeable building blocks.
- Tree mesh (`createTreeMesh`): box trunk + 3 stacked cones (pine), flat-shaded, built once into a shared vertex buffer. Reuses `ChunkVertex` (pos/normal/color).
- Instanced rendering: `ObjectInstance { pos, scale, rot }`, per-chunk instance buffer in `ChunkRenderData`. `object.vert` applies per-instance Z-rotation + scale; fragment shader reused from `chunk.frag`. Scale/rotation varied per tree via hash (less repetition).
- Dedicated `m_objectPipeline` reuses `m_pipelineLayout` (same UBO descriptor). `cullMode = NONE` — procedural cone/box winding isn't guaranteed outward-facing, so draw both sides (overdraw negligible for small meshes).
- Drawn per chunk after chunk meshes, reusing the existing frustum-cull AABB test.
- NOTE: trees no longer block movement (objects have no collision). Deferred to Phase 3 (tool system / object interaction).

### In-game Time System
- `DAY_DURATION = 120.0f` seconds per in-game day (adjustable constant in `GameState.h`).
- `GameState` gains `m_time` (total elapsed seconds), `m_day` (int, increments each full day), `m_timeOfDay` (0.0 = midnight → 0.5 = noon → 1.0 = midnight). All three updated in `update()` each frame.
- `day()` and `timeOfDay()` accessors exposed for the farming system (crop growth checks against `m_day`).
- Sky clear color in `VulkanContext` now lerps across 4 keyframes keyed on `timeOfDay × 4`: midnight (dark navy), dawn (pink/orange), noon (sky blue), dusk (orange/red). Stored in `m_skyColor[4]`, set in `drawFrame()`, read in `recordCommandBuffer()`.
- No dynamic lighting change yet — terrain brightness stays constant. Ambient light tie-in deferred to Phase 6 (rendering polish).

### Item/Tool System + Inventory UI
- `ItemType` enum added to `Types.h`: `NONE`, `BLOCK_GRASS/DIRT/STONE/WOOD/LEAVES/WATER`, `TOOL_HOE`, `TOOL_AXE`, `COUNT`. Inline helpers: `isBlock()`, `isTool()`, `itemToTile()`, `itemColor()`.
- Inventory layout constants (`INV_COLS=4`, `INV_ROWS=2`, `INV_SLOT_SIZE`, `INV_GAP`, `INV_PAD`) in `Types.h` — shared between `GameState` (click detection) and `VulkanContext` (rendering) so the math stays in sync.
- `GameState`: hotbar palette changed from `TileType[9]` to `ItemType[9]`. Default slots: 6 block types + HOE + AXE + NONE. Right-click with a tool selected does nothing (actual tool actions added in Phase 3-6). World interaction (click/raycast) suppressed while inventory is open.
- `I` key toggles inventory; edge-detected with `m_prevToggleInv` to avoid repeated fires on hold.
- Inventory click: mouse position checked against each slot rect using the shared layout constants. Matching slot's `ItemType` written to `m_palette[m_selectedSlot]`.
- `VulkanContext`: `updateHotbar()` now reads `ItemType` and calls `itemColor()` directly — no more `World::tileColor` in the UI path. Inventory open → full-screen dim quad + panel background + 4×2 item grid rendered on top of hotbar. UI vertex buffer expanded from 256 to 512.

### Farming System (Farmland / Seeds / Growth)
- New `TileType`: `FARMLAND` (dark moist brown), `WHEAT` (growth-stage color).
- New `ItemType`: `SEED_WHEAT` (replaces the last NONE slot in the default palette).
- **HOE right-click** on GRASS/DIRT → replaces that tile with FARMLAND (same Z, in-place tilling).
- **SEED_WHEAT right-click** on FARMLAND → places WHEAT at Z+1, initializes `TileState { growthStage=0, lastUpdatedDay=currentDay }`.
- **Growth**: `World::growthTick(currentDay)` called once per in-game day from `GameState`. Iterates all loaded WHEAT tiles; if `currentDay - lastUpdatedDay >= 2`, increments `growthStage` (max 3) and marks chunk dirty.
- **WHEAT color** is growth-stage-aware: `World::tileColor(TileType, uint8_t growthStage)` returns pale green → yellow-green → yellow → golden. `buildChunkBuffer` reads `chunk.states[z][ly][lx].growthStage` for WHEAT tiles.
- **Harvest**: left-click on any WHEAT → AIR (existing destroy logic). FARMLAND below is unaffected.
- **Walkability**: `isWalkable` excludes WHEAT (crop is not solid ground). `canOccupy` treats WHEAT as passable for the body-height check so the player can walk over farmland with growing crops.

### World Save / Load
- Binary format (`save.dat`): magic `"PFRM"` + version byte + player position (3×float) + game time (float) + chunk count + per-chunk data.
- Per chunk: `cx/cy` (int32×2) + raw `tiles[8][32][32]` (8 192 B) + `growthStage[8][32][32]` (8 192 B) + `lastUpdatedDay[8][32][32]` (32 768 B) ≈ 49 KB per chunk.
- Only **modified** chunks are saved. `Chunk::modified` is set by `setTile` / `setTileState`; procedural generation writes directly to `chunk.tiles` and does not set the flag.
- `m_modifiedUnloaded` map: when `unloadChunksOutside` would erase a modified chunk, it moves it here instead. `loadChunksAround` checks this map before calling `TerrainGen` — so modified chunks survive unload/reload cycles without hitting disk.
- On startup, `world.load()` pre-populates `m_modifiedUnloaded`; `loadChunksAround` then picks those up in place of fresh generation.
- Ctrl+S triggers `world.save()`; no auto-save yet (planned for Phase 5 settings screen).
- Object layer (tree instances) is not saved — trees are deterministically re-generated by `TerrainGen`, so no data loss.

### Optimization Pass
**growthTick catch-up fix**
- `if` → `while` in `World::growthTick`. Previously, crops in a chunk that had been unloaded (moved to `m_modifiedUnloaded`) could only advance one growth stage on the first tick after reloading, regardless of how many days had passed. The while loop now drains all pending stages in one tick: `while (growthStage < 3 && currentDay - lastUpdatedDay >= GROWTH_DAYS)`.

**growthTick visual update fix**
- `bool changed = false` was declared in `growthTick` but never set to `true` inside the while loop — `if (changed) chunk.dirty = true` never fired.
- Crops advanced in memory (growthStage incremented, lastUpdatedDay updated) but the chunk mesh was never re-queued for GPU rebuild, so the color change from green → golden never appeared on screen.
- Fix: add `changed = true` inside the while loop body.

**Deferred deletion queue (vkDeviceWaitIdle removal)**
- Added `DeferredDelete { VkBuffer, VkDeviceMemory, uint64_t frame }` and `m_deletionQueue` + `m_frameCount` to `VulkanContext`.
- `deferDestroy(buf, mem)` pushes a buffer onto the queue tagged with the current frame.
- `drawFrame()` increments `m_frameCount` and flushes queue entries older than `MAX_FRAMES_IN_FLIGHT` frames — at that point the GPU is guaranteed to have finished reading them.
- `buildChunkBuffer()`: removed `vkDeviceWaitIdle` + immediate `vkDestroyBuffer`; replaced with two `deferDestroy` calls for vertex and index buffers.
- `rebuildDirtyChunks()`: removed `vkDeviceWaitIdle` + immediate destroy for unloaded chunk buffers; replaced with `deferDestroy` for vertex, index, and object-instance buffers.
- Destructor flushes the remaining queue after `waitIdle()`.
- Eliminates full GPU pipeline stalls on every tile change and chunk unload. Most impactful when `growthTick` dirties multiple chunks simultaneously on a day transition.
### VulkanContext 파일 분리 (리팩토링)
- `VulkanContext.cpp` (~1716줄) → 4개 .cpp + 1개 비공개 헤더로 분리. 클래스 인터페이스(`VulkanContext.h`) 변경 없음.

| 파일 | 내용 |
|------|------|
| `VulkanContext.cpp` (~275줄) | 생성자/소멸자, waitIdle, deferDestroy, createBuffer, copyBuffer, createImage, cleanupSwapchain, recreateSwapchain 등 공유 헬퍼 |
| `VulkanContext_Init.cpp` (~970줄) | 모든 초기화 `create*` 함수 (device, swapchain, pipeline, descriptor, buffer 생성 등) |
| `VulkanContext_Frame.cpp` (~264줄) | drawFrame, recordCommandBuffer, updateUniformBuffer, updateHotbar, updateSelectorInstanceBuffer |
| `VulkanContext_Chunk.cpp` (~173줄) | buildChunkBuffer, buildChunkObjectBuffer, rebuildDirtyChunks |
| `VulkanContext_Private.h` (~107줄) | 파일 간 공유 상수/타입 (kVertices, kIndices, UniformBufferObject, kEnableValidation, debug helpers) |

- 분리 기준: Init = 시작 시 1회 호출, Frame = 매 프레임, Chunk = 월드 지오메트리. 헬퍼 함수는 Init/Frame/Chunk 양쪽에서 쓰이므로 core 파일에 유지.
- `buildChunkObjectBuffer`에서 `vkDestroyBuffer`를 즉시 호출하던 버그 수정 → `deferDestroy()`로 교체. GPU가 아직 읽는 도중 버퍼를 파괴해 `VUID-vkDestroyBuffer-buffer-00922` validation error + 블록 설치/파괴 직후 크래시가 발생하던 문제 해결.

### Dynamic Sun Lighting
- `UniformBufferObject`에 `vec4 lightDir` 추가 (xyz = 태양 방향, w = dayFactor 0..1). 기존 192 bytes → 208 bytes.
- `updateUniformBuffer()`가 `timeOfDay`를 받아 매 프레임 태양 위치 계산: `elevation = sin(tod × π)` (자정=0, 정오=1), `azimuth = tod × 2π` (하루 동안 360° 회전). sunDir은 두 값으로 구성한 단위 벡터.
- Descriptor set layout의 UBO stage flags: `VERTEX_BIT` → `VERTEX_BIT | FRAGMENT_BIT`. Fragment shader에서 UBO를 읽으려면 필수.
- `chunk.frag` / `triangle.frag`에 UBO 바인딩 추가. Lambert diffuse에 `dayFactor` 곱해 낮엔 full lighting, 밤엔 diffuse=0. ambient는 `mix(0.15, 0.3, dayFactor)`로 보간 — 밤엔 0.15(달빛), 낮엔 0.3.
- `chunk.vert`, `object.vert`, `triangle.vert` UBO 구조체에 `vec4 lightDir` 선언 추가 (C++ 쪽 버퍼 크기와 일치).
- 결과: 시간 흐름에 따라 태양 방향이 회전하고 밤이 되면 어두워짐. Shadow Map의 light matrix 기반이 되는 단계.

### Shadow Map Infrastructure
- Shadow map 전용 Vulkan 리소스 생성 (`createShadowResources()`). 스왑체인과 무관하게 한 번만 생성되며 리사이즈 시 재생성 불필요.
- `VkImage` (1024×1024, depth format, `DEPTH_STENCIL_ATTACHMENT | SAMPLED` usage) + `VkDeviceMemory` + `VkImageView` (depth aspect).
- Shadow 전용 `VkRenderPass`: color attachment 없이 depth attachment 1개만. `loadOp=CLEAR`, `storeOp=STORE`, `finalLayout=DEPTH_STENCIL_READ_ONLY_OPTIMAL` — 이후 main pass의 fragment shader가 샘플링할 수 있는 상태로 전환.
- Subpass dependency 2개: (1) 이전 프레임의 shadow 샘플링 → 이번 depth write 순서 보장, (2) depth write 완료 → main pass fragment 샘플링 순서 보장. 이 두 dependency가 없으면 GPU가 shadow map을 읽는 도중 덮어쓰는 race condition 발생.
- `VkFramebuffer` (shadow image view 연결).
- 소멸자에 5개 리소스 정리 추가.
- 이 단계에서 화면 변화 없음. 다음 단계(shadow pipeline + shadow pass 실행)의 기반.

### Shadow Pass Pipeline
- `shadow.vert` 신규 셰이더: push constant `mat4 lightMVP` 하나만 받아 위치 변환. UBO 불필요 — light matrix는 매 프레임 바뀌고 double-buffering이 필요 없어 push constant가 적합 (최소 보장 크기 128 bytes, mat4 = 64 bytes).
- `createShadowPipeline()`: fragment shader 없는 depth-only 파이프라인. `ChunkVertex` binding (stride 36 bytes) + location 0(pos)만 선언. `cullMode = FRONT_BIT` (back-face shadow map에서 peter-panning 억제), `depthBias` 상수 2.0 + slope 1.5 (shadow acne 방지). viewport/scissor 고정 1024×1024.
- `m_shadowPipelineLayout`: push constant range 1개(VERTEX stage, 64 bytes). descriptor set 없음.
- `drawFrame`에서 매 프레임 light matrix 계산: `elevation = sin(tod×π)`, `azimuth = tod×2π`로 sunDir 구성 → `glm::lookAt(player + sunDir×150, player, Z_UP)` + `glm::ortho(±60, ±60, 1, 300)` + Vulkan Y-flip.
- `recordCommandBuffer` 첫 부분에 shadow pass 삽입: shadow render pass begin → pipeline bind → push constant → 청크 메시 전체 draw → render pass end. 이후 기존 main pass 실행.
- 결과: 매 프레임 청크 메시의 깊이가 태양 시점으로 shadow map에 기록됨. 화면 변화 없음 — 다음 단계(descriptor에 shadow sampler 추가 + fragment shader에서 비교)에서 실제 그림자가 보임.

### Shadow Sampler + Descriptor 연결
- `createShadowSampler()`: `VK_COMPARE_OP_LESS_OR_EQUAL` comparison sampler. `CLAMP_TO_BORDER` + `FLOAT_OPAQUE_WHITE`(depth=1.0) — shadow map 밖 영역은 항상 lit 처리. `LINEAR` filter로 PCF 효과(비교 결과를 4 texel bilinear 평균).
- `createDescriptorSetLayout()`: binding 1 추가 — `COMBINED_IMAGE_SAMPLER`, `FRAGMENT_BIT`.
- `createDescriptorPool()`: `COMBINED_IMAGE_SAMPLER` pool size 추가 (`MAX_FRAMES_IN_FLIGHT`개).
- `createDescriptorSets()`: binding 1에 shadow image view(`DEPTH_STENCIL_READ_ONLY_OPTIMAL`) + sampler 바인딩. `vkUpdateDescriptorSets`에 write 2개로 UBO + sampler 동시 업데이트.
- 이 단계에서 화면 변화 없음. 다음 단계(chunk.vert에서 light space 좌표 출력 + chunk.frag에서 `sampler2DShadow`로 비교)에서 실제 그림자가 보임.

### Shadow Rendering
- UBO에 `mat4 lightMVP` 추가 (208 → 272 bytes). `updateUniformBuffer()`에서 `m_lightMVP`를 UBO에 기록.
- `chunk.vert` / `object.vert` / `triangle.vert`: UBO에 `lightMVP` 추가, `fragPosLightSpace = ubo.lightMVP * vec4(worldPos, 1.0)` 출력 (location 2 또는 3).
- `chunk.frag` / `triangle.frag`: `sampler2DShadow shadowMap` (binding 1). `fragPosLightSpace` perspective divide → UV 변환 (`xy * 0.5 + 0.5`) → NdotL 기반 bias(`mix(0.008, 0.001, NdotL)`) 적용 후 `texture(shadowMap, vec3(projCoords.xy, z - bias))` 비교. `shadowFactor = max(shadow, 0.4)` — 그림자 안도 40% 밝기 유지.
- 결과: 지형·나무·플레이어에 그림자가 드리워지고 시간에 따라 방향 변화.

### Shadow Quality Tuning + Depth Fix
- Shadow map 2048×2048 (기존 1024), ortho range ±80 (초기 ±60 → ±45 시도 후 최종 ±80). texel 크기 ≈ 0.078 units/texel.
- `GLM_FORCE_DEPTH_ZERO_TO_ONE` 를 `CMakeLists.txt`에 추가. 근본 원인: GLM `ortho`가 기본적으로 OpenGL 깊이 범위 [-1,1]로 계산 → Vulkan [0,1] 불일치 → 플레이어 근처 geometry가 NDC Z ≈ 0 경계에 걸려 shadow map에서 들어갔다 나갔다 → 이동할 때 그림자 잘림. 수정 후 정상 동작.
- **미결 항목 (추후 처리):**
  - Shadow aliasing (경계 계단 현상): PCF 샘플 수 증가로 개선 가능. 로우폴리 스타일에서 허용 범위 안이므로 렌더링 폴리시 2차 때 검토.

### 태양 방향 수정
- `azimuth = timeOfDay * 2π` → `-timeOfDay * 2π`. 한 줄 수정으로 그림자 회전 방향이 더 자연스러워짐.

### Fog (안개)
- UBO에 `vec4 fogColor` 추가 (272 → 288 bytes). `updateUniformBuffer()`에서 `m_skyColor`를 그대로 기록 → 안개 색이 시간대 sky color와 자동 동기화 (낮=파랑, 노을=주황 등).
- `chunk.vert` / `object.vert` / `triangle.vert`: view-space 깊이 계산 `fragViewDepth = -(ubo.view * worldPos).z` 추가 (각 location 3, 3, 4).
- `chunk.frag` / `triangle.frag`: `FOG_START=27, FOG_END=57` 선형 안개. `fogFactor = clamp((END - depth) / (END - START), 0, 1)` → `mix(fogColor, litColor, fogFactor)`. 안개 범위는 카메라 기준이므로 플레이어 기준 약 7~37 유닛 밖에서 적용.
- 결과: 먼 지형이 하늘색으로 자연스럽게 희미해지고 청크 경계가 가려짐.

### Shadow PCF (3×3)
- `chunk.frag` / `triangle.frag`: 단일 `texture()` 호출 → 3×3 루프로 교체. `texel = 1.0 / 2048.0`, 각 샘플에 같은 bias 적용 후 9로 나눔.
- `LINEAR` 샘플러가 이미 하드웨어 2×2 bilinear PCF를 수행하므로 수동 3×3과 결합하면 샘플링 부드러움 개선.
- 참고: 지형의 계단 모양 그림자는 shadow map 문제가 아니라 블록 지형 자체의 기하학적 계단에서 기인. PCF로 해결 불가, 로우폴리 복셀 스타일에서 자연스러운 특성으로 수용.

### 리사이즈 viewport 수정 (dynamic state)
- 파이프라인이 viewport/scissor를 정적 상태로 구워, 창 리사이즈 후에도 옛 크기로 렌더돼 화면이 한쪽으로 쏠리거나 잘리던 버그 수정.
- 메인 패스 파이프라인 4개(triangle/chunk/ui/object)에 `VK_DYNAMIC_STATE_VIEWPORT` + `VK_DYNAMIC_STATE_SCISSOR` 추가, `recordCommandBuffer`의 메인 패스 시작 직후 `vkCmdSetViewport`/`vkCmdSetScissor`를 `m_swapchainExtent` 기준으로 매 프레임 설정.
- shadow 파이프라인은 2048×2048 고정이라 정적 viewport 유지. `recreateSwapchain`은 파이프라인 재생성 없이도 새 크기에 맞게 렌더됨.

### 프레임별 동적 버퍼 분리 (frame-in-flight 경합 수정)
- player/selector/UI 인스턴스 버퍼가 단일 버퍼라 매 프레임 in-place `memcpy` → frame N의 GPU가 읽는 도중 frame N+1이 덮어쓰는 data race 존재.
- `m_playerInst*` / `m_selectorInst*` / `m_ui*`(buffer·memory·mapped)를 `std::vector`로 전환해 `MAX_FRAMES_IN_FLIGHT`개씩 생성, update/bind를 `[m_currentFrame]`로 분리 (UBO와 동일 패턴).
- selector 정점/인덱스 버퍼는 정적이라 단일 유지. `m_uiVertexCount`는 같은 프레임 내에서 쓰고 그리므로 단일 값 유지.

### present semaphore 이미지별 분리 + imagesInFlight
- present용 `m_renderFinished` semaphore가 frame-in-flight 단위(2개)라, present wait가 끝나기 전에 같은 semaphore가 재signal돼 검증 레이어가 잡던 동기화 위반 수정.
- `m_renderFinished`를 스왑체인 이미지 개수만큼 생성하고 submit signal / present wait를 `[imageIndex]`로 변경. imageAvailable·inFlight fence는 frame-in-flight 단위 유지.
- `m_imagesInFlight`(이미지별 fence 참조) 추가 — acquire한 이미지가 이전 프레임에서 아직 사용 중이면 그 fence를 먼저 대기. `recreateSwapchain`에서 이미지 수 변동에 대비해 semaphore·추적 배열 재생성.

### Frustum near 평면 수정 (Vulkan depth)
- `GLM_FORCE_DEPTH_ZERO_TO_ONE`(Vulkan [0,1] depth)를 쓰는데 Gribb-Hartmann near 평면을 OpenGL [-1,1] 공식(`row(3)+row(2)`)으로 추출하던 버그 수정 → `row(2)`로 변경.
- 나머지 5개 평면(left/right/bottom/top/far)은 clip-space x·y, far 모두 두 깊이 규약에서 동일하므로 그대로.

### 죽은 코드 제거 (updateTargetTile)
- 마우스 레이캐스팅으로 대체돼 호출처가 없던 `GameState::updateTargetTile` 선언/정의 제거. 동작 변화 없음.

### 태양 방향 계산 중복 제거
- `drawFrame`(lightMVP 계산)과 `updateUniformBuffer`(UBO `lightDir`)에서 중복 계산하던 `elevation`/`azimuth`/`sunDir`를 `drawFrame`에서 한 번만 계산해 `m_sunDir`/`m_dayFactor` 멤버에 저장.
- `updateUniformBuffer`는 멤버를 읽기만 하고, 미사용이 된 `timeOfDay` 파라미터 제거. 렌더 결과는 동일.

### 밤 shadow pass 스킵 (최적화)
- 프래그먼트 셰이더가 `dayFactor > 0.01`일 때만 그림자를 샘플링하므로, 밤(태양 지평선 아래)엔 shadow map 청크 렌더를 건너뜀 — 매 프레임 ~수십 개 청크를 태양 시점으로 다시 그리던 비용 절약.
- 렌더패스 begin/end + clear(depth=1.0=전부 lit)는 유지해 이미지 레이아웃 전환과 시작 첫 프레임(밤) 안전성 보장. 무거운 청크 draw 루프만 `m_dayFactor > 0.01f`로 가드.

### object 인스턴스 버퍼 재생성 회피 (최적화)
- 타일 변경·작물 성장 등으로 청크 메시가 리빌드될 때마다 나무 인스턴스 버퍼까지 매번 defer-destroy 후 재생성하던 낭비 제거.
- 나무는 지형 생성 시 한 번 배치된 뒤 불변이므로 `ChunkRenderData::objInstBuilt` 플래그로 청크 로드당 1회만 빌드. 언로드→재로드 시 새 `ChunkRenderData`(플래그 false)로 다시 빌드되어 정상.

### 청크 dirty 리빌드 프레임 분할 (최적화)
- `rebuildDirtyChunks`가 매 프레임 dirty 청크를 전부 빌드하던 것을 프레임당 `MAX_CHUNK_BUILDS_PER_FRAME`(=2)개로 제한. 초과분은 `dirty=true`로 남아 다음 프레임들에 분산.
- 청크 스트리밍(경계 넘을 때 한 줄 ~7개)·작물 성장(날짜 전환 시 다수 청크 동시 dirty) 시 한 프레임에 몰리던 스파이크 완화. 농장 게임 특성상 성장 색 변화는 지연에 둔감해 분산 처리가 적합. N은 튜닝 가능(우선순위/시간예산 방식은 추후).

### 입력 일원화 (InputManager)
- `main.cpp`이 직접 `glfwGetKey`로 읽던 ESC·Q/E·Ctrl+S를 `InputManager::pollInput`으로 이전. `PlayerInput`에 `quit`/`rotateLeft`/`rotateRight`/`saveKey` 추가.
- `Window::close()` 추가로 main이 raw glfw 호출 없이 종료 요청. main은 폴링된 `input`으로 종료/회전/저장(edge-detect) 처리. 동작 동일, 입력 수집이 한 곳으로 통일.

### 파이프라인 생성 보일러플레이트 헬퍼화
- 메인 패스 파이프라인 4개(player/selector·chunk·ui·object)가 복붙하던 shader stage·input assembly·viewport+dynamic·rasterizer·multisample·blend·depth·pipelineInfo 조립을 `createPipeline(PipelineConfig)` 헬퍼 하나로 통합.
- 각 함수는 vertex binding/attribute·cullMode·depthTest·alphaBlend·shader 경로·layout만 `PipelineConfig`로 채워 호출(~430줄 → ~110줄). pipeline layout 생성은 각 함수에 유지, shadow 파이프라인(depth-only·push constant·고정 viewport)은 성격이 달라 미변경. 렌더 결과 동일.

### 디스크 로드 청크 나무 복원 (버그 수정)
- `save.dat`에서 로드된 수정 청크는 오브젝트(나무)가 저장도 재생성도 되지 않아 나무가 사라지던 버그 수정.
- `World::load()`에서 청크마다 `TerrainGen::generate`를 임시 청크에 돌려 `objects`만 가져와 적용. 저장된 타일/상태는 보존하고 나무만 좌표 기반 결정론으로 원래대로 복원. 시작 시 수정 청크 수만큼 1회.

### shadow pass 라이트 프러스텀 컬링 (최적화)
- shadow pass(주간)가 로드된 청크를 전부 그리던 것을, `m_lightMVP`에서 추출한 라이트 프러스텀으로 청크 AABB 컬링. 태양 직교 박스 밖 청크는 어차피 shadow map에 안 잡히므로 그림자 손실 없이 draw call만 감소.
- 메인 패스 컬링과 동일한 `Frustum::extractFrom`/`containsAABB` 재사용. (4번 near 평면 수정 덕에 직교+[0,1] depth에도 정확)

### 나무 그림자 캐스팅
- shadow pass가 청크 메시만 그려 나무가 그림자를 못 드리우던 것 해결. `shadow_object.vert`(인스턴스 scale/rot/pos 변환 + push constant `lightMVP`) + `m_shadowObjectPipeline`(depth-only, `ChunkVertex`+`ObjectInstance`) 추가.
- shadow pass에서 청크 다음으로 나무 인스턴스를 그림 — 2a의 라이트 프러스텀 컬링 재사용, `m_shadowPipelineLayout`(push constant)·`m_shadowRenderPass` 공유. cullMode NONE(나무 메시 비watertight). 시간에 따라 그림자 방향 회전.

### 플레이어 그림자 캐스팅
- 플레이어 큐브도 shadow map에 기록. `shadow_player.vert`(pos + instancePos + push constant `lightMVP`) + `m_shadowPlayerPipeline`(depth-only, `Vertex`+`InstanceData`, cullMode FRONT_BIT=watertight 큐브 피터패닝 억제) 추가.
- shadow pass에서 나무 다음으로 플레이어 cube를 인덱스 draw. 라이트 박스 중심이라 컬링 없음.

### 문서 4종 체계 + 월드 모델 방향 결정
- README/DEVLOG에 더해 `ARCHITECTURE.md`(엔진 구조 + 기술 방향, `[구현됨]`/`[계획]` 태그)와 `DESIGN.md`(게임 기획·비전·스코프) 추가. README 상단에 4종 문서 역할·갱신주기 표.
- 갱신 정책: README=기능 추가 시 / DEVLOG=작업마다 / ARCHITECTURE=구조 변경 시 / DESIGN=방향 변경 시. 각 사실은 한 문서에만(중복·노후화 방지).
- 결정: 월드 모델은 **고정맵 + 절차 레이어** 방향(문서상). 청크=스트리밍 단위라 추후 `WorldSource` 추상화로 전환, 현재 구현은 절차생성 유지.

### 그림자 접지(contact) 튜닝
- 피터패닝(그림자가 geometry에서 떠 보임) 교정. 원인: 파이프라인 depthBias + 셰이더 bias 이중 적용 + front-face culling.
- 단계: ① 셰이더 bias `mix(0.008,0.001)` → `mix(0.0015,0.0003)` ② 파이프라인 depthBias `2.0/1.5` → chunk·player `0/0` ③ chunk·player shadow `cullMode FRONT → NONE`(접지 다이얼의 핵심). 나무는 `1.5/1.2`·NONE 유지(현 느낌 좋음).
- `max(shadow,0.4)`(파스텔 부드러움)는 유지. 잔여 극미세 틈은 shadow map texel 해상도 한계 — 필요 시 ortho 범위 축소/해상도↑로 개선.

### 태양 방위각 180° 스윕 + 방향 오프셋
- 그림자가 하루에 360° 돌던 것(부자연)을, 방위각을 `(timeOfDay-0.5)·π`로 바꿔 **동→남중→서 180°만** 스윕하도록 수정 — 자연스러운 일주 아크.
- `kSunAzimuth` 오프셋(225°)으로 기본 시점에서 그림자 방향을 원하는 각으로 정렬. **태양만 회전**(카메라 구도·이동 기준 불변). 태양 계산은 `drawFrame` 한 곳이라 조명·그림자에 동시 적용.

### 포스트 프로세스 패스 + 컬러 그레이딩
- 씬을 **오프스크린 컬러 타겟**(스왑체인 sRGB 포맷, frame-in-flight별 2개)에 렌더 → **풀스크린 post 패스**가 톤/색 보정 후 스왑체인에 출력. (UI는 현재 씬 패스라 함께 grading됨 — 추후 분리 가능)
- 구조: 기존 main 렌더패스를 **씬 패스**(color finalLayout=`SHADER_READ_ONLY`)로 전환, **post 렌더패스**(스왑체인) 신설, 정점버퍼 없는 풀스크린 삼각형 파이프라인(`post.vert`/`post.frag`) + 오프스크린 샘플 디스크립터. `recreateSwapchain`·소멸자에 오프스크린/디스크립터 재생성·해제 추가. 오프스크린=스왑체인 sRGB라 패스스루(0a)가 픽셀 동일.
- 그레이딩(`post.frag` 상수): exposure·contrast·saturation + split-tone(쿨 그림자/웜 하이라이트, 따뜻한 쪽으로 튜닝) + vignette. 이후 bloom 등 image-space 효과의 토대.

### 물주기 (watering) — 물이 성장을 gate
- `ItemType::TOOL_WATERINGCAN` + `TileState.watered`(일시 상태, 매일 리셋이라 미저장). 물뿌리개 우클릭 → FARMLAND `watered=true`, 짙은(촉촉) 색으로 렌더.
- `growthTick` 재작성: **물 준 farmland 위 WHEAT만 하루 1단계 성장**, 처리 후 farmland 마름(매일 재급수 필요). 기존 날짜기반 catch-up 제거.
- 인벤토리 그리드 `INV_ROWS 2→3`(아이템 전부 노출). 기본 핫바 도끼→물뿌리개로 교체(호미+물뿌리개+씨앗 = 농사 루프 즉시 사용, 도끼는 인벤토리에서).

### UI 숫자 렌더러 + Day HUD (1-B-i)
- 텍스처/폰트 없이 **3×5 도트matrix 숫자**(`pushNumber`)를 기존 UI 색 quad로 렌더. 좌상단에 현재 Day 표시.
- `drawFrame`에 `day` 파라미터 추가, UI 버퍼 512→1024 verts. 이 렌더러를 1-B-ii의 슬롯 개수 표시에 재사용. (인벤토리 키스톤의 선행 단계)

### 인벤토리 모델 — 스택+개수 (1-B-ii)
- `m_palette(ItemType[9])` → `ItemStack{type,count}[27]`(9×3, 핫바=앞 9칸). `GameState::inventory()` 노출, `drawFrame`로 전달.
- 핫바/인벤토리 창을 스택+개수로 렌더(`count>1`이면 숫자, 1-B-i 렌더러 공용). 인벤토리 창(`I`)은 27칸 **읽기전용** 그리드(드래그/재배치는 폴리시로 미룸).
- 시작 아이템: 호미·물뿌리개·씨앗×10·도끼. 씨앗은 심을 때 1 소모(0이면 빈칸), 도구는 무한. 무한 블록 팔레트·인벤토리 클릭-할당 폐지(지형 불변 방향과 일치).
- 미결: 좌클릭 지형 파괴는 아직 잔존(스타듀 아크 ④단계에서 제거 예정).

### 낫 수확 → 인벤토리 획득 (1단계 닫기)
- `ItemType`에 `TOOL_SICKLE`(낫)·`ITEM_WHEAT`(수확된 밀) 추가. `isTool()`에 낫 포함, `itemColor()` 케이스 추가. 시작 핫바 슬롯 4에 낫(키 5).
- `GameState::addItem(ItemType, count)` 신규 — 같은 타입 스택 우선 채우고 없으면 첫 빈 칸, 자리 없으면 false. **③ 자원 채집에서 그대로 재사용할 제네릭 API**.
- 좌클릭 분기 재구성: 대상이 `WHEAT`면 **낫 선택 + `growthStage==3`(완숙) + 인벤토리 여유**가 모두 맞을 때만 `AIR`로 만들고 `addItem(ITEM_WHEAT, 1)`. 미성숙/낫 아님/인벤토리 가득이면 작물 보호(변화 없음). WHEAT 아닌 타일은 기존 파괴 유지(④에서 제거 예정).
- 인벤토리는 save 대상이 아니라 호환성 영향 없음. 이로써 농사 루프(경작→심기→물주기→성장→수확→인벤토리)가 처음으로 완결.

### 작물 아래 흙 물주기 버그 수정
- 작물(WHEAT)이 흙(FARMLAND) 위 칸에 심기는데 타겟 선택이 "가장 위 non-AIR"라 작물이 항상 잡혀, 심은 뒤엔 흙에 재급수가 불가능 → 물-성장 게이트가 사실상 막히던 버그.
- 물뿌리개 분기: 타겟이 WHEAT면 한 칸 아래(`fz = tz-1`)를 보고 FARMLAND면 거기에 급수. 작물 없는 빈 흙은 기존대로 그 자리 급수.

### 드롭 아이템 + 줍기 (③ 채집 인프라 선행)
- `DroppedItem{pos, type, count}`(Types.h) + `GameState::m_drops` 벡터. 수확이 `addItem` 직행 → **바닥에 드롭 스폰**으로 변경(낫으로 완숙 밀 좌클릭 시 작은 큐브가 그 자리에 떨어짐).
- 줍기: `update()`에서 매 프레임 플레이어-드롭 **수평 거리**(반경 0.9) 검사 → 근접 시 `addItem` 성공하면 제거. 인벤토리 가득이면 바닥에 남아 대기.
- 렌더: `kVertices` 0.3배 작은 큐브(`createItemMesh`, DEVICE_LOCAL 1회) + 프레임별 드롭 인스턴스 버퍼(`InstanceData`, `itemColor` 색). **기존 플레이어 인스턴싱 파이프라인·인덱스 버퍼 재사용**, 플레이어 draw 직후 드롭 전체 1 draw call(`MAX_DROPS=256`). 이 드롭/줍기 레이어는 ③ 자원 채집(나무/돌)에서 그대로 재사용.
- 미결: 드롭은 save 대상 아님(바닥에 둔 채 재시작하면 소실). 필요 시 ③에서 저장 검토.

### 오브젝트 렌더링 일반화 (②a, 순수 리팩토링)
- 나무 전용 단일 메시(`m_treeVertex*`) → **타입 인덱싱 메시 레지스트리** `m_objectMeshes[ObjectType::COUNT]`(`ObjectMesh{vbuf,vmem,count}`). `createTreeMesh` → `createObjectMeshes`(타입별 `upload()` 헬퍼).
- 청크 단일 오브젝트 인스턴스 버퍼 → **타입별 그룹** `ChunkRenderData::objGroups`(`{type,buffer,memory,count}`). `buildChunkObjectBuffer`가 `chunk.objects`를 타입별로 묶어 그룹마다 버퍼 생성.
- main/shadow 오브젝트 draw 루프가 그룹을 순회하며 `m_objectMeshes[type]` 메시 + 그룹 버퍼 바인딩. 소멸자·`rebuildDirtyChunks` 정리도 그룹 단위.
- 타입은 여전히 TREE 하나뿐 — **화면·동작 변화 0**(회귀 없음 확인). ②b에서 ROCK + ObjectDef를 얹을 토대.

### ObjectDef 테이블 + ROCK 타입 (②b)
- `ObjectType::ROCK` 추가 + **데이터 주도 `ObjectDef` 테이블**(`objectDef(type)` lookup, Chunk.h): `castShadow/collidable/placeable/harvestTool/dropItem/dropCount`. TREE→도끼/WOOD, ROCK→곡괭이/STONE. `harvestTool/dropItem`은 ③ 채집에서, `placeable`은 ⑥ 건축에서 사용.
- `TOOL_PICKAXE` 아이템(enum+색) 추가, 시작 핫바 슬롯 6에 배치(③ 대비).
- ROCK 로우폴리 메시(눌린 8면체, 법선 중심 기준 바깥쪽 강제)를 `createObjectMeshes`에 추가 → 레지스트리가 실제로 2타입을 다룸을 검증.
- `TerrainGen::placeRocks` — 비숲(건조/개활, b<0.45) 평지에 sparse 배치. 나무(b>0.58)와 바이옴 분리라 겹치지 않음.
- shadow pass에 `objectDef(type).castShadow` 게이트 추가(현재 둘 다 true라 화면 변화 없음, ③/이후 타입 대비).
- `collidable=true`는 저장만, 이동 차단 미적용(나무·돌 통과). 채집 상호작용은 ③.

### 자원 채집 (③)
- `World::tryHarvestObject(x,y,tool,...)` — 타일 위 오브젝트를 찾아 `objectDef().harvestTool`과 도구가 맞으면 제거 + 드롭정보(`dropItem`×`dropCount`) 반환. 결과 `HarvestResult{NoObject, WrongTool, Harvested}`.
- 좌클릭 우선순위: **① 오브젝트 채집(도끼→나무/곡괭이→돌)** → ② 도구 안 맞으면 막힘(아래 지형 보호) → ③ 오브젝트 없으면 기존 작물 수확/타일 파괴. 채집물은 드롭/줍기 레이어로 바닥에 떨어져 줍기.
- `Chunk::objectsDirty` 플래그 신설 — 오브젝트 추가/제거 시에만 렌더러가 인스턴스 버퍼 재빌드(성장·타일변경 땐 스킵 유지). 기존 `ChunkRenderData::objInstBuilt` 대체.
- 채집 시 `chunk.modified=true`로 세션 내 언로드/재방문 시 유지. **한계: 디스크 save→재시작 시 오브젝트 결정론 재생성으로 respawn**(DESIGN 자원 재생 방향과 일치, 영구 채집은 추후 제거-기록 저장).
- 이로써 스타듀 오브젝트 경제 아크 ①②③ 완료(인벤토리/작물 경제 → 제네릭 오브젝트 → 채집).
- 버그 수정: 오브젝트 드롭이 바닥 블록에 파묻혀 안 보이던 문제. `objPos.z`(지면 윗면 0.5)에 −0.2를 적용해 큐브가 지면 아래(0.15~0.45)로 묻혔음 → `+0.2`로 변경해 지면 위(0.55~0.85)에 안착. (밀 드롭은 블록 중심 기준이라 −0.25가 맞았던 것 — 기준점 차이였음)

### 복셀 편집 은퇴 (④, 지형 불변)
- 좌클릭 타일 파괴(`setTile AIR`) 분기 제거 → 좌클릭은 **오브젝트 채집 + 작물 수확**만.
- 우클릭 복셀 블록 설치(`isBlock(item)`) 분기 제거 → 우클릭은 **호미/씨앗/물뿌리개**만.
- `BLOCK_*` 아이템 타입은 유지(채집 드롭 + ⑤ 제작 재료). 동작만 제거. `isBlock`/`itemToTile` 인라인 헬퍼는 미사용 상태지만 ⑤⑥ 대비 Types.h에 보존.
- DESIGN "지형 불변 / Minecraft식 복셀 설치·파괴 은퇴" 실현. 지형은 이제 authored·고정, 건축은 ⑥ 오브젝트 레이어 예정.

### 인벤토리 제작 (⑤a, 스타듀식 클릭형 레시피)
- `Recipe{result, resultCount, inputs[3], requiresWorkbench}` 테이블 + `craftingRecipes()`(Types.h, `ObjectDef`처럼 데이터 주도). 초기 레시피: 작업대 `WOOD×4`, 울타리 `WOOD×2`(둘 다 인벤 제작). 새 `ItemType` `ITEM_WORKBENCH`/`ITEM_FENCE`.
- `GameState`: `countItem`/`removeItem`/`craft(idx)` — 재료 확인 → 소모 → 결과 `addItem`, 인벤 가득이면 입력 롤백. 인벤 열렸을 때 제작 행 클릭(엣지 감지 `m_prevCraftClick`)으로 `craft`. `requiresWorkbench` 레시피는 인벤에선 미노출(⑤b 작업대용).
- UI: 인벤 창 아래 제작 패널(결과+입력 색 swatch+개수, 재료 부족 시 어둡게). 클릭 사각형은 공유 `craftRowRect`로 GameState와 동기화. UI 버퍼 1024→`UI_MAX_VERTS`(2048) + 오버플로 가드.
- 제작 메커니즘은 **클릭형 레시피 목록**(마크식 격자 배치 아님). 제작물은 인벤에 쌓이고 설치는 ⑥. 본격 UI 리뉴얼은 별도 UI 패스로 미룸.
- 마크식 2단계 계획: ⑤a 인벤 제작(기본) → ⑥ 설치(작업대를 바닥에) → ⑤b 작업대 근처에서 고급 레시피(`requiresWorkbench`) 해금.

### 오브젝트 설치/철거 (⑥a)
- `ObjectType` WORKBENCH/FENCE 추가 + `ObjectDef`(placeable=true, harvestTool=NONE=맨손철거, dropItem=자기아이템). `itemToObjectType()` 매핑. 메시: `pushBox` 헬퍼로 작업대(테이블 박스)·울타리(기둥2+가로대2).
- 설치(우클릭): placeable 아이템 → `World::placeObject(x,y,type)`(지면 위·물 아님·중복 금지, `hasObjectAt` 체크) → Object 추가 + 인벤 1 소모.
- 철거(좌클릭): `tryHarvestObject`에서 placeable 오브젝트는 **도구 무관 철거 + 아이템 회수**, 자연물은 기존대로 도구 필요.
- 버그 수정: `placeObject`가 `objectsDirty`만 켜고 `dirty`를 안 켜서 설치물이 안 보이던 문제 → `dirty`도 set(오브젝트 버퍼 재빌드는 `buildChunkBuffer`=dirty 게이트 안에서 일어남).

### 오브젝트 영속성 (⑥b, save v2)
- save 포맷 버전 1→2. 청크마다 `objects` 직렬화(type/pos/scale/rot). load는 TerrainGen 재생성 대신 저장된 objects를 직접 읽음.
- 효과 둘: ① 설치한 작업대/울타리 **재시작 후 유지**, ② 채집한 자연물(나무/돌) **respawn 버그 해소**(저장 objects가 채집/설치 결과를 반영). ③의 "디스크 재시작 시 respawn" 한계 제거.
- 기존 v1 세이브는 무시되고 새 월드(개발 중 합의된 호환 깨짐), 다음 저장에 v2로 갱신.
- 이로써 제작·건축 아크 중 ⑤a·⑥ 완료. 남은 건 ⑤b(작업대 근처 고급 레시피 해금).

### 작업대 근처 고급 레시피 해금 (⑤b)
- `World::isObjectTypeNear(x,y,type,radius)` — 플레이어 주변에 특정 오브젝트가 있는지 검색. `GameState`가 매 프레임 `m_nearWorkbench`(반경 2, WORKBENCH) 판정.
- `requiresWorkbench` 레시피는 작업대 근처일 때만 제작 패널에 노출 + 클릭 제작 허용. 판정값을 `drawFrame`으로 넘겨 렌더 필터(`m_nearWorkbenchHud`)와 클릭 필터가 같은 프레임 단일 값으로 동기화(행 위치 어긋남 없음).
- 데모 고급 레시피: 돌담 `STONE×2`(작업대 필요). `ObjectType::STONE_FENCE` + 메시(회색 벽) + ⑥ 설치/철거/save 인프라 그대로 재사용.
- **이로써 스타듀 오브젝트 경제 아크 ①~⑥ 전체 완성** — 인벤토리/작물 경제 → 제네릭 오브젝트 → 채집 → 지형 불변 → 제작(인벤+작업대 2단계) → 설치/철거+영속성.

### 중간점검 — 방향/마일스톤 재설정 (외부 분석 검토 후)
- 아크 ①~⑥ 완료 시점에 프로젝트 상태·기술부채·미래대비를 점검(외부 LLM 분석을 비판적으로 검토). 결론을 4개 문서에 반영.
- **핵심 판단**: 기술 기반은 이미 충분 — "기능 추가"보다 **iteration speed · 비주얼 정체성 · app-flow**가 ROI 높음. 비주얼은 **다시 만들기가 아니라 조율**(grading·fog·shadow·AO 이미 존재).
- **비주얼 노스스타 확정**(DESIGN): 스타일라이즈드 로우폴리 디오라마 룩(덕코프류 **그래픽만** 참고, 시스템·전투 연출은 차용 안 함).
- **기술 Tier 재정렬**(ARCHITECTURE): Tier 1(DevUI/프로파일링·`FrameRenderData` 스냅샷·`GpuBuffer` RAII·app-state/메뉴/설정) → Tier 2(height fog·hemisphere ambient·wind·variation·AA SMAA+FXAA·LUT) → Tier 3(오디오·IRenderPass·데이터화·테스트).
- **명시적 비목표 신설**(ARCHITECTURE): ECS rewrite·render graph·asset DB·material graph·job system·RTX/PBR/mesh shader/bindless — rule of 3/실제 병목 전엔 안 함(renderer addiction 방어선).
- **불변식 명문화**: 결정론 생성 / 레이어 분리(렌더러=스냅샷 소비) / 상태는 World·GameState 경유 / 지형 불변 / 세이브 버전 정책.
- **분석 정정 사항**(코드 실제 기준): 색 그레이딩·split-tone·fog는 이미 구현됨 / 청크 리빌드 throttle 존재 / 에셋·텍스처·애니메이션 거의 없음 → asset DB·material 시스템은 시기상조 / 좌표 변환은 이미 중앙화.
- **즉시 가능한 작은 완성도**로 식별: 오브젝트 충돌(`collidable` 이미 데이터, `canOccupy` 한 줄).

### 오브젝트 충돌 (즉시 완성도)
- `World::isCollidableAt(x,y)` — 타일 위 오브젝트 중 `objectDef.collidable`인 게 있으면 true. `hasObjectAt`(아무 오브젝트나)과 분리 — 후자는 설치 중복 방지용, 책임이 다름.
- `GameState::canOccupy`에 `&& !world.isCollidableAt(...)` 한 줄 추가. 이로써 데이터로만 있던 `collidable` 플래그가 실제 이동 차단으로 배선됨.
- 결과: 나무·돌·울타리·작업대·돌담을 통과하지 못함(건축이 장식→기능). 축 분리 이동이라 옆면 미끄러짐은 유지, 작물(WHEAT 타일)은 타일 충돌이라 영향 없음.

### FrameRenderData 스냅샷 (Tier 1-A, 순수 리팩토링)
- `drawFrame`의 인자 10개 → `FrameRenderData` 구조체 1개로 묶음(`VulkanContext.h`, 클래스 위). 기능마다 인자 +1 하던 압력 제거, 렌더러는 public 경계에서 스냅샷만 소비.
- by-ref/by-value를 기존 시그니처 그대로 미러링(camera/inventory/drops=참조, 나머지=값) → **동작·성능 불변**. 본문은 `frame.*` 기계적 치환, 호출처(`main.cpp`)는 중괄호 초기화.
- 내부 헬퍼(`updateUniformBuffer` 등)는 좁은 인자 유지 — 스냅샷을 더 깊이 배선하지 않음(수술적 변경, 스코프 크리프 방지). DevUI(Tier 1-C)가 읽고 쓸 접합면 확보.

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
