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

나중에 **staging buffer** 방식으로 업그레이드 예정:
`CPU → HOST_VISIBLE 임시 버퍼 → (GPU가 복사) → DEVICE_LOCAL 버퍼`

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
