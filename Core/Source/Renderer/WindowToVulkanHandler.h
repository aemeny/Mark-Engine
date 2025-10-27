#pragma once
#include "MarkVulkanCommandBuffers.h"

#include <volk.h>
#include <memory>
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

    private:
        void destroyFrameSyncObjects(std::shared_ptr<VulkanCore> _VkCoreRef);

        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        Platform::Window& m_windowRef;

        VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
        VulkanSwapChain m_swapChain{ m_vulkanCoreRef, m_surface };
        VulkanRenderPass m_renderPass{ m_vulkanCoreRef, m_swapChain, m_windowRef };
        VulkanCommandBuffers m_commandBuffers{ m_vulkanCoreRef, m_swapChain, m_renderPass };

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