#pragma once
#include "game/GameState.h" // PlayerInput 구조체 사용을 위해 포함
#include <GLFW/glfw3.h>

class InputManager {
public:
    InputManager(GLFWwindow* window);

    // 매 프레임 호출하여 현재 입력 상태를 스냅샷으로 반환
    PlayerInput pollInput();

private:
    GLFWwindow* m_window;
};