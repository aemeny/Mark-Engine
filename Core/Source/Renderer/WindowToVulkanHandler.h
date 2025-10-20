#pragma once
#include "MarkVulkanCommandBuffers.h"

#include <memory>

struct GLFWwindow;
union VkClearColorValue;

namespace Mark::RendererVK
{
    struct WindowToVulkanHandler 
    {
        WindowToVulkanHandler(std::weak_ptr<RendererVK::VulkanCore> _vulkanCoreRef, GLFWwindow* _window, VkClearColorValue _clearColour);
        ~WindowToVulkanHandler();
        WindowToVulkanHandler(const WindowToVulkanHandler&) = delete;
        WindowToVulkanHandler& operator=(const WindowToVulkanHandler&) = delete;

        void createSurface();
        VkSurfaceKHR surface() const { return m_surface; }

    private:
        std::weak_ptr<RendererVK::VulkanCore> m_vulkanCoreRef;
        GLFWwindow* m_window;

        VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
        VulkanSwapChain m_swapChain{ m_vulkanCoreRef, m_surface };
        VulkanCommandBuffers m_commandBuffers{ m_vulkanCoreRef, m_swapChain };
    };
} // namespace Mark::RendererVK