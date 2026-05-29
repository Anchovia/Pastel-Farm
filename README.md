# Game Engine (C++ / Vulkan)

저사양 PC에서도 부드럽게 돌아가는 **로우폴리 / 플랫 셰이딩** 스타일 게임을 만들기 위한 커스텀 게임 엔진입니다.
상용 엔진(Unity·Unreal) 대신 C++과 Vulkan으로 처음부터 직접 구축하며, **게임 완성**과 **엔진 개발** 두 가지를 동시에 목표로 합니다.

---

## 🎯 프로젝트 목표

- **저사양 친화적**: 오래된/내장 그래픽에서도 60fps가 나오는 가벼운 엔진
- **로우폴리 + 플랫 셰이딩**: 단순하고 스타일리시한 그래픽 (Duckside 류)
- **Vulkan 직접 사용**: 낮은 CPU 오버헤드와 렌더링 제어권 확보, 장기적으로 최적화 가능한 구조
- **학습 + 실전**: 엔진 내부를 이해하면서, 실제로 굴러가는 게임까지 완성

### 그래픽 방향성
- 로우폴리 메시, 플랫 셰이딩, 단순한 디렉셔널 라이트
- 텍스처 의존도 최소화
- 무거운 PBR / 포토리얼리즘은 **하지 않음**

### 목표 하드웨어
- **GTX 750 Ti / Intel UHD** 급에서 **60fps**
- CPU·GPU 부담이 적은 경량 렌더링

---

## 🧰 기술 스택

| 분류 | 사용 기술 |
|------|-----------|
| 언어 | C++20 |
| 빌드 | CMake (3.20+) |
| 그래픽 API | Vulkan |
| 윈도우/입력 | GLFW 3.4 |
| 수학 | GLM 1.0.1 |
| 셰이더 | GLSL → SPIR-V (`glslc`) |
| 의존성 관리 | CMake **FetchContent** (vcpkg 불필요) |

> GLFW와 GLM은 CMake가 빌드 시 자동으로 받아옵니다. 별도 설치 불필요.
> Vulkan SDK만 미리 설치돼 있으면 됩니다.

---

## 📌 현재 진행 상황

### ✅ Phase 1 — Vulkan 초기화 + 삼각형 (완료)
- Instance / Validation Layer / Surface
- 물리·논리 디바이스, 큐 패밀리
- 스왑체인 + 이미지 뷰
- 렌더 패스 + 그래픽스 파이프라인
- 프레임버퍼, 커맨드 풀/버퍼
- 동기화(세마포어·펜스), 창 리사이즈 시 스왑체인 재생성
- **하드코딩된 삼각형 렌더링 성공**

### 🚧 Phase 2 — 최소 3D (진행 중)
- ✅ **Vertex Buffer** — 정점 데이터를 GPU로 업로드해서 그리기 (정점별 색상 그라데이션 삼각형 확인)
- ⬜ Index Buffer — 정점 재사용 (다음 단계)
- ⬜ Uniform Buffer + Descriptor Set
- ⬜ MVP 행렬 (GLM) — 3D 변환
- ⬜ Depth Buffer
- ⬜ 카메라 (WASD 이동)
- 🎯 목표: **카메라로 둘러볼 수 있는 3D 큐브**

### ⬜ Phase 3 — 플랫 셰이딩 렌더러 (예정)
- 디렉셔널 라이트 기반 플랫 셰이딩
- 프러스텀 컬링

---

## 📂 프로젝트 구조

```
game project/
├─ CMakeLists.txt          # 빌드 설정 (FetchContent, 셰이더 컴파일 포함)
├─ README.md
├─ .gitignore
├─ shaders/
│  ├─ triangle.vert        # 정점 셰이더 (GLSL)
│  └─ triangle.frag        # 프래그먼트 셰이더 (GLSL)
└─ src/
   ├─ main.cpp             # 진입점: 창 생성 + 렌더 루프
   ├─ platform/
   │  ├─ Window.h          # GLFW 창 래퍼 (RAII, 리사이즈 콜백)
   │  └─ Window.cpp
   └─ renderer/
      ├─ Types.h           # Vertex 구조체 (pos, color)
      ├─ VulkanContext.h   # Vulkan 렌더러 선언
      └─ VulkanContext.cpp # Vulkan 렌더러 구현 (초기화~drawFrame 전부)
```

> `build/` 폴더는 CMake 생성물 + 자동으로 받은 GLFW/GLM 소스가 들어있어 git에서 제외됩니다.

---

## 🏗️ 빌드 방법 (Windows / Visual Studio 2022)

### 사전 요구사항
- **Visual Studio 2022** (C++ 데스크톱 개발 워크로드)
- **CMake** (VS에 내장된 것 사용 가능)
- **Vulkan SDK** (예: `C:\VulkanSDK\1.3.275.0`)

### 빌드 절차
```cmd
cd "C:\Users\USER\Desktop\game project"

REM Vulkan SDK 경로 지정 (환경변수 미설정 시)
set VULKAN_SDK=C:\VulkanSDK\1.3.275.0

REM CMake 구성
cmake -B build -S .
```

이후 둘 중 하나로 빌드/실행:

- **Visual Studio**: `build/GameEngine.sln` 열기 → `engine`을 시작 프로젝트로 지정 → **F5**
- **명령줄**:
  ```cmd
  cmake --build build --config Debug
  build\Debug\engine.exe
  ```

> 셰이더(`.spv`)는 빌드 시 `glslc`로 자동 컴파일되어 실행 파일 옆 `shaders/` 폴더로 복사됩니다.
> VS 디버거 작업 디렉토리도 실행 파일 위치로 고정되어 있어 F5로 바로 실행됩니다.

---

## ⚙️ 동작 원리

### 실행 흐름 (`main.cpp`)
```
Window 생성 → VulkanContext 생성
   └─ while (창이 닫히지 않는 동안)
        pollEvents()   // 입력/창 이벤트 처리
        drawFrame()    // 한 프레임 렌더링
   └─ waitIdle()       // 종료 시 GPU 작업 완료 대기
```

### 모듈 역할
- **`Window`** — GLFW 래퍼. `GLFW_NO_API`로 생성(OpenGL 컨텍스트 없음, Vulkan용). 리사이즈 콜백으로 `m_resized` 플래그를 세워 스왑체인 재생성을 트리거.
- **`VulkanContext`** — 엔진의 심장. Vulkan 객체 생성부터 매 프레임 렌더링까지 전부 담당.
  - 초기화 순서: Instance → Debug Messenger → Surface → Physical/Logical Device → Swapchain → Image Views → Render Pass → Pipeline → Framebuffers → Command Pool/Buffers → Sync Objects → Vertex Buffer
  - `drawFrame()`: 펜스 대기 → 이미지 획득 → 커맨드 버퍼 기록 → 제출 → 프레젠트
  - `MAX_FRAMES_IN_FLIGHT = 2` (CPU가 GPU보다 최대 1프레임만 앞서감)

### 셰이더 파이프라인
```
shaders/triangle.vert ─┐
                       ├─ glslc ─→ build/spirv/*.spv ─→ (복사) ─→ exe 옆 shaders/*.spv
shaders/triangle.frag ─┘
```

---

## 🚀 최적화 우선순위

성능 작업은 아래 순서를 우선합니다 (위쪽이 효과 큼):

1. 드로우 콜 감소
2. 배칭 / 인스턴싱
3. 컬링 (프러스텀 등)
4. 오버드로우 감소
5. 메모리 최적화
6. 에셋 파이프라인
7. API 레벨 미세 최적화

---

## 🧭 의도적으로 미루는 것들

저사양 목표와 단순함을 위해 아래는 **당분간 도입하지 않습니다**:
레이 트레이싱, 디퍼드 렌더링, 렌더 그래프, 클러스터드 라이팅, GPU 파티클, 바인드리스, 과도한 ECS, 과도한 아키텍처화.

---

## 📄 라이선스

미정 (개인 학습/개발 프로젝트)
