#include "platform/InputManager.h"

InputManager::InputManager(GLFWwindow* window) : m_window(window) {}

PlayerInput InputManager::pollInput() {
    PlayerInput input{};

    // Arrow key input
    input.moveForward = glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS;
    input.moveBackward = glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS;
    input.moveLeft = glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS;
    input.moveRight = glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS;

    // Mouse position and click state
    glfwGetCursorPos(m_window, &input.mouseX, &input.mouseY);
    input.leftClick = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    // Window size
    glfwGetFramebufferSize(m_window, &input.windowWidth, &input.windowHeight);

    return input;
}