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
   ├─ platform/
   │  ├─ Window.h          # GLFW 창 래퍼 (RAII, 리사이즈 콜백)
   │  └─ Window.cpp
   └─ renderer/
      ├─ Types.h           # Vertex 구조체 (pos, color)
      ├─ VulkanContext.h   # Vulkan 렌더러 선언
      └─ VulkanContext.cpp # Vulkan 렌더러 구현
```

> `build/` 폴더는 CMake 생성물 + 자동으로 받은 GLFW/GLM 소스가 들어있어 git에서 제외됩니다.

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
