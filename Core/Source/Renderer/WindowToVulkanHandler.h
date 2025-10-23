#pragma once
#include "MarkVulkanCommandBuffers.h"

#include <volk.h>
#include <memory>
#include <array>

struct GLFWwindow;

namespace Mark::RendererVK
{
    struct WindowToVulkanHandler 
    {
        WindowToVulkanHandler(std::weak_ptr<RendererVK::VulkanCore> _vulkanCoreRef, GLFWwindow* _window, VkClearColorValue _clearColour);
        ~WindowToVulkanHandler();
        WindowToVulkanHandler(const WindowToVulkanHandler&) = delete;
        WindowToVulkanHandler& operator=(const WindowToVulkanHandler&) = delete;

        void renderToWindow();

        void createSurface();
        VkSurfaceKHR surface() const { return m_surface; }

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        GLFWwindow* m_window;

        VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
        VulkanSwapChain m_swapChain{ m_vulkanCoreRef, m_surface };
        VulkanCommandBuffers m_commandBuffers{ m_vulkanCoreRef, m_swapChain };

        struct FrameSyncData
        {
            VkSemaphore m_imageAvailableSem{ VK_NULL_HANDLE };
            VkSemaphore m_renderFinishedSem{ VK_NULL_HANDLE };
            VkFence m_inFlightFence{ VK_NULL_HANDLE };
        };
        std::array<FrameSyncData, 3> m_framesInFlight; // frames-in-flight
        uint32_t m_frame = 0;
    };
} // namespace Mark::RendererVK