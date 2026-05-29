#include "platform/Window.h"
#include "renderer/VulkanContext.h"
#include <iostream>

int main() {
    try {
        Window       window(1280, 720, "Game Engine - Phase 1");
        VulkanContext ctx(window);

        while (!window.shouldClose()) {
            window.pollEvents();
            ctx.drawFrame();
        }
        ctx.waitIdle();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
