#include "platform/InputManager.h"

InputManager::InputManager(GLFWwindow* window) : m_window(window) {}

PlayerInput InputManager::pollInput() {
    PlayerInput input{};

    // 키보드 방향키 입력
    input.moveForward = glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS;
    input.moveBackward = glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS;
    input.moveLeft = glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS;
    input.moveRight = glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS;

    // 마우스 좌표 및 클릭 상태
    glfwGetCursorPos(m_window, &input.mouseX, &input.mouseY);
    input.leftClick = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    // 창 크기
    glfwGetFramebufferSize(m_window, &input.windowWidth, &input.windowHeight);

    return input;
}