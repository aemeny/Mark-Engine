#pragma once
#include "MarkVulkanSwapChain.h"

#include <memory>

struct GLFWwindow;

namespace Mark::RendererVK
{
    struct WindowToVulkanHandler 
    {
        WindowToVulkanHandler(std::weak_ptr<RendererVK::VulkanCore> _vulkanCoreRef, GLFWwindow* _window);
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
    };
} // namespace Mark::RendererVK