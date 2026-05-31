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
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createChunkPipeline();
    createUIPipeline();
    createObjectPipeline();
    createPostPipeline();
    createDepthResources();
    createOffscreenResources();
    createShadowResources();
    createShadowPipeline();
    createShadowObjectPipeline();
    createShadowPlayerPipeline();
    createFramebuffers();
    createCommandPool();
    createVertexBuffer();
    createIndexBuffer();
    createSelectorBuffers();
    createUIBuffer();
    createTreeMesh();
    rebuildDirtyChunks();
    createPlayerInstanceBuffer({15.0f, 15.0f, 1.0f});
    createUniformBuffers();
    createShadowSampler();
    createPostSampler();
    createDescriptorPool();
    createDescriptorSets();
    createPostDescriptors();
    createCommandBuffers();
    createSyncObjects();
}

VulkanContext::~VulkanContext() {
    waitIdle();

    for (auto& d : m_deletionQueue) {
        vkDestroyBuffer(m_device, d.buffer, nullptr);
        vkFreeMemory(m_device, d.memory, nullptr);
    }
    m_deletionQueue.clear();

    cleanupSwapchain();

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(m_device, m_uniformBuffers[i], nullptr);
        vkFreeMemory(m_device, m_uniformBuffersMemory[i], nullptr);
    }
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(m_device, m_playerInstBuffer[i], nullptr);
        vkFreeMemory(m_device, m_playerInstMemory[i], nullptr);
        vkDestroyBuffer(m_device, m_selectorInstBuffer[i], nullptr);
        vkFreeMemory(m_device, m_selectorInstMemory[i], nullptr);
    }
    vkDestroyBuffer(m_device, m_selectorIndexBuffer, nullptr);
    vkFreeMemory(m_device, m_selectorIndexMemory, nullptr);
    vkDestroyBuffer(m_device, m_selectorVertexBuffer, nullptr);
    vkFreeMemory(m_device, m_selectorVertexMemory, nullptr);
    for (auto& [coord, data] : m_chunkBuffers) {
        if (data.vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, data.vertexBuffer, nullptr);
            vkFreeMemory(m_device, data.vertexMemory, nullptr);
        }
        if (data.indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, data.indexBuffer, nullptr);
            vkFreeMemory(m_device, data.indexMemory, nullptr);
        }
        if (data.objInstBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, data.objInstBuffer, nullptr);
            vkFreeMemory(m_device, data.objInstMemory, nullptr);
        }
    }
    vkDestroyBuffer(m_device, m_treeVertexBuffer, nullptr);
    vkFreeMemory(m_device, m_treeVertexMemory, nullptr);
    vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
    vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
    vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(m_device, m_uiBuffer[i], nullptr);
        vkFreeMemory(m_device, m_uiMemory[i], nullptr);
    }
    vkDestroyPipeline(m_device, m_uiPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_uiPipelineLayout, nullptr);
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
    vkDestroySampler            (m_device, m_postSampler,             nullptr);
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

void VulkanContext::deferDestroy(VkBuffer buf, VkDeviceMemory mem) {
    if (buf != VK_NULL_HANDLE)
        m_deletionQueue.push_back({buf, mem, m_frameCount});
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

void VulkanContext::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory)
{
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(m_device, &bufInfo, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(m_device, buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate buffer memory");

    vkBindBufferMemory(m_device, buffer, memory, 0);
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
    for (auto fb : m_sceneFramebuffers)    vkDestroyFramebuffer(m_device, fb, nullptr);
    for (auto fb : m_postFramebuffers)     vkDestroyFramebuffer(m_device, fb, nullptr);
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
    createFramebuffers();
    updatePostDescriptors();   // offscreen views were recreated

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
