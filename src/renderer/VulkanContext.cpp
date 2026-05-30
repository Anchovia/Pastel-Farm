#include "VulkanContext.h"
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

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// Flat-shaded cube (24 verts, 4 per face — no shared normals, color from instance)
#define FACE(ax,ay,az, bx,by,bz, cx,cy,cz, dx,dy,dz, nx,ny,nz) \
    {{ax,ay,az},{nx,ny,nz}}, \
    {{bx,by,bz},{nx,ny,nz}}, \
    {{cx,cy,cz},{nx,ny,nz}}, \
    {{dx,dy,dz},{nx,ny,nz}}

static const std::vector<Vertex> kVertices = {
    FACE(-0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,  0, 0, 1),  // 윗면
    FACE(-0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f,-0.5f,-0.5f, -0.5f,-0.5f,-0.5f,  0, 0,-1),  // 아랫면
    FACE(-0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f, -0.5f,-0.5f, 0.5f,  0,-1, 0),  // 앞면
    FACE( 0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f, -0.5f, 0.5f, 0.5f,  0.5f, 0.5f, 0.5f,  0, 1, 0),  // 뒷면
    FACE( 0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  1, 0, 0),  // 오른면
    FACE(-0.5f, 0.5f,-0.5f, -0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -1, 0, 0),  // 왼면
};
#undef FACE

static const std::vector<uint16_t> kIndices = {
     0, 1, 2,   0, 2, 3,   // Top
     4, 5, 6,   4, 6, 7,   // Bottom
     8, 9,10,   8,10,11,   // Front
    12,13,14,  12,14,15,   // Back
    16,17,18,  16,18,19,   // Right
    20,21,22,  20,22,23,   // Left
};

static const std::vector<Vertex> kSelectorVertices = {
    {{-0.48f,  0.40f, 0.54f}, {0, 0, 1}},
    {{ 0.48f,  0.40f, 0.54f}, {0, 0, 1}},
    {{ 0.48f,  0.48f, 0.54f}, {0, 0, 1}},
    {{-0.48f,  0.48f, 0.54f}, {0, 0, 1}},

    {{ 0.40f, -0.48f, 0.54f}, {0, 0, 1}},
    {{ 0.48f, -0.48f, 0.54f}, {0, 0, 1}},
    {{ 0.48f,  0.48f, 0.54f}, {0, 0, 1}},
    {{ 0.40f,  0.48f, 0.54f}, {0, 0, 1}},

    {{-0.48f, -0.48f, 0.54f}, {0, 0, 1}},
    {{ 0.48f, -0.48f, 0.54f}, {0, 0, 1}},
    {{ 0.48f, -0.40f, 0.54f}, {0, 0, 1}},
    {{-0.48f, -0.40f, 0.54f}, {0, 0, 1}},

    {{-0.48f, -0.48f, 0.54f}, {0, 0, 1}},
    {{-0.40f, -0.48f, 0.54f}, {0, 0, 1}},
    {{-0.40f,  0.48f, 0.54f}, {0, 0, 1}},
    {{-0.48f,  0.48f, 0.54f}, {0, 0, 1}},
};

static const std::vector<uint16_t> kSelectorIndices = {
     0,  1,  2,   0,  2,  3,
     4,  5,  6,   4,  6,  7,
     8,  9, 10,   8, 10, 11,
    12, 13, 14,  12, 14, 15,
};

// ============================================================
//  Constants
// ============================================================
static const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
static const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
#ifdef NDEBUG
static constexpr bool kEnableValidation = false;
#else
static constexpr bool kEnableValidation = true;
#endif

// ============================================================
//  Debug messenger helpers
// ============================================================
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::cerr << "[Vulkan] " << data->pMessage << "\n";
    return VK_FALSE;
}

static VkResult CreateDebugMessenger(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* info,
    VkDebugUtilsMessengerEXT* out)
{
    auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    return fn ? fn(instance, info, nullptr, out) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger) {
    auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (fn) fn(instance, messenger, nullptr);
}

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
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createChunkPipeline();
    createDepthResources();
    createFramebuffers();
    createCommandPool();
    createVertexBuffer();
    createIndexBuffer();
    createSelectorBuffers();
    rebuildDirtyChunks();
    createPlayerInstanceBuffer({15.0f, 15.0f, 1.0f});
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

VulkanContext::~VulkanContext() {
    waitIdle();
    cleanupSwapchain();

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(m_device, m_uniformBuffers[i], nullptr);
        vkFreeMemory(m_device, m_uniformBuffersMemory[i], nullptr);
    }
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    vkDestroyBuffer(m_device, m_playerInstBuffer, nullptr);
    vkFreeMemory(m_device, m_playerInstMemory, nullptr);
    vkDestroyBuffer(m_device, m_selectorInstBuffer, nullptr);
    vkFreeMemory(m_device, m_selectorInstMemory, nullptr);
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
    }
    vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
    vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
    vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
    vkDestroyPipeline(m_device, m_chunkPipeline, nullptr);
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(m_device, m_imageAvailable[i], nullptr);
        vkDestroySemaphore(m_device, m_renderFinished[i], nullptr);
        vkDestroyFence(m_device, m_inFlight[i], nullptr);
    }
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    vkDestroyDevice(m_device, nullptr);
    if (kEnableValidation) DestroyDebugMessenger(m_instance, m_debugMessenger);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void VulkanContext::waitIdle() { vkDeviceWaitIdle(m_device); }

// ============================================================
//  Instance
// ============================================================
void VulkanContext::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "GameEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName        = "CustomEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    uint32_t    glfwExtCount = 0;
    const char** glfwExts    = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);
    if (kEnableValidation) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Fill debug create-info so it also covers instance creation/destruction
    VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
    debugInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.pfnUserCallback = debugCallback;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = (uint32_t)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();
    if (kEnableValidation) {
        createInfo.enabledLayerCount   = (uint32_t)kValidationLayers.size();
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
        createInfo.pNext               = &debugInfo;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan instance");
}

// ============================================================
//  Debug messenger
// ============================================================
void VulkanContext::setupDebugMessenger() {
    if (!kEnableValidation) return;
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
    CreateDebugMessenger(m_instance, &info, &m_debugMessenger);
}

// ============================================================
//  Surface
// ============================================================
void VulkanContext::createSurface() {
    if (glfwCreateWindowSurface(m_instance, m_window.handle(), nullptr, &m_surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface");
}

// ============================================================
//  Physical device
// ============================================================
VulkanContext::QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphics = i;
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport) indices.present = i;
        if (indices.complete()) break;
    }
    return indices;
}

void VulkanContext::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) throw std::runtime_error("No Vulkan-capable GPU found");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    // Prefer discrete GPU; otherwise take first suitable device
    VkPhysicalDevice fallback = VK_NULL_HANDLE;
    for (auto device : devices) {
        if (!findQueueFamilies(device).complete()) continue;
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            m_physicalDevice = device;
            std::cout << "GPU (discrete): " << props.deviceName << "\n";
            return;
        }
        if (fallback == VK_NULL_HANDLE) fallback = device;
    }
    if (fallback != VK_NULL_HANDLE) {
        m_physicalDevice = fallback;
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(fallback, &props);
        std::cout << "GPU (integrated): " << props.deviceName << "\n";
        return;
    }
    throw std::runtime_error("No suitable GPU found");
}

// ============================================================
//  Logical device + queues
// ============================================================
void VulkanContext::createLogicalDevice() {
    auto indices = findQueueFamilies(m_physicalDevice);
    std::set<uint32_t> uniqueFamilies = { *indices.graphics, *indices.present };

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family;
        qi.queueCount       = 1;
        qi.pQueuePriorities = &priority;
        queueInfos.push_back(qi);
    }

    VkPhysicalDeviceFeatures features{};  // nothing needed for Phase 1

    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = (uint32_t)queueInfos.size();
    createInfo.pQueueCreateInfos       = queueInfos.data();
    createInfo.pEnabledFeatures        = &features;
    createInfo.enabledExtensionCount   = (uint32_t)kDeviceExtensions.size();
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device");

    vkGetDeviceQueue(m_device, *indices.graphics, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, *indices.present,  0, &m_presentQueue);
}

// ============================================================
//  Swapchain
// ============================================================
void VulkanContext::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &caps);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &modeCount, modes.data());

    // Format: prefer BGRA8 sRGB
    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            chosenFormat = f;

    // Present mode: prefer mailbox (triple buffer, low latency), fallback to FIFO (vsync)
    VkPresentModeKHR chosenMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto& m : modes)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) { chosenMode = m; break; }

    // Extent
    VkExtent2D extent;
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent = { (uint32_t)m_window.width(), (uint32_t)m_window.height() };
        extent.width  = std::clamp(extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR info{};
    info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface          = m_surface;
    info.minImageCount    = imageCount;
    info.imageFormat      = chosenFormat.format;
    info.imageColorSpace  = chosenFormat.colorSpace;
    info.imageExtent      = extent;
    info.imageArrayLayers = 1;
    info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.preTransform     = caps.currentTransform;
    info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode      = chosenMode;
    info.clipped          = VK_TRUE;

    auto indices = findQueueFamilies(m_physicalDevice);
    uint32_t queueFamilies[] = { *indices.graphics, *indices.present };
    if (*indices.graphics != *indices.present) {
        info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices   = queueFamilies;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    if (vkCreateSwapchainKHR(m_device, &info, nullptr, &m_swapchain) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swapchain");

    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());

    m_swapchainFormat = chosenFormat.format;
    m_swapchainExtent = extent;
}

// ============================================================
//  Image views
// ============================================================
void VulkanContext::createImageViews() {
    m_swapchainImageViews.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_swapchainImages.size(); i++) {
        VkImageViewCreateInfo info{};
        info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image                           = m_swapchainImages[i];
        info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        info.format                          = m_swapchainFormat;
        info.components                      = { VK_COMPONENT_SWIZZLE_IDENTITY,
                                                 VK_COMPONENT_SWIZZLE_IDENTITY,
                                                 VK_COMPONENT_SWIZZLE_IDENTITY,
                                                 VK_COMPONENT_SWIZZLE_IDENTITY };
        info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel   = 0;
        info.subresourceRange.levelCount     = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount     = 1;
        if (vkCreateImageView(m_device, &info, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create image view");
    }
}

// ============================================================
//  Render pass
// ============================================================
void VulkanContext::createRenderPass() {
    VkAttachmentDescription color{};
    color.format         = m_swapchainFormat;
    color.samples        = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth{};
    depth.format         = findDepthFormat();
    depth.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {color, depth};
    VkRenderPassCreateInfo info{};
    info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 2;
    info.pAttachments    = attachments;
    info.subpassCount    = 1;
    info.pSubpasses      = &subpass;
    info.dependencyCount = 1;
    info.pDependencies   = &dep;

    if (vkCreateRenderPass(m_device, &info, nullptr, &m_renderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass");
}

// ============================================================
//  Graphics pipeline
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

void VulkanContext::createGraphicsPipeline() {
    auto vertCode = readFile("shaders/triangle.vert.spv");
    auto fragCode = readFile("shaders/triangle.frag.spv");
    VkShaderModule vertMod = createShaderModule(vertCode);
    VkShaderModule fragMod = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName  = "main";

    // Register Vertex layout with pipeline
    VkVertexInputBindingDescription bindingDescs[2]{};
    bindingDescs[0].binding   = 0;
    bindingDescs[0].stride    = sizeof(Vertex);
    bindingDescs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescs[1].binding   = 1;
    bindingDescs[1].stride    = sizeof(InstanceData);
    bindingDescs[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    VkVertexInputAttributeDescription attributeDescs[5]{};
    attributeDescs[0].binding  = 0;
    attributeDescs[0].location = 0;
    attributeDescs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescs[0].offset   = offsetof(Vertex, pos);
    attributeDescs[1].binding  = 0;
    attributeDescs[1].location = 1;
    attributeDescs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescs[1].offset   = offsetof(Vertex, normal);
    attributeDescs[2].binding  = 1;
    attributeDescs[2].location = 2;
    attributeDescs[2].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescs[2].offset   = offsetof(InstanceData, pos);
    attributeDescs[3].binding  = 1;
    attributeDescs[3].location = 3;
    attributeDescs[3].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescs[3].offset   = offsetof(InstanceData, topColor);
    attributeDescs[4].binding  = 1;
    attributeDescs[4].location = 4;
    attributeDescs[4].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescs[4].offset   = offsetof(InstanceData, sideColor);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 2;
    vertexInput.pVertexBindingDescriptions      = bindingDescs;
    vertexInput.vertexAttributeDescriptionCount = 5;
    vertexInput.pVertexAttributeDescriptions    = attributeDescs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{ 0, 0,
        (float)m_swapchainExtent.width, (float)m_swapchainExtent.height, 0.0f, 1.0f };
    VkRect2D scissor{ {0, 0}, m_swapchainExtent };

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode  = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo msaa{};
    msaa.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttach{};
    blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments    = &blendAttach;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable  = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &m_descriptorSetLayout;
    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = stages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState   = &msaa;
    pipelineInfo.pColorBlendState    = &blend;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.layout              = m_pipelineLayout;
    pipelineInfo.renderPass          = m_renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create graphics pipeline");

    vkDestroyShaderModule(m_device, vertMod, nullptr);
    vkDestroyShaderModule(m_device, fragMod, nullptr);
}

void VulkanContext::createChunkPipeline() {
    auto vertCode = readFile("shaders/chunk.vert.spv");
    auto fragCode = readFile("shaders/chunk.frag.spv");
    VkShaderModule vertMod = createShaderModule(vertCode);
    VkShaderModule fragMod = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName  = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = sizeof(ChunkVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[3]{};
    attrs[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ChunkVertex, pos)    };
    attrs[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ChunkVertex, normal) };
    attrs[2] = { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ChunkVertex, color)  };

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &binding;
    vertexInput.vertexAttributeDescriptionCount = 3;
    vertexInput.pVertexAttributeDescriptions    = attrs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{ 0, 0,
        (float)m_swapchainExtent.width, (float)m_swapchainExtent.height, 0.0f, 1.0f };
    VkRect2D scissor{ {0, 0}, m_swapchainExtent };

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode    = VK_CULL_MODE_BACK_BIT;
    raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo msaa{};
    msaa.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttach{};
    blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments    = &blendAttach;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable  = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = stages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState   = &msaa;
    pipelineInfo.pColorBlendState    = &blend;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.layout              = m_pipelineLayout;
    pipelineInfo.renderPass          = m_renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_chunkPipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create chunk pipeline");

    vkDestroyShaderModule(m_device, vertMod, nullptr);
    vkDestroyShaderModule(m_device, fragMod, nullptr);
}

// ============================================================
//  Framebuffers
// ============================================================
void VulkanContext::createFramebuffers() {
    m_framebuffers.resize(m_swapchainImageViews.size());
    for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
        VkFramebufferCreateInfo info{};
        VkImageView attachments[] = { m_swapchainImageViews[i], m_depthImageView };
        info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass      = m_renderPass;
        info.attachmentCount = 2;
        info.pAttachments    = attachments;
        info.width           = m_swapchainExtent.width;
        info.height          = m_swapchainExtent.height;
        info.layers          = 1;
        if (vkCreateFramebuffer(m_device, &info, nullptr, &m_framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer");
    }
}

// ============================================================
//  Command pool + buffers
// ============================================================
void VulkanContext::createCommandPool() {
    auto indices = findQueueFamilies(m_physicalDevice);
    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = *indices.graphics;
    if (vkCreateCommandPool(m_device, &info, nullptr, &m_commandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");
}

void VulkanContext::createCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo info{};
    info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool        = m_commandPool;
    info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    if (vkAllocateCommandBuffers(m_device, &info, m_commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers");
}

void VulkanContext::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &begin);

    VkClearValue clearValues[2];
    clearValues[0].color        = {{0.08f, 0.08f, 0.12f, 1.0f}};
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
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

// ============================================================
//  Sync objects
// ============================================================
void VulkanContext::createSyncObjects() {
    m_imageAvailable.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinished.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlight.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // start signaled so frame 0 doesn't wait forever

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkCreateSemaphore(m_device, &semInfo,   nullptr, &m_imageAvailable[i]);
        vkCreateSemaphore(m_device, &semInfo,   nullptr, &m_renderFinished[i]);
        vkCreateFence    (m_device, &fenceInfo, nullptr, &m_inFlight[i]);
    }
}

// ============================================================
//  drawFrame
// ============================================================
void VulkanContext::drawFrame(const Camera& camera, const glm::vec3& playerPosition, const std::optional<glm::ivec3>& targetTile) {
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
    updateUniformBuffer(m_currentFrame, camera);
    updatePlayerInstanceBuffer(playerPosition);
    updateSelectorInstanceBuffer(targetTile);
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
//  Swapchain recreation (window resize)
// ============================================================
void VulkanContext::cleanupSwapchain() {
    vkDestroyImageView(m_device, m_depthImageView, nullptr);
    vkDestroyImage    (m_device, m_depthImage,     nullptr);
    vkFreeMemory      (m_device, m_depthImageMemory, nullptr);
    for (auto fb : m_framebuffers)         vkDestroyFramebuffer(m_device, fb, nullptr);
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
    createFramebuffers();
}

// ============================================================
//  Vertex buffer
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

// ============================================================
//  Depth resources
// ============================================================
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

void VulkanContext::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();
    createImage(m_swapchainExtent.width, m_swapchainExtent.height, depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_depthImage, m_depthImageMemory);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_depthImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = depthFormat;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create depth image view");
}

// ============================================================
//  Descriptor set layout
// ============================================================
void VulkanContext::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding         = 0;
    uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings    = &uboBinding;

    if (vkCreateDescriptorSetLayout(m_device, &info, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout");
}

// ============================================================
//  Uniform buffers
// ============================================================
void VulkanContext::createUniformBuffers() {
    VkDeviceSize size = sizeof(UniformBufferObject);
    m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_uniformBuffers[i], m_uniformBuffersMemory[i]);
        vkMapMemory(m_device, m_uniformBuffersMemory[i], 0, size, 0, &m_uniformBuffersMapped[i]);
    }
}

// ============================================================
//  Descriptor pool + sets
// ============================================================
void VulkanContext::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo info{};
    info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = 1;
    info.pPoolSizes    = &poolSize;
    info.maxSets       = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(m_device, &info, nullptr, &m_descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool");
}

void VulkanContext::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_descriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts        = layouts.data();

    m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor sets");

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(UniformBufferObject);

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = m_descriptorSets[i];
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }
}

// ============================================================
//  Update uniform buffer (called every frame)
// ============================================================
void VulkanContext::updateUniformBuffer(uint32_t currentFrame, const Camera& camera) {
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = camera.view();
    ubo.proj = camera.proj();

    memcpy(m_uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}

void VulkanContext::updatePlayerInstanceBuffer(const glm::vec3& playerPosition) {
    static const glm::vec3 kPlayerColor = {1.0f, 0.45f, 0.1f};
    InstanceData inst{playerPosition, kPlayerColor, kPlayerColor};
    memcpy(m_playerInstMapped, &inst, sizeof(inst));
}

void VulkanContext::createPlayerInstanceBuffer(const glm::vec3& playerPosition) {
    VkDeviceSize size = sizeof(InstanceData);
    createBuffer(size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_playerInstBuffer, m_playerInstMemory);
    vkMapMemory(m_device, m_playerInstMemory, 0, size, 0, &m_playerInstMapped);

    updatePlayerInstanceBuffer(playerPosition);
}

void VulkanContext::updateSelectorInstanceBuffer(const std::optional<glm::ivec3>& targetTile) {
    m_showSelector = targetTile.has_value();
    if (!m_showSelector) return;

    static const glm::vec3 kSelectorColor = {1.0f, 0.9f, 0.1f};
    const glm::ivec3 tile = *targetTile;
    InstanceData inst{m_world.tileCenter(tile.x, tile.y, tile.z), kSelectorColor, kSelectorColor};
    memcpy(m_selectorInstMapped, &inst, sizeof(inst));
}

void VulkanContext::createSelectorBuffers() {
    VkDeviceSize vertexSize = sizeof(kSelectorVertices[0]) * kSelectorVertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    createBuffer(vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);
    void* data;
    vkMapMemory(m_device, stagingMemory, 0, vertexSize, 0, &data);
    memcpy(data, kSelectorVertices.data(), vertexSize);
    vkUnmapMemory(m_device, stagingMemory);

    createBuffer(vertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_selectorVertexBuffer, m_selectorVertexMemory);
    copyBuffer(stagingBuffer, m_selectorVertexBuffer, vertexSize);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);

    VkDeviceSize indexSize = sizeof(kSelectorIndices[0]) * kSelectorIndices.size();
    createBuffer(indexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);

    vkMapMemory(m_device, stagingMemory, 0, indexSize, 0, &data);
    memcpy(data, kSelectorIndices.data(), indexSize);
    vkUnmapMemory(m_device, stagingMemory);

    createBuffer(indexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_selectorIndexBuffer, m_selectorIndexMemory);
    copyBuffer(stagingBuffer, m_selectorIndexBuffer, indexSize);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);

    VkDeviceSize instanceSize = sizeof(InstanceData);
    createBuffer(instanceSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_selectorInstBuffer, m_selectorInstMemory);
    vkMapMemory(m_device, m_selectorInstMemory, 0, instanceSize, 0, &m_selectorInstMapped);
}

void VulkanContext::buildChunkBuffer(const glm::ivec2& coord, Chunk& chunk) {
    // 6 face local vertex offsets, normals, neighbor offsets, top-face flag
    struct FaceDef {
        glm::vec3  verts[4];
        glm::vec3  normal;
        glm::ivec3 neighborOff;
        bool       isTop;
    };
    static const FaceDef kFaces[6] = {
        // Top (+Z)
        {{{-0.5f,-0.5f,0.5f},{0.5f,-0.5f,0.5f},{0.5f,0.5f,0.5f},{-0.5f,0.5f,0.5f}}, {0,0,1},  {0,0,1},  true  },
        // Bottom (-Z)
        {{{-0.5f,0.5f,-0.5f},{0.5f,0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{-0.5f,-0.5f,-0.5f}}, {0,0,-1}, {0,0,-1}, false },
        // Front (+Y)
        {{{0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f},{-0.5f,0.5f,0.5f},{0.5f,0.5f,0.5f}},  {0,1,0},  {0,1,0},  false },
        // Back (-Y)
        {{{-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,-0.5f,0.5f},{-0.5f,-0.5f,0.5f}}, {0,-1,0}, {0,-1,0}, false },
        // Right (+X)
        {{{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{0.5f,0.5f,0.5f},{0.5f,-0.5f,0.5f}},  {1,0,0},  {1,0,0},  false },
        // Left (-X)
        {{{-0.5f,0.5f,-0.5f},{-0.5f,-0.5f,-0.5f},{-0.5f,-0.5f,0.5f},{-0.5f,0.5f,0.5f}}, {-1,0,0}, {-1,0,0}, false },
    };

    const int baseX = coord.x * CHUNK_SIZE;
    const int baseY = coord.y * CHUNK_SIZE;

    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t>    indices;

    for (int z  = 0; z  < CHUNK_DEPTH; z++)
    for (int ly = 0; ly < CHUNK_SIZE;  ly++)
    for (int lx = 0; lx < CHUNK_SIZE;  lx++) {
        TileType t = chunk.tiles[z][ly][lx];
        if (t == TileType::AIR) continue;

        const int wx = baseX + lx;
        const int wy = baseY + ly;
        const glm::vec3 topColor  = World::tileColor(t);
        const glm::vec3 sideColor = World::tileSideColor(t);
        const glm::vec3 center    = { (float)wx, (float)wy, (float)z };

        for (const auto& face : kFaces) {
            // Skip if neighbor tile is opaque
            TileType neighbor = m_world.getTile(
                wx + face.neighborOff.x,
                wy + face.neighborOff.y,
                z  + face.neighborOff.z
            );
            if (neighbor != TileType::AIR) continue;

            const glm::vec3 color = face.isTop ? topColor : sideColor;
            const uint32_t  base  = (uint32_t)vertices.size();

            for (int i = 0; i < 4; i++)
                vertices.push_back({ center + face.verts[i], face.normal, color });

            indices.insert(indices.end(), {
                base+0, base+1, base+2,
                base+0, base+2, base+3
            });
        }
    }

    auto& data = m_chunkBuffers[coord];

    // Release old buffers
    if (data.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, data.vertexBuffer, nullptr);
        vkFreeMemory(m_device, data.vertexMemory, nullptr);
        data.vertexBuffer = VK_NULL_HANDLE;
    }
    if (data.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, data.indexBuffer, nullptr);
        vkFreeMemory(m_device, data.indexMemory, nullptr);
        data.indexBuffer = VK_NULL_HANDLE;
    }

    data.indexCount = (uint32_t)indices.size();
    if (data.indexCount == 0) return;

    // Vertex buffer
    VkDeviceSize vSize = sizeof(ChunkVertex) * vertices.size();
    createBuffer(vSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        data.vertexBuffer, data.vertexMemory);
    void* vMapped;
    vkMapMemory(m_device, data.vertexMemory, 0, vSize, 0, &vMapped);
    memcpy(vMapped, vertices.data(), vSize);
    vkUnmapMemory(m_device, data.vertexMemory);

    // Index buffer
    VkDeviceSize iSize = sizeof(uint32_t) * indices.size();
    createBuffer(iSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        data.indexBuffer, data.indexMemory);
    void* iMapped;
    vkMapMemory(m_device, data.indexMemory, 0, iSize, 0, &iMapped);
    memcpy(iMapped, indices.data(), iSize);
    vkUnmapMemory(m_device, data.indexMemory);
}

void VulkanContext::rebuildDirtyChunks() {
    // Free GPU buffers for chunks no longer in the world
    for (auto it = m_chunkBuffers.begin(); it != m_chunkBuffers.end(); ) {
        if (m_world.chunks().find(it->first) == m_world.chunks().end()) {
            auto& d = it->second;
            if (d.vertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device, d.vertexBuffer, nullptr);
                vkFreeMemory(m_device, d.vertexMemory, nullptr);
            }
            if (d.indexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device, d.indexBuffer, nullptr);
                vkFreeMemory(m_device, d.indexMemory, nullptr);
            }
            it = m_chunkBuffers.erase(it);
        } else {
            ++it;
        }
    }

    // Rebuild dirty chunks
    for (auto& [coord, chunk] : m_world.chunks()) {
        if (!chunk.dirty) continue;
        buildChunkBuffer(coord, chunk);
        chunk.dirty = false;
    }
}

void VulkanContext::createIndexBuffer() {
    VkDeviceSize size = sizeof(kIndices[0]) * kIndices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_device, stagingBufferMemory, 0, size, 0, &data);
    memcpy(data, kIndices.data(), (size_t)size);
    vkUnmapMemory(m_device, stagingBufferMemory);

    createBuffer(size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_indexBuffer, m_indexBufferMemory);

    copyBuffer(stagingBuffer, m_indexBuffer, size);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingBufferMemory, nullptr);
}

void VulkanContext::createVertexBuffer() {
    VkDeviceSize size = sizeof(kVertices[0]) * kVertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_device, stagingBufferMemory, 0, size, 0, &data);
    memcpy(data, kVertices.data(), (size_t)size);
    vkUnmapMemory(m_device, stagingBufferMemory);

    createBuffer(size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_vertexBuffer, m_vertexBufferMemory);

    copyBuffer(stagingBuffer, m_vertexBuffer, size);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingBufferMemory, nullptr);
}

void VulkanContext::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
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
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    vkCreateFence(m_device, &fenceInfo, nullptr, &fence);

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, fence);
    vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(m_device, fence, nullptr);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}