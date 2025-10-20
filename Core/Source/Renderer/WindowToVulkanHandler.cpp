#pragma once
#include "WindowToVulkanHandler.h"
#include "MarkVulkanCore.h"
#include "Utils/ErrorHandling.h"
#include "Utils/VulkanUtils.h"

#include <GLFW/glfw3.h>
#include <volk.h>

namespace Mark::RendererVK
{
    WindowToVulkanHandler::WindowToVulkanHandler(std::weak_ptr<RendererVK::VulkanCore> _vulkanCoreRef, GLFWwindow* _windowRef, VkClearColorValue _clearColour) :
        m_vulkanCoreRef(_vulkanCoreRef), m_window(_windowRef)
    {
        createSurface();

        m_vulkanCoreRef.lock()->physicalDevices().querySurfaceProperties(m_surface);
        m_vulkanCoreRef.lock()->selectDevicesForSurface(m_surface);

        m_swapChain.createSwapChain();

        m_commandBuffers.createCommandPool();
        m_commandBuffers.createCommandBuffers();
        m_commandBuffers.recordCommandBuffers(_clearColour);
    }

    WindowToVulkanHandler::~WindowToVulkanHandler()
    {
        // Explicitly destroy swap chain before surface
        m_swapChain.destroySwapChain();

        if (m_surface != VK_NULL_HANDLE)
        {
            if (m_vulkanCoreRef.expired())
            {
                MARK_ERROR("VulkanCore reference expired, cannot destroy surface");
            }
            vkDestroySurfaceKHR(m_vulkanCoreRef.lock()->instance(), m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
            printf("GLFW Window Surface Destroyed\n");
        }
    }

    void WindowToVulkanHandler::createSurface()
    {
        if (m_surface != VK_NULL_HANDLE) return;
        if (m_vulkanCoreRef.expired())
        {
            MARK_ERROR("VulkanCore reference expired, cannot create surface");
        }

        VkResult res = glfwCreateWindowSurface(m_vulkanCoreRef.lock()->instance(), m_window, nullptr, &m_surface);

        CHECK_VK_RESULT(res, "Create window surface");
        printf("GLFW Window Surface Created\n");
    }
} // namespace Mark::RendererVK