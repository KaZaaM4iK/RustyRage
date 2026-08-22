#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <string>

namespace Core {

class Model;

/**
 * Vulkan renderer with SDL3 integration
 * Handles 3D rendering, shaders, and graphics pipeline
 */
class Renderer {
public:
    Renderer(SDL_Window* window);
    ~Renderer();

    // Initialization and cleanup
    bool initialize();
    void shutdown();
    
    // Rendering
    void beginFrame();
    void endFrame();
    void render();
    void renderModel(const Model& model, const glm::mat4& transform);
    
    // Camera
    void setViewMatrix(const glm::mat4& view);
    void setProjectionMatrix(const glm::mat4& projection);
    
    // Getters
    VkDevice getDevice() const { return m_device; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkCommandPool getCommandPool() const { return m_commandPool; }
    
    // Window resize
    void onWindowResize(int width, int height);

private:
    SDL_Window* m_window;
    
    // Vulkan instance and devices
    VkInstance m_instance;
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    
    // Swapchain
    VkSwapchainKHR m_swapchain;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkFormat m_swapchainFormat;
    VkExtent2D m_swapchainExtent;
    
    // Command buffers
    VkCommandPool m_commandPool;
    std::vector<VkCommandBuffer> m_commandBuffers;
    
    // Synchronization
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame;
    
    // Render pass and pipeline
    VkRenderPass m_renderPass;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;
    
    // Framebuffers
    std::vector<VkFramebuffer> m_swapchainFramebuffers;
    
    // Matrices
    glm::mat4 m_viewMatrix;
    glm::mat4 m_projectionMatrix;
    
    // Vulkan setup methods
    bool createInstance();
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapchain();
    bool createImageViews();
    bool createRenderPass();
    bool createGraphicsPipeline();
    bool createFramebuffers();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();
    
    // Helper methods
    VkSurfaceKHR createSurface();
    VkShaderModule createShaderModule(const std::vector<char>& code);
    bool isDeviceSuitable(VkPhysicalDevice device);
    std::vector<const char*> getRequiredExtensions();
    bool checkValidationLayerSupport();
    void cleanupSwapchain();
    void recreateSwapchain();
    
    // Validation layers (debug only)
    #ifdef DEBUG
    VkDebugUtilsMessengerEXT m_debugMessenger;
    bool setupDebugMessenger();
    #endif
};

} // namespace Core
