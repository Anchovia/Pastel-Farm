#include "platform/InputManager.h"

InputManager::InputManager(Window& window) : m_window(window) {}

PlayerInput InputManager::pollInput() {
    PlayerInput input{};
    GLFWwindow* win = m_window.handle();

    // Movement keys
    input.moveForward  = glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS;
    input.moveBackward = glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS;
    input.moveLeft     = glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS;
    input.moveRight    = glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS;

    // Hotbar number keys 1..HOTBAR_SLOTS
    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        if (glfwGetKey(win, GLFW_KEY_1 + i) == GLFW_PRESS) {
            input.selectSlot = i;
            break;
        }
    }

    // Scroll wheel: scroll down -> next slot
    double scroll = m_window.consumeScrollY();
    if (scroll != 0.0)
        input.scrollDelta = (scroll > 0.0) ? -1 : 1;

    // Mouse position and click state
    glfwGetCursorPos(win, &input.mouseX, &input.mouseY);
    input.leftClick  = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT)  == GLFW_PRESS;
    input.rightClick = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    input.toggleInventory = glfwGetKey(win, GLFW_KEY_I) == GLFW_PRESS;

    // Camera / system keys
    input.quit        = glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    input.rotateLeft  = glfwGetKey(win, GLFW_KEY_Q)      == GLFW_PRESS;
    input.rotateRight = glfwGetKey(win, GLFW_KEY_E)      == GLFW_PRESS;
    input.saveKey     = glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
                     && glfwGetKey(win, GLFW_KEY_S)            == GLFW_PRESS;
    input.toggleDevUi = glfwGetKey(win, GLFW_KEY_F3)     == GLFW_PRESS;
    input.startKey    = glfwGetKey(win, GLFW_KEY_ENTER)  == GLFW_PRESS;
    input.settingsKey = glfwGetKey(win, GLFW_KEY_S)      == GLFW_PRESS;
    input.toggleVsyncKey = glfwGetKey(win, GLFW_KEY_V)   == GLFW_PRESS;

    // Window size
    glfwGetFramebufferSize(win, &input.windowWidth, &input.windowHeight);

    return input;
}
