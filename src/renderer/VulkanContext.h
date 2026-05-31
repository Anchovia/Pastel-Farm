#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <optional>
#include <string>
#include <unordered_map>
#include <array>
#include "world/Chunk.h"
#include "renderer/Frustum.h"

class Window;
class World;
class Camera;

class VulkanContext {
public:
    VulkanContext(Window& window, World& world);
    ~VulkanContext();

    void drawFrame(const Camera& camera, const glm::vec3& playerPosition, const std::optional<glm::ivec3>& targetTile,
                   int hotbarSelected, const std::array<ItemType, HOTBAR_SLOTS>& palette, float timeOfDay, bool inventoryOpen);
    void waitIdle();

private:
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
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
    void createSelectorBuffers();
    void createChunkPipeline();
    void buildChunkBuffer(const glm::ivec2& coord, Chunk& chunk);
    void buildChunkObjectBuffer(const glm::ivec2& coord, Chunk& chunk);
    void rebuildDirtyChunks();
    void createUIPipeline();
    void createUIBuffer();
    void updateHotbar();
    void createObjectPipeline();
    void createTreeMesh();
    void createPlayerInstanceBuffer(const glm::vec3& playerPosition);
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentFrame, const Camera& camera);
    void updatePlayerInstanceBuffer(const glm::vec3& playerPosition);
    void updateSelectorInstanceBuffer(const std::optional<glm::ivec3>& targetTile);
    void createDepthResources();
    void createShadowResources();
    void createShadowPipeline();
    void createShadowSampler();
    void createImage(uint32_t width, uint32_t height, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory);
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
        VkImageTiling tiling, VkFormatFeatureFlags features);

    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void cleanupSwapchain();
    void recreateSwapchain();

    void deferDestroy(VkBuffer buf, VkDeviceMemory mem);

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

    VkRenderPass             m_renderPass        = VK_NULL_HANDLE;
    VkPipelineLayout         m_pipelineLayout    = VK_NULL_HANDLE;
    VkPipeline               m_pipeline          = VK_NULL_HANDLE;  // Player / selector (instancing)
    VkPipeline               m_chunkPipeline     = VK_NULL_HANDLE;  // Chunk mesh
    VkPipeline               m_uiPipeline        = VK_NULL_HANDLE;  // 2D UI overlay
    VkPipelineLayout         m_uiPipelineLayout  = VK_NULL_HANDLE;
    VkPipeline               m_objectPipeline    = VK_NULL_HANDLE;  // Instanced low-poly props (trees)

    VkBuffer                 m_vertexBuffer        = VK_NULL_HANDLE;
    VkDeviceMemory           m_vertexBufferMemory  = VK_NULL_HANDLE;
    VkBuffer                 m_indexBuffer         = VK_NULL_HANDLE;
    VkDeviceMemory           m_indexBufferMemory   = VK_NULL_HANDLE;
    struct ChunkRenderData {
        VkBuffer       vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
        VkBuffer       indexBuffer  = VK_NULL_HANDLE;
        VkDeviceMemory indexMemory  = VK_NULL_HANDLE;
        uint32_t       indexCount   = 0;
        VkBuffer       objInstBuffer = VK_NULL_HANDLE;  // per-chunk tree instances
        VkDeviceMemory objInstMemory = VK_NULL_HANDLE;
        uint32_t       objInstCount  = 0;
    };
    std::unordered_map<glm::ivec2, ChunkRenderData, IVec2Hash> m_chunkBuffers;
    Frustum                  m_frustum;

    // Shared low-poly tree mesh (instanced per Object)
    VkBuffer                 m_treeVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory           m_treeVertexMemory = VK_NULL_HANDLE;
    uint32_t                 m_treeVertexCount  = 0;

    // UI / hotbar — one buffer per frame in flight (avoids overwrite while GPU still reads)
    std::vector<VkBuffer>       m_uiBuffer;
    std::vector<VkDeviceMemory> m_uiMemory;
    std::vector<void*>          m_uiMapped;
    uint32_t                 m_uiVertexCount   = 0;
    int                      m_hotbarSelected  = 0;
    std::array<ItemType, HOTBAR_SLOTS> m_hotbarPalette{};
    bool                     m_inventoryOpen   = false;
    std::array<float, 4>     m_skyColor        = {0.08f, 0.08f, 0.12f, 1.0f};
    std::vector<VkBuffer>       m_playerInstBuffer;
    std::vector<VkDeviceMemory> m_playerInstMemory;
    std::vector<void*>          m_playerInstMapped;
    VkBuffer                 m_selectorVertexBuffer  = VK_NULL_HANDLE;
    VkDeviceMemory           m_selectorVertexMemory  = VK_NULL_HANDLE;
    VkBuffer                 m_selectorIndexBuffer   = VK_NULL_HANDLE;
    VkDeviceMemory           m_selectorIndexMemory   = VK_NULL_HANDLE;
    std::vector<VkBuffer>       m_selectorInstBuffer;
    std::vector<VkDeviceMemory> m_selectorInstMemory;
    std::vector<void*>          m_selectorInstMapped;
    bool                     m_showSelector          = false;

    VkImage                      m_depthImage           = VK_NULL_HANDLE;
    VkDeviceMemory               m_depthImageMemory     = VK_NULL_HANDLE;
    VkImageView                  m_depthImageView       = VK_NULL_HANDLE;

    static constexpr uint32_t    SHADOW_MAP_SIZE        = 2048;
    VkImage                      m_shadowImage          = VK_NULL_HANDLE;
    VkDeviceMemory               m_shadowImageMemory    = VK_NULL_HANDLE;
    VkImageView                  m_shadowImageView      = VK_NULL_HANDLE;
    VkRenderPass                 m_shadowRenderPass     = VK_NULL_HANDLE;
    VkFramebuffer                m_shadowFramebuffer    = VK_NULL_HANDLE;
    VkPipelineLayout             m_shadowPipelineLayout = VK_NULL_HANDLE;
    VkPipeline                   m_shadowPipeline       = VK_NULL_HANDLE;
    VkSampler                    m_shadowSampler        = VK_NULL_HANDLE;
    glm::mat4                    m_lightMVP             = glm::mat4(1.0f);
    glm::vec3                    m_sunDir               = glm::vec3(0.0f, 0.0f, 1.0f);
    float                        m_dayFactor            = 0.0f;

    VkDescriptorSetLayout        m_descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkBuffer>        m_uniformBuffers;
    std::vector<VkDeviceMemory>  m_uniformBuffersMemory;
    std::vector<void*>           m_uniformBuffersMapped;
    VkDescriptorPool             m_descriptorPool   = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;

    VkCommandPool            m_commandPool      = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> m_imageAvailable;   // per frame in flight
    std::vector<VkSemaphore> m_renderFinished;   // per swapchain image (present wait)
    std::vector<VkFence>     m_inFlight;          // per frame in flight
    std::vector<VkFence>     m_imagesInFlight;    // per swapchain image; non-owning fence refs
    uint32_t                 m_currentFrame     = 0;

    struct DeferredDelete {
        VkBuffer       buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        uint64_t       frame  = 0;
    };
    std::vector<DeferredDelete> m_deletionQueue;
    uint64_t                    m_frameCount = 0;
};
