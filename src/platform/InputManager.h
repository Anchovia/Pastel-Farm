#pragma once
#include "game/GameState.h" // For PlayerInput struct
#include <GLFW/glfw3.h>

class InputManager {
public:
    InputManager(GLFWwindow* window);

    // Called each frame to return current input snapshot
    PlayerInput pollInput();

private:
    GLFWwindow* m_window;
};