#pragma once
#include "MarkVulkanCommandBuffers.h"

#include "Engine/EarlyCameraController.h" // TEMP

#include <array>

struct GLFWwindow;
namespace Mark::Platform { struct Window; }
namespace Mark::RendererVK
{
    struct WindowToVulkanHandler 
    {
        WindowToVulkanHandler(std::weak_ptr<VulkanCore> _vulkanCoreRef, Platform::Window& _windowRef, VkClearColorValue _clearColour);
        ~WindowToVulkanHandler();
        WindowToVulkanHandler(const WindowToVulkanHandler&) = delete;
        WindowToVulkanHandler& operator=(const WindowToVulkanHandler&) = delete;

        void renderToWindow();

        void createSurface();
        VkSurfaceKHR surface() const { return m_surface; }

        // TEMP FOR TESTING
        std::weak_ptr<MeshHandler> addMesh(const char* _meshPath);
        void initCameraController();

    private:
        void destroyFrameSyncObjects(std::shared_ptr<VulkanCore> _VkCoreRef);

        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        Platform::Window& m_windowRef;
        VkClearColorValue m_clearColour{};

        // TEMP list of meshes to render for this window
        std::vector<std::shared_ptr<MeshHandler>> m_meshesToDraw;
        // TEMP camera controller for testing
        std::shared_ptr<Systems::EarlyCameraController> m_cameraController;

        VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
        VulkanSwapChain m_swapChain{ m_vulkanCoreRef, m_surface };
        VulkanUniformBuffer m_uniformBuffer{ m_vulkanCoreRef };
        VulkanGraphicsPipeline m_graphicsPipeline{ m_vulkanCoreRef, m_swapChain, m_uniformBuffer, &m_meshesToDraw };
        VulkanCommandBuffers m_commandBuffers{ m_vulkanCoreRef, m_swapChain, m_graphicsPipeline };

        struct FrameSyncData
        {
            VkSemaphore m_imageAvailableSem{ VK_NULL_HANDLE };
            VkFence m_inFlightFence{ VK_NULL_HANDLE };
        };
        std::array<FrameSyncData, 3> m_framesInFlight; // frames-in-flight
        uint32_t m_frame = 0;

        // One present semaphore per swapchain image
        std::vector<VkSemaphore> m_presentSems;
        // Tracks which fence is currently each image
        std::vector<VkFence> m_imagesInFlight;
    };
} // namespace Mark::RendererVK