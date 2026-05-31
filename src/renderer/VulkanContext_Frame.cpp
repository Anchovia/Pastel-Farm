#include "VulkanContext.h"
#include "VulkanContext_Private.h"
#include "renderer/Types.h"
#include "platform/Window.h"
#include "world/World.h"
#include "game/Camera.h"

#include <stdexcept>
#include <cstring>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ============================================================
//  Command buffer recording
// ============================================================
void VulkanContext::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &begin);

    // Shadow pass — render chunk depth from sun's perspective
    {
        VkClearValue shadowClear{};
        shadowClear.depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo shadowRp{};
        shadowRp.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        shadowRp.renderPass      = m_shadowRenderPass;
        shadowRp.framebuffer     = m_shadowFramebuffer;
        shadowRp.renderArea      = {{0, 0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}};
        shadowRp.clearValueCount = 1;
        shadowRp.pClearValues    = &shadowClear;

        vkCmdBeginRenderPass(cmd, &shadowRp, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);
        vkCmdPushConstants(cmd, m_shadowPipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &m_lightMVP);

        for (auto& [coord, data] : m_chunkBuffers) {
            if (data.vertexBuffer == VK_NULL_HANDLE || data.indexCount == 0) continue;
            VkBuffer     vBuf[] = {data.vertexBuffer};
            VkDeviceSize offs[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vBuf, offs);
            vkCmdBindIndexBuffer(cmd, data.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, data.indexCount, 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(cmd);
    }

    VkClearValue clearValues[2];
    clearValues[0].color        = {{m_skyColor[0], m_skyColor[1], m_skyColor[2], 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo rp{};
    rp.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass      = m_renderPass;
    rp.framebuffer     = m_framebuffers[imageIndex];
    rp.renderArea      = {{0, 0}, m_swapchainExtent};
    rp.clearValueCount = 2;
    rp.pClearValues    = clearValues;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipelineLayout, 0, 1, &m_descriptorSets[m_currentFrame], 0, nullptr);

    // Chunk mesh (hidden face culling, dedicated pipeline)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_chunkPipeline);
    for (auto& [coord, data] : m_chunkBuffers) {
        if (data.vertexBuffer == VK_NULL_HANDLE || data.indexCount == 0) continue;

        glm::vec3 chunkMin = { coord.x * CHUNK_SIZE,       coord.y * CHUNK_SIZE,       0.0f };
        glm::vec3 chunkMax = { (coord.x + 1) * CHUNK_SIZE, (coord.y + 1) * CHUNK_SIZE, (float)CHUNK_DEPTH };
        if (!m_frustum.containsAABB(chunkMin, chunkMax)) continue;

        VkBuffer     vBuf[] = { data.vertexBuffer };
        VkDeviceSize offs[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vBuf, offs);
        vkCmdBindIndexBuffer(cmd, data.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, data.indexCount, 1, 0, 0, 0);
    }

    // Objects (trees) — shared mesh instanced per chunk
    if (m_treeVertexCount > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_objectPipeline);
        for (auto& [coord, data] : m_chunkBuffers) {
            if (data.objInstBuffer == VK_NULL_HANDLE || data.objInstCount == 0) continue;

            glm::vec3 chunkMin = { coord.x * CHUNK_SIZE,       coord.y * CHUNK_SIZE,       0.0f };
            glm::vec3 chunkMax = { (coord.x + 1) * CHUNK_SIZE, (coord.y + 1) * CHUNK_SIZE, (float)CHUNK_DEPTH };
            if (!m_frustum.containsAABB(chunkMin, chunkMax)) continue;

            VkBuffer     bufs[] = { m_treeVertexBuffer, data.objInstBuffer };
            VkDeviceSize offs[] = { 0, 0 };
            vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offs);
            vkCmdDraw(cmd, m_treeVertexCount, data.objInstCount, 0, 0);
        }
    }

    // Player / selector (instanced pipeline)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    if (m_showSelector) {
        VkBuffer     sBufs[] = {m_selectorVertexBuffer, m_selectorInstBuffer};
        VkDeviceSize sOffs[] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, sBufs, sOffs);
        vkCmdBindIndexBuffer(cmd, m_selectorIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd, (uint32_t)kSelectorIndices.size(), 1, 0, 0, 0);
    }

    // Player
    VkBuffer     pBufs[] = {m_vertexBuffer, m_playerInstBuffer};
    VkDeviceSize pOffs[] = {0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 2, pBufs, pOffs);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, (uint32_t)kIndices.size(), 1, 0, 0, 0);

    // UI overlay (screen-space, on top of everything)
    if (m_uiVertexCount > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiPipeline);
        VkBuffer     uiBufs[] = { m_uiBuffer };
        VkDeviceSize uiOffs[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, uiBufs, uiOffs);
        vkCmdDraw(cmd, m_uiVertexCount, 1, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

// ============================================================
//  drawFrame
// ============================================================
void VulkanContext::drawFrame(const Camera& camera, const glm::vec3& playerPosition, const std::optional<glm::ivec3>& targetTile,
                              int hotbarSelected, const std::array<ItemType, HOTBAR_SLOTS>& palette, float timeOfDay, bool inventoryOpen) {
    m_hotbarSelected = hotbarSelected;
    m_hotbarPalette  = palette;
    m_inventoryOpen  = inventoryOpen;

    // Advance frame counter and free buffers that are no longer in flight
    m_frameCount++;
    m_deletionQueue.erase(
        std::remove_if(m_deletionQueue.begin(), m_deletionQueue.end(),
            [&](const DeferredDelete& d) {
                if (m_frameCount - d.frame > (uint64_t)MAX_FRAMES_IN_FLIGHT) {
                    vkDestroyBuffer(m_device, d.buffer, nullptr);
                    vkFreeMemory(m_device, d.memory, nullptr);
                    return true;
                }
                return false;
            }),
        m_deletionQueue.end()
    );

    // Sky color: 4 keyframes keyed on timeOfDay (0=midnight, 0.25=dawn, 0.5=noon, 0.75=dusk)
    static constexpr float kSkyKeys[4][3] = {
        {0.05f, 0.05f, 0.12f}, // midnight
        {0.85f, 0.55f, 0.35f}, // dawn
        {0.45f, 0.72f, 0.95f}, // noon
        {0.80f, 0.40f, 0.20f}, // dusk
    };
    const float t4  = timeOfDay * 4.0f;
    const int   seg = static_cast<int>(t4) % 4;
    const float f   = t4 - static_cast<int>(t4);
    const int   next = (seg + 1) % 4;
    for (int i = 0; i < 3; ++i)
        m_skyColor[i] = kSkyKeys[seg][i] * (1.0f - f) + kSkyKeys[next][i] * f;

    // Wait for the previous frame using this slot to finish
    vkWaitForFences(m_device, 1, &m_inFlight[m_currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        m_device, m_swapchain, UINT64_MAX,
        m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("Failed to acquire swapchain image");

    vkResetFences(m_device, 1, &m_inFlight[m_currentFrame]);

    // Light space matrix — orthographic from sun direction, centered on player
    {
        float elevation = sinf(timeOfDay * 3.14159265f);
        float azimuth   = timeOfDay * 6.28318530f;
        glm::vec3 sunDir = glm::normalize(glm::vec3(cosf(azimuth), sinf(azimuth), elevation));
        const float range = 80.0f;
        glm::mat4 lightView = glm::lookAt(
            playerPosition + sunDir * 150.0f,
            playerPosition,
            glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 lightProj = glm::ortho(-range, range, -range, range, 1.0f, 300.0f);
        lightProj[1][1] *= -1.0f;
        m_lightMVP = lightProj * lightView;
    }

    updateUniformBuffer(m_currentFrame, camera, timeOfDay);
    updatePlayerInstanceBuffer(playerPosition);
    updateSelectorInstanceBuffer(targetTile);
    updateHotbar();
    rebuildDirtyChunks();
    m_frustum = Frustum::extractFrom(camera.viewProj());
    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
    recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &m_imageAvailable[m_currentFrame];
    submit.pWaitDstStageMask    = &waitStage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &m_commandBuffers[m_currentFrame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &m_renderFinished[m_currentFrame];
    vkQueueSubmit(m_graphicsQueue, 1, &submit, m_inFlight[m_currentFrame]);

    VkPresentInfoKHR present{};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &m_renderFinished[m_currentFrame];
    present.swapchainCount     = 1;
    present.pSwapchains        = &m_swapchain;
    present.pImageIndices      = &imageIndex;

    result = vkQueuePresentKHR(m_presentQueue, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window.wasResized()) {
        m_window.resetResized();
        recreateSwapchain();
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ============================================================
//  Per-frame update functions
// ============================================================
void VulkanContext::updateUniformBuffer(uint32_t currentFrame, const Camera& camera, float timeOfDay) {
    // Sun arc: elevation 0 at midnight, 1 at noon, 0 at next midnight
    float elevation = sinf(timeOfDay * 3.14159265f);
    float azimuth   = timeOfDay * 6.28318530f;

    glm::vec3 sunDir = glm::normalize(glm::vec3(
        cosf(azimuth),
        sinf(azimuth),
        elevation
    ));

    UniformBufferObject ubo{};
    ubo.model    = glm::mat4(1.0f);
    ubo.view     = camera.view();
    ubo.proj     = camera.proj();
    ubo.lightDir = glm::vec4(sunDir, elevation); // w = dayFactor
    ubo.lightMVP = m_lightMVP;
    memcpy(m_uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}

void VulkanContext::updatePlayerInstanceBuffer(const glm::vec3& playerPosition) {
    static const glm::vec3 kPlayerColor = {1.0f, 0.45f, 0.1f};
    InstanceData inst{playerPosition, kPlayerColor, kPlayerColor};
    memcpy(m_playerInstMapped, &inst, sizeof(inst));
}

void VulkanContext::updateSelectorInstanceBuffer(const std::optional<glm::ivec3>& targetTile) {
    m_showSelector = targetTile.has_value();
    if (!m_showSelector) return;

    static const glm::vec3 kSelectorColor = {1.0f, 0.9f, 0.1f};
    const glm::ivec3 tile = *targetTile;
    InstanceData inst{m_world.tileCenter(tile.x, tile.y, tile.z), kSelectorColor, kSelectorColor};
    memcpy(m_selectorInstMapped, &inst, sizeof(inst));
}

// ============================================================
//  Hotbar / inventory UI geometry (rebuilt each frame)
// ============================================================
void VulkanContext::updateHotbar() {
    const float W = (float)m_swapchainExtent.width;
    const float H = (float)m_swapchainExtent.height;

    std::vector<UIVertex> verts;
    verts.reserve(256);

    auto pushQuad = [&](float x, float y, float w, float h, glm::vec4 color) {
        auto toNDC = [&](float px, float py) {
            return glm::vec2(px / W * 2.0f - 1.0f, py / H * 2.0f - 1.0f);
        };
        glm::vec2 p0 = toNDC(x,     y);
        glm::vec2 p1 = toNDC(x + w, y);
        glm::vec2 p2 = toNDC(x + w, y + h);
        glm::vec2 p3 = toNDC(x,     y + h);
        verts.push_back({p0, color});
        verts.push_back({p1, color});
        verts.push_back({p2, color});
        verts.push_back({p0, color});
        verts.push_back({p2, color});
        verts.push_back({p3, color});
    };

    // --- Hotbar ---
    const float slot = 56.0f, gap = 6.0f, pad = 6.0f;
    const float barW = HOTBAR_SLOTS * slot + (HOTBAR_SLOTS - 1) * gap;
    const float startX = (W - barW) * 0.5f;
    const float startY = H - slot - 20.0f;

    pushQuad(startX - pad, startY - pad, barW + 2 * pad, slot + 2 * pad, {0.10f, 0.10f, 0.12f, 0.7f});

    float selX = startX + m_hotbarSelected * (slot + gap);
    pushQuad(selX - 3.0f, startY - 3.0f, slot + 6.0f, slot + 6.0f, {1.0f, 0.85f, 0.2f, 1.0f});

    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        float x = startX + i * (slot + gap);
        glm::vec4 bg = (i == m_hotbarSelected)
            ? glm::vec4(0.35f, 0.35f, 0.40f, 1.0f)
            : glm::vec4(0.20f, 0.20f, 0.24f, 0.9f);
        pushQuad(x, startY, slot, slot, bg);

        ItemType t = m_hotbarPalette[i];
        if (t != ItemType::NONE) {
            glm::vec3 col = itemColor(t);
            float inset = 10.0f;
            pushQuad(x + inset, startY + inset, slot - 2 * inset, slot - 2 * inset,
                     glm::vec4(col, 1.0f));
        }
    }

    // --- Inventory overlay ---
    if (m_inventoryOpen) {
        const float gridW = INV_COLS * INV_SLOT_SIZE + (INV_COLS - 1) * INV_GAP;
        const float gridH = INV_ROWS * INV_SLOT_SIZE + (INV_ROWS - 1) * INV_GAP;
        const float ox = (W - gridW) * 0.5f - INV_PAD;
        const float oy = (H - gridH) * 0.5f - INV_PAD;
        const float panelW = gridW + 2 * INV_PAD;
        const float panelH = gridH + 2 * INV_PAD;

        // Dimmed background over entire screen
        pushQuad(0.0f, 0.0f, W, H, {0.0f, 0.0f, 0.0f, 0.45f});

        // Panel background
        pushQuad(ox, oy, panelW, panelH, {0.12f, 0.12f, 0.15f, 0.92f});

        for (int r = 0; r < INV_ROWS; ++r) {
            for (int c = 0; c < INV_COLS; ++c) {
                int idx = r * INV_COLS + c;
                ItemType item = static_cast<ItemType>(idx + 1); // skip NONE(0)
                if (item >= ItemType::COUNT) continue;

                float sx = ox + INV_PAD + c * (INV_SLOT_SIZE + INV_GAP);
                float sy = oy + INV_PAD + r * (INV_SLOT_SIZE + INV_GAP);

                pushQuad(sx, sy, INV_SLOT_SIZE, INV_SLOT_SIZE, {0.22f, 0.22f, 0.27f, 1.0f});

                glm::vec3 col = itemColor(item);
                float inset = 12.0f;
                pushQuad(sx + inset, sy + inset, INV_SLOT_SIZE - 2 * inset, INV_SLOT_SIZE - 2 * inset,
                         glm::vec4(col, 1.0f));
            }
        }
    }

    m_uiVertexCount = (uint32_t)verts.size();
    memcpy(m_uiMapped, verts.data(), sizeof(UIVertex) * verts.size());
}
