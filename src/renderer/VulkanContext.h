#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <optional>
#include <string>

class Window;
class World;

class VulkanContext {
public:
    VulkanContext(Window& window, World& world);
    ~VulkanContext();

    void drawFrame(const glm::vec3& playerPosition);
    void waitIdle();
    void rotateOrbit(float degrees);
    float orbitAngle() const { return m_orbitAngle; }

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void createDescriptorSetLayout();
    void createVertexBuffer();
    void createIndexBuffer();
    void createInstanceBuffer();
    void createPlayerInstanceBuffer(const glm::vec3& playerPosition);
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentFrame, const glm::vec3& playerPosition);
    void updatePlayerInstanceBuffer(const glm::vec3& playerPosition);
    void createDepthResources();
    void createImage(uint32_t width, uint32_t height, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory);
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
        VkImageTiling tiling, VkFormatFeatureFlags features);

    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void cleanupSwapchain();
    void recreateSwapchain();

    VkShaderModule          createShaderModule(const std::vector<char>& code);
    std::vector<char>       readFile(const std::string& path);
    uint32_t                findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void                    createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                VkMemoryPropertyFlags properties,
                                VkBuffer& buffer, VkDeviceMemory& memory);

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;
        bool complete() const { return graphics && present; }
    };
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    // ---- Vulkan handles ----
    Window& m_window;
    World&  m_world;

    VkInstance               m_instance        = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger   = VK_NULL_HANDLE;
    VkSurfaceKHR             m_surface          = VK_NULL_HANDLE;
    VkPhysicalDevice         m_physicalDevice   = VK_NULL_HANDLE;
    VkDevice                 m_device           = VK_NULL_HANDLE;
    VkQueue                  m_graphicsQueue    = VK_NULL_HANDLE;
    VkQueue                  m_presentQueue     = VK_NULL_HANDLE;

    VkSwapchainKHR           m_swapchain        = VK_NULL_HANDLE;
    std::vector<VkImage>     m_swapchainImages;
    VkFormat                 m_swapchainFormat  = VK_FORMAT_UNDEFINED;
    VkExtent2D               m_swapchainExtent  = {};
    std::vector<VkImageView> m_swapchainImageViews;
    std::vector<VkFramebuffer> m_framebuffers;

    VkRenderPass             m_renderPass       = VK_NULL_HANDLE;
    VkPipelineLayout         m_pipelineLayout   = VK_NULL_HANDLE;
    VkPipeline               m_pipeline         = VK_NULL_HANDLE;

    VkBuffer                 m_vertexBuffer        = VK_NULL_HANDLE;
    VkDeviceMemory           m_vertexBufferMemory  = VK_NULL_HANDLE;
    VkBuffer                 m_indexBuffer         = VK_NULL_HANDLE;
    VkDeviceMemory           m_indexBufferMemory   = VK_NULL_HANDLE;
    VkBuffer                 m_instanceBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory           m_instanceBufferMemory = VK_NULL_HANDLE;
    uint32_t                 m_instanceCount        = 0;
    VkBuffer                 m_playerInstBuffer     = VK_NULL_HANDLE;
    VkDeviceMemory           m_playerInstMemory     = VK_NULL_HANDLE;
    void*                    m_playerInstMapped     = nullptr;

    VkImage                      m_depthImage           = VK_NULL_HANDLE;
    VkDeviceMemory               m_depthImageMemory     = VK_NULL_HANDLE;
    VkImageView                  m_depthImageView       = VK_NULL_HANDLE;

    VkDescriptorSetLayout        m_descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkBuffer>        m_uniformBuffers;
    std::vector<VkDeviceMemory>  m_uniformBuffersMemory;
    std::vector<void*>           m_uniformBuffersMapped;
    VkDescriptorPool             m_descriptorPool   = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;

    VkCommandPool            m_commandPool      = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> m_imageAvailable;
    std::vector<VkSemaphore> m_renderFinished;
    std::vector<VkFence>     m_inFlight;
    uint32_t                 m_currentFrame     = 0;

    glm::vec3 m_orbitTarget   = {0.0f, 0.0f, 0.0f};
    float     m_orbitAngle    = 45.0f;
    float     m_orbitDistance = 14.0f;
    float     m_orbitPitch    = 45.0f;
};
