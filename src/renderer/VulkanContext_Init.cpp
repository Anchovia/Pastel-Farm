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

    VkPhysicalDeviceFeatures features{};

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
//  Graphics pipelines
// ============================================================
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

void VulkanContext::createUIPipeline() {
    auto vertCode = readFile("shaders/ui.vert.spv");
    auto fragCode = readFile("shaders/ui.frag.spv");
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
    binding.stride    = sizeof(UIVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0] = { 0, 0, VK_FORMAT_R32G32_SFLOAT,       offsetof(UIVertex, pos)   };
    attrs[1] = { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(UIVertex, color) };

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &binding;
    vertexInput.vertexAttributeDescriptionCount = 2;
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
    raster.cullMode    = VK_CULL_MODE_NONE;
    raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo msaa{};
    msaa.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Alpha blending for semi-transparent panels
    VkPipelineColorBlendAttachmentState blendAttach{};
    blendAttach.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttach.blendEnable         = VK_TRUE;
    blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
    blendAttach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments    = &blendAttach;

    // UI draws on top of everything — no depth test/write
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable  = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp   = VK_COMPARE_OP_ALWAYS;

    // No descriptor sets — UI vertices are already in NDC
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_uiPipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create UI pipeline layout");

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
    pipelineInfo.layout              = m_uiPipelineLayout;
    pipelineInfo.renderPass          = m_renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_uiPipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create UI pipeline");

    vkDestroyShaderModule(m_device, vertMod, nullptr);
    vkDestroyShaderModule(m_device, fragMod, nullptr);
}

void VulkanContext::createObjectPipeline() {
    auto vertCode = readFile("shaders/object.vert.spv");
    auto fragCode = readFile("shaders/chunk.frag.spv");  // reuse chunk fragment shader
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

    VkVertexInputBindingDescription bindings[2]{};
    bindings[0].binding   = 0;
    bindings[0].stride    = sizeof(ChunkVertex);
    bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings[1].binding   = 1;
    bindings[1].stride    = sizeof(ObjectInstance);
    bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    VkVertexInputAttributeDescription attrs[6]{};
    attrs[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ChunkVertex, pos)    };
    attrs[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ChunkVertex, normal) };
    attrs[2] = { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ChunkVertex, color)  };
    attrs[3] = { 3, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ObjectInstance, pos)   };
    attrs[4] = { 4, 1, VK_FORMAT_R32_SFLOAT,       offsetof(ObjectInstance, scale) };
    attrs[5] = { 5, 1, VK_FORMAT_R32_SFLOAT,       offsetof(ObjectInstance, rot)   };

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 2;
    vertexInput.pVertexBindingDescriptions      = bindings;
    vertexInput.vertexAttributeDescriptionCount = 6;
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
    raster.cullMode    = VK_CULL_MODE_NONE;  // procedural mesh — avoid winding issues
    raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo msaaObj{};
    msaaObj.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaaObj.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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
    pipelineInfo.pMultisampleState   = &msaaObj;
    pipelineInfo.pColorBlendState    = &blend;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.layout              = m_pipelineLayout;  // reuse UBO descriptor layout
    pipelineInfo.renderPass          = m_renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_objectPipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create object pipeline");

    vkDestroyShaderModule(m_device, vertMod, nullptr);
    vkDestroyShaderModule(m_device, fragMod, nullptr);
}

// ============================================================
//  UI buffer
// ============================================================
// Capacity: hotbar (~66 verts) + inventory overlay (~120 verts) + margin
void VulkanContext::createUIBuffer() {
    VkDeviceSize size = sizeof(UIVertex) * 512;
    createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_uiBuffer, m_uiMemory);
    vkMapMemory(m_device, m_uiMemory, 0, size, 0, &m_uiMapped);
}

// ============================================================
//  Tree mesh
// ============================================================
// Shared low-poly pine tree: box trunk + 3 stacked cones, flat shaded.
void VulkanContext::createTreeMesh() {
    std::vector<ChunkVertex> verts;

    const glm::vec3 trunkColor = {0.40f, 0.26f, 0.13f};
    const glm::vec3 leafColor  = {0.28f, 0.52f, 0.24f};

    // Trunk: 4 side faces of a thin box (Z up), base at 0, top at 0.55
    const float tw = 0.09f, t0 = 0.0f, t1 = 0.55f;
    const glm::vec3 trunkCorners[4] = {
        {-tw, -tw, 0}, { tw, -tw, 0}, { tw, tw, 0}, {-tw, tw, 0}
    };
    for (int i = 0; i < 4; i++) {
        glm::vec3 a = trunkCorners[i];
        glm::vec3 b = trunkCorners[(i + 1) % 4];
        glm::vec3 n = glm::normalize(glm::vec3((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, 0.0f));
        glm::vec3 a0 = {a.x, a.y, t0}, b0 = {b.x, b.y, t0};
        glm::vec3 a1 = {a.x, a.y, t1}, b1 = {b.x, b.y, t1};
        verts.push_back({a0, n, trunkColor});
        verts.push_back({b0, n, trunkColor});
        verts.push_back({b1, n, trunkColor});
        verts.push_back({a0, n, trunkColor});
        verts.push_back({b1, n, trunkColor});
        verts.push_back({a1, n, trunkColor});
    }

    // Canopy: 3 stacked cones
    struct Cone { float baseZ, radius, topZ; };
    const Cone cones[3] = {
        {0.35f, 0.45f, 0.95f},
        {0.70f, 0.34f, 1.25f},
        {1.00f, 0.22f, 1.55f},
    };
    const int seg = 8;
    for (const auto& cone : cones) {
        for (int i = 0; i < seg; i++) {
            float a0 = (float)i       / seg * 6.2831853f;
            float a1 = (float)(i + 1) / seg * 6.2831853f;
            glm::vec3 b0   = {cone.radius * cosf(a0), cone.radius * sinf(a0), cone.baseZ};
            glm::vec3 b1   = {cone.radius * cosf(a1), cone.radius * sinf(a1), cone.baseZ};
            glm::vec3 apex = {0.0f, 0.0f, cone.topZ};
            glm::vec3 n    = glm::normalize(glm::cross(b1 - b0, apex - b0));
            verts.push_back({b0,   n, leafColor});
            verts.push_back({b1,   n, leafColor});
            verts.push_back({apex, n, leafColor});
        }
    }

    m_treeVertexCount = (uint32_t)verts.size();
    VkDeviceSize size = sizeof(ChunkVertex) * verts.size();
    createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_treeVertexBuffer, m_treeVertexMemory);
    void* mapped;
    vkMapMemory(m_device, m_treeVertexMemory, 0, size, 0, &mapped);
    memcpy(mapped, verts.data(), size);
    vkUnmapMemory(m_device, m_treeVertexMemory);
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
//  Depth resources
// ============================================================
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
    uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

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
//  Static geometry buffers
// ============================================================
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

void VulkanContext::createPlayerInstanceBuffer(const glm::vec3& playerPosition) {
    VkDeviceSize size = sizeof(InstanceData);
    createBuffer(size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_playerInstBuffer, m_playerInstMemory);
    vkMapMemory(m_device, m_playerInstMemory, 0, size, 0, &m_playerInstMapped);

    updatePlayerInstanceBuffer(playerPosition);
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
