#include "game/GameState.h"
#include "platform/Window.h"
#include "renderer/VulkanContext.h"
#include "world/World.h"
#include <iostream>

int main() {
    try {
        Window        window(1280, 720, "Pastel Farm");
        World         world;
        GameState     gameState;
        VulkanContext ctx(window, world);
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
            if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS) ctx.rotateOrbit(-rotSpeed);
            if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS) ctx.rotateOrbit(rotSpeed);

            PlayerInput input{};
            input.moveForward  = glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS;
            input.moveBackward = glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS;
            input.moveLeft     = glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS;
            input.moveRight    = glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS;
            gameState.update(dt, input, ctx.orbitAngle());

            ctx.drawFrame(gameState.player().position());
        }
        ctx.waitIdle();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
