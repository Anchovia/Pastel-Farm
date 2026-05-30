#include "game/GameState.h"
#include "platform/Window.h"
#include "platform/InputManager.h"
#include "renderer/VulkanContext.h"
#include "world/World.h"
#include "world/Chunk.h"
#include <iostream>
#include "game/Camera.h"

static constexpr int LOAD_RADIUS   = 3;
static constexpr int UNLOAD_RADIUS = 4;

int main() {
    try {
        Window        window(1280, 720, "Pastel Farm");
        World         world;
        GameState     gameState;
        VulkanContext ctx(window, world);
        InputManager  inputManager(window);
        Camera camera(45.0f, 1280.0f / 720.0f, 0.1f, 100.0f);

        // Auto-load save file if it exists
        {
            glm::vec3 savedPos;
            float     savedTime;
            if (world.load("save.dat", savedPos, savedTime)) {
                gameState.setPlayerPosition(savedPos);
                gameState.setTime(savedTime);
            }
        }

        // Initial chunk load around spawn (or restored position)
        glm::ivec2 spawnChunk = World::chunkCoord(
            (int)gameState.player().position().x,
            (int)gameState.player().position().y
        );
        world.loadChunksAround(spawnChunk.x, spawnChunk.y, LOAD_RADIUS);
        glm::ivec2 lastPlayerChunk = spawnChunk;

        bool prevCtrlS = false;

        float  orbitAngle = 45.0f;
        double lastTime   = glfwGetTime();

        while (!window.shouldClose()) {
            window.pollEvents();

            double now = glfwGetTime();
            float  dt  = static_cast<float>(now - lastTime);
            lastTime = now;

            GLFWwindow* win = window.handle();
            if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(win, GLFW_TRUE);

            const float rotSpeed = 90.0f * dt;
            if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS) orbitAngle -= rotSpeed;
            if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS) orbitAngle += rotSpeed;

            // Ctrl+S save (edge-detect)
            bool ctrlS = glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
                      && glfwGetKey(win, GLFW_KEY_S)            == GLFW_PRESS;
            if (ctrlS && !prevCtrlS)
                world.save("save.dat", gameState.player().position(), gameState.time());
            prevCtrlS = ctrlS;

            PlayerInput input = inputManager.pollInput();

            if (input.windowWidth > 0 && input.windowHeight > 0)
                camera.setAspectRatio((float)input.windowWidth / input.windowHeight);

            camera.update(gameState.player().position(), orbitAngle);
            gameState.update(dt, input, camera, world);

            // Load/unload chunks when player crosses a chunk boundary
            glm::ivec2 playerChunk = World::chunkCoord(
                (int)gameState.player().position().x,
                (int)gameState.player().position().y
            );
            if (playerChunk != lastPlayerChunk) {
                world.loadChunksAround(playerChunk.x, playerChunk.y, LOAD_RADIUS);
                world.unloadChunksOutside(playerChunk.x, playerChunk.y, UNLOAD_RADIUS);
                lastPlayerChunk = playerChunk;
            }

            ctx.drawFrame(camera, gameState.player().position(), gameState.targetTile(),
                          gameState.selectedSlot(), gameState.palette(), gameState.timeOfDay(),
                          gameState.inventoryOpen());
        }
        ctx.waitIdle();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
