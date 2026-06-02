#include "VulkanContext.h"
#include "VulkanContext_Private.h"
#include "renderer/Types.h"
#include "platform/Window.h"
#include "world/World.h"
#include "game/Camera.h"

#include <stdexcept>
#include <iostream>
#include <set>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <chrono>
#include <cmath>
#include <utility>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ============================================================
//  Constructor / Destructor
// ============================================================
VulkanContext::VulkanContext(Window& window, World& world) : m_window(window), m_world(world) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createPostRenderPass();
    createSmaaRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createChunkPipeline();
    createUIPipeline();
    createObjectPipeline();
    createGrassPipeline();
    createPostPipeline();
    createSmaaPipelines();
    createDepthResources();
    createOffscreenResources();
    createSmaaResources();
    createShadowResources();
    createShadowPipeline();
    createShadowObjectPipeline();
    createShadowPlayerPipeline();
    createFramebuffers();
    createCommandPool();
    createSmaaLookupTextures();
#ifdef PASTEL_DEV_BUILD
    createDevTools();
#endif
    createVertexBuffer();
    createIndexBuffer();
    createSelectorBuffers();
    createUIBuffer();
    createObjectMeshes();
    createGrassTexture();
    createItemMesh();
    createDropInstanceBuffer();
    rebuildDirtyChunks();
    createPlayerInstanceBuffer({15.0f, 15.0f, 1.0f});
    createUniformBuffers();
    createShadowSampler();
    createPostSampler();
    createDescriptorPool();
    createDescriptorSets();
    createPostDescriptors();
    createSmaaDescriptors();
    createCommandBuffers();
    createSyncObjects();
}

VulkanContext::~VulkanContext() {
    waitIdle();

#ifdef PASTEL_DEV_BUILD
    destroyDevTools();
#endif

    // Free all GpuBuffers here (device still alive). Their dtors run again after this
    // body when members destruct, but destroy() is idempotent so those are no-ops.
    m_deletionQueue.clear();

    cleanupSwapchain();

    m_uniformBuffers.clear();
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    m_playerInstBuffer.clear();
    m_selectorInstBuffer.clear();
    m_selectorIndexBuffer.destroy();
    m_selectorVertexBuffer.destroy();
    m_chunkBuffers.clear();          // frees chunk mesh, dressing, and object group buffers
    for (auto& mesh : m_objectMeshes) mesh.vbuf.destroy();
    m_grassClumpMesh.vbuf.destroy();
    m_grassCardMesh.vbuf.destroy();
    m_groundPatchMesh.vbuf.destroy();
    m_pebbleMesh.vbuf.destroy();
    m_itemVertexBuffer.destroy();
    m_dropInstBuffer.clear();
    m_indexBuffer.destroy();
    m_vertexBuffer.destroy();
    m_uiBuffer.clear();
    vkDestroyPipeline(m_device, m_uiPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_uiPipelineLayout, nullptr);
    vkDestroyPipeline(m_device, m_grassPipeline, nullptr);
    vkDestroyPipeline(m_device, m_objectPipeline, nullptr);
    vkDestroyPipeline(m_device, m_chunkPipeline, nullptr);
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyPipeline      (m_device, m_shadowPlayerPipeline, nullptr);
    vkDestroyPipeline      (m_device, m_shadowObjectPipeline, nullptr);
    vkDestroyPipeline      (m_device, m_shadowPipeline,       nullptr);
    vkDestroyPipelineLayout(m_device, m_shadowPipelineLayout, nullptr);
    vkDestroySampler       (m_device, m_shadowSampler,        nullptr);
    vkDestroyFramebuffer   (m_device, m_shadowFramebuffer,    nullptr);
    vkDestroyRenderPass    (m_device, m_shadowRenderPass,     nullptr);
    vkDestroyImageView  (m_device, m_shadowImageView,    nullptr);
    vkDestroyImage      (m_device, m_shadowImage,        nullptr);
    vkFreeMemory        (m_device, m_shadowImageMemory,  nullptr);
    vkDestroyPipeline           (m_device, m_postPipeline,            nullptr);
    vkDestroyPipelineLayout     (m_device, m_postPipelineLayout,      nullptr);
    vkDestroyDescriptorPool     (m_device, m_postDescriptorPool,      nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_postDescriptorSetLayout, nullptr);
    vkDestroyPipeline           (m_device, m_smaaNeighborhoodPipeline,       nullptr);
    vkDestroyPipelineLayout     (m_device, m_smaaNeighborhoodPipelineLayout, nullptr);
    vkDestroyPipeline           (m_device, m_smaaBlendPipeline,              nullptr);
    vkDestroyPipelineLayout     (m_device, m_smaaBlendPipelineLayout,        nullptr);
    vkDestroyPipeline           (m_device, m_smaaEdgePipeline,               nullptr);
    vkDestroyPipelineLayout     (m_device, m_smaaEdgePipelineLayout,         nullptr);
    vkDestroyDescriptorPool     (m_device, m_smaaDescriptorPool,             nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_smaaNeighborhoodDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_smaaBlendDescriptorSetLayout,        nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_smaaEdgeDescriptorSetLayout,         nullptr);
    vkDestroyImageView(m_device, m_smaaAreaView,    nullptr);
    vkDestroyImage    (m_device, m_smaaAreaImage,   nullptr);
    vkFreeMemory      (m_device, m_smaaAreaMemory,  nullptr);
    vkDestroyImageView(m_device, m_smaaSearchView,   nullptr);
    vkDestroyImage    (m_device, m_smaaSearchImage,  nullptr);
    vkFreeMemory      (m_device, m_smaaSearchMemory, nullptr);
    vkDestroySampler            (m_device, m_postSampler,             nullptr);
    vkDestroySampler  (m_device, m_grassTexSampler, nullptr);
    vkDestroyImageView(m_device, m_grassTexView,    nullptr);
    vkDestroyImage    (m_device, m_grassTexImage,   nullptr);
    vkFreeMemory      (m_device, m_grassTexMemory,  nullptr);
    vkDestroyRenderPass         (m_device, m_smaaRenderPass,          nullptr);
    vkDestroyRenderPass         (m_device, m_postRenderPass,          nullptr);
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(m_device, m_imageAvailable[i], nullptr);
        vkDestroyFence(m_device, m_inFlight[i], nullptr);
    }
    for (auto sem : m_renderFinished)
        vkDestroySemaphore(m_device, sem, nullptr);
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    vkDestroyDevice(m_device, nullptr);
    if (kEnableValidation) DestroyDebugMessenger(m_instance, m_debugMessenger);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void VulkanContext::waitIdle() { vkDeviceWaitIdle(m_device); }

void VulkanContext::deferDestroy(GpuBuffer&& buf) {
    if (buf.buffer != VK_NULL_HANDLE)
        m_deletionQueue.push_back({std::move(buf), m_frameCount});
}

// ============================================================
//  Shader helpers
// ============================================================
std::vector<char> VulkanContext::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Cannot open file: " + path);
    size_t size = (size_t)file.tellg();
    std::vector<char> buf(size);
    file.seekg(0);
    file.read(buf.data(), size);
    return buf;
}

VkShaderModule VulkanContext::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod;
    if (vkCreateShaderModule(m_device, &info, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module");
    return mod;
}

// ============================================================
//  Memory / buffer helpers
// ============================================================
uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

GpuBuffer VulkanContext::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties)
{
    GpuBuffer buf;
    buf.device = m_device;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(m_device, &bufInfo, nullptr, &buf.buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(m_device, buf.buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &buf.memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate buffer memory");

    vkBindBufferMemory(m_device, buf.buffer, buf.memory, 0);
    return buf;
}

VkFormat VulkanContext::findSupportedFormat(const std::vector<VkFormat>& candidates,
    VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_OPTIMAL &&
            (props.optimalTilingFeatures & features) == features)
            return format;
    }
    throw std::runtime_error("Failed to find supported format");
}

VkFormat VulkanContext::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void VulkanContext::createImage(uint32_t width, uint32_t height, VkFormat format,
    VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
    VkImage& image, VkDeviceMemory& memory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = {width, height, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(m_device, image, &memReq);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate image memory");
    vkBindImageMemory(m_device, image, memory, 0);
}

// ============================================================
//  Swapchain cleanup / recreation
// ============================================================
void VulkanContext::cleanupSwapchain() {
    vkDestroyImageView(m_device, m_depthImageView, nullptr);
    vkDestroyImage    (m_device, m_depthImage,     nullptr);
    vkFreeMemory      (m_device, m_depthImageMemory, nullptr);
    for (size_t i = 0; i < m_offscreenImage.size(); i++) {
        vkDestroyImageView(m_device, m_offscreenView[i],   nullptr);
        vkDestroyImage    (m_device, m_offscreenImage[i],  nullptr);
        vkFreeMemory      (m_device, m_offscreenMemory[i], nullptr);
    }
    for (size_t i = 0; i < m_smaaEdgeImage.size(); i++) {
        vkDestroyImageView(m_device, m_smaaEdgeView[i],   nullptr);
        vkDestroyImage    (m_device, m_smaaEdgeImage[i],  nullptr);
        vkFreeMemory      (m_device, m_smaaEdgeMemory[i], nullptr);
    }
    for (size_t i = 0; i < m_smaaBlendImage.size(); i++) {
        vkDestroyImageView(m_device, m_smaaBlendView[i],   nullptr);
        vkDestroyImage    (m_device, m_smaaBlendImage[i],  nullptr);
        vkFreeMemory      (m_device, m_smaaBlendMemory[i], nullptr);
    }
    for (auto fb : m_sceneFramebuffers)    vkDestroyFramebuffer(m_device, fb, nullptr);
    for (auto fb : m_postFramebuffers)     vkDestroyFramebuffer(m_device, fb, nullptr);
    for (auto fb : m_smaaEdgeFramebuffers) vkDestroyFramebuffer(m_device, fb, nullptr);
    for (auto fb : m_smaaBlendFramebuffers) vkDestroyFramebuffer(m_device, fb, nullptr);
    for (auto iv : m_swapchainImageViews)  vkDestroyImageView  (m_device, iv, nullptr);
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}

void VulkanContext::recreateSwapchain() {
    // Pause while minimized
    int w = 0, h = 0;
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(m_window.handle(), &w, &h);
        glfwWaitEvents();
    }
    waitIdle();
    cleanupSwapchain();
    createSwapchain();
    createImageViews();
    createDepthResources();
    createOffscreenResources();
    createSmaaResources();
    createFramebuffers();
    updatePostDescriptors();   // offscreen views were recreated
    updateSmaaDescriptors();   // SMAA intermediate views were recreated

    // Swapchain image count may have changed — recreate per-image present semaphores
    for (auto sem : m_renderFinished)
        vkDestroySemaphore(m_device, sem, nullptr);
    m_renderFinished.resize(m_swapchainImages.size());
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (auto& sem : m_renderFinished)
        vkCreateSemaphore(m_device, &semInfo, nullptr, &sem);
    m_imagesInFlight.assign(m_swapchainImages.size(), VK_NULL_HANDLE);
}

// ============================================================
//  Buffer copy (one-shot command buffer)
// ============================================================
void VulkanContext::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    vkCreateFence(m_device, &fenceInfo, nullptr, &fence);

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, fence);
    vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(m_device, fence, nullptr);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

// ============================================================
//  Image layout transition / buffer→image copy (one-shot command buffers)
// ============================================================
// NOTE: these share the one-shot submit boilerplate with copyBuffer above. Once a
// third use appears, extracting begin/endSingleTimeCommands() is worth doing.
void VulkanContext::transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = m_commandPool;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported image layout transition");
    }
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    vkCreateFence(m_device, &fenceInfo, nullptr, &fence);
    vkQueueSubmit(m_graphicsQueue, 1, &submit, fence);
    vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(m_device, fence, nullptr);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
}

void VulkanContext::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = m_commandPool;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent                 = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    vkCreateFence(m_device, &fenceInfo, nullptr, &fence);
    vkQueueSubmit(m_graphicsQueue, 1, &submit, fence);
    vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(m_device, fence, nullptr);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
}
