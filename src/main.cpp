#include "game/GameState.h"
#include "platform/Window.h"
#include "renderer/VulkanContext.h"
#include "world/World.h"
#include <iostream>
#include "game/Camera.h"

int main() {
    try {
        Window        window(1280, 720, "Pastel Farm");
        World         world;
        GameState     gameState;
        VulkanContext ctx(window, world);

        Camera camera(45.0f, 1280.0f / 720.0f, 0.1f, 100.0f);
        float orbitAngle = 45.0f;

        double        lastTime = glfwGetTime();

        while (!window.shouldClose()) {
            window.pollEvents();

            double now = glfwGetTime();
            float  dt  = static_cast<float>(now - lastTime);
            lastTime = now;

            GLFWwindow* win = window.handle();
            if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(win, GLFW_TRUE);
            }

            const float rotSpeed = 90.0f * dt;
            if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS) orbitAngle -= rotSpeed;
            if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS) orbitAngle += rotSpeed;

            PlayerInput input{};
            input.moveForward  = glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS;
            input.moveBackward = glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS;
            input.moveLeft     = glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS;
            input.moveRight    = glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS;

            // 마우스 및 창 크기 정보 수집
            glfwGetCursorPos(win, &input.mouseX, &input.mouseY);
            input.leftClick = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            glfwGetFramebufferSize(win, &input.windowWidth, &input.windowHeight);

            if (input.windowWidth > 0 && input.windowHeight > 0) {
                camera.setAspectRatio((float)input.windowWidth / input.windowHeight);
            }

            camera.update(gameState.player().position(), orbitAngle);

            gameState.update(dt, input, camera, world);
            ctx.drawFrame(camera, gameState.player().position(), gameState.targetTile());
        }
        ctx.waitIdle();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
