#pragma once
#include "MarkVulkanPhysicalDevices.h"

#include <volk.h>

namespace Mark { struct EngineAppInfo; }
namespace Mark::Platform { struct WindowManager; }

namespace Mark::RendererVK
{
    struct VulkanCore
    {
        VulkanCore(const EngineAppInfo& _appInfo);
        ~VulkanCore();
        VulkanCore(const VulkanCore&) = delete;
        VulkanCore& operator=(const VulkanCore&) = delete;

        void selectDevices();
        const VkInstance& instance() const { return m_instance; }

    private:
        friend struct ::Mark::Platform::WindowManager;

        void createInstance(const EngineAppInfo& _appInfo);
        void createDebugCallback();
        void createLogicalDevice();

        VkInstance m_instance{ VK_NULL_HANDLE };
        VkDebugUtilsMessengerEXT m_debugMessenger{ VK_NULL_HANDLE };

        VulkanPhysicalDevices m_physicalDevices;
        uint32_t m_queueFamilyIndex{ 0 };
        VkDevice m_device{ VK_NULL_HANDLE };
    };
} // namespace Mark::RendererVK