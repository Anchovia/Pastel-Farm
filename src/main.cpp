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

            PlayerInput input = inputManager.pollInput();

            if (input.quit)
                window.close();

            const float rotSpeed = 90.0f * dt;
            if (input.rotateLeft)  orbitAngle -= rotSpeed;
            if (input.rotateRight) orbitAngle += rotSpeed;

            // Ctrl+S save (edge-detect)
            if (input.saveKey && !prevCtrlS)
                world.save("save.dat", gameState.player().position(), gameState.time());
            prevCtrlS = input.saveKey;

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

            ctx.drawFrame(FrameRenderData{
                camera, gameState.player().position(), gameState.targetTile(),
                gameState.selectedSlot(), gameState.inventory(), gameState.timeOfDay(),
                gameState.inventoryOpen(), gameState.day(), gameState.drops(), gameState.nearWorkbench()
            });
        }
        ctx.waitIdle();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
