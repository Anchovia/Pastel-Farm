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

enum class AppMode {
    MainMenu,
    Gameplay,
    Paused,
};

struct AppFlow {
    AppMode mode = AppMode::MainMenu;
    bool prevEsc = false;
    bool prevStart = false;
    bool prevCtrlS = false;
#ifdef PASTEL_DEV_BUILD
    bool prevDevUiToggle = false;
#endif

    bool gameplayActive() const {
        return mode == AppMode::Gameplay;
    }

    bool paused() const {
        return mode == AppMode::Paused;
    }

    bool mainMenu() const {
        return mode == AppMode::MainMenu;
    }

    void updateMainMenuStart(bool startPressed) {
        if (mode == AppMode::MainMenu && startPressed && !prevStart)
            mode = AppMode::Gameplay;
        prevStart = startPressed;
    }

    void updatePauseToggle(bool escPressed) {
        if (escPressed && !prevEsc) {
            if (mode == AppMode::Gameplay)
                mode = AppMode::Paused;
            else if (mode == AppMode::Paused)
                mode = AppMode::Gameplay;
        }
        prevEsc = escPressed;
    }

    bool consumeSavePress(bool savePressed) {
        const bool pressed = savePressed && !prevCtrlS;
        prevCtrlS = savePressed;
        return pressed;
    }

#ifdef PASTEL_DEV_BUILD
    bool consumeDevUiToggle(bool togglePressed) {
        const bool pressed = togglePressed && !prevDevUiToggle;
        prevDevUiToggle = togglePressed;
        return pressed;
    }
#endif
};

static void clearGameplayInput(PlayerInput& input) {
    input.moveForward     = false;
    input.moveBackward    = false;
    input.moveLeft        = false;
    input.moveRight       = false;
    input.leftClick       = false;
    input.rightClick      = false;
    input.toggleInventory = false;
    input.rotateLeft      = false;
    input.rotateRight     = false;
    input.selectSlot      = -1;
    input.scrollDelta     = 0;
}

static void applyAppModeInputPolicy(PlayerInput& input, AppMode mode) {
    if (mode != AppMode::Gameplay)
        clearGameplayInput(input);
    if (mode == AppMode::MainMenu)
        input.saveKey = false;
}

#ifdef PASTEL_DEV_BUILD
static void applyDevUiInputCapture(PlayerInput& input, const VulkanContext& ctx) {
    if (ctx.devWantsMouse()) {
        input.leftClick   = false;
        input.rightClick  = false;
        input.scrollDelta = 0;
    }
    if (ctx.devWantsKeyboard()) {
        clearGameplayInput(input);
        input.saveKey         = false;
        input.quit            = false;
        input.startKey        = false;
    }
}
#endif

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

        AppFlow app;

        float  orbitAngle = 45.0f;
        double lastTime   = glfwGetTime();

        while (!window.shouldClose()) {
            window.pollEvents();
#ifdef PASTEL_DEV_BUILD
            ctx.beginDevFrame();
#endif

            double now = glfwGetTime();
            float  dt  = static_cast<float>(now - lastTime);
            lastTime = now;

            PlayerInput input = inputManager.pollInput();
#ifdef PASTEL_DEV_BUILD
            if (app.consumeDevUiToggle(input.toggleDevUi))
                ctx.toggleDevUi();
            applyDevUiInputCapture(input, ctx);
#endif

            app.updatePauseToggle(input.quit);
            app.updateMainMenuStart(input.startKey);

            applyAppModeInputPolicy(input, app.mode);

            const float rotSpeed = 90.0f * dt;
            if (input.rotateLeft)  orbitAngle -= rotSpeed;
            if (input.rotateRight) orbitAngle += rotSpeed;

            // Ctrl+S save (edge-detect)
            if (app.consumeSavePress(input.saveKey))
                world.save("save.dat", gameState.player().position(), gameState.time());

            if (input.windowWidth > 0 && input.windowHeight > 0)
                camera.setAspectRatio((float)input.windowWidth / input.windowHeight);

            camera.update(gameState.player().position(), orbitAngle);
            if (app.gameplayActive())
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
                gameState.inventoryOpen(), gameState.day(), gameState.drops(), gameState.nearWorkbench(),
                app.mainMenu(), app.paused()
            });
        }
        ctx.waitIdle();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
