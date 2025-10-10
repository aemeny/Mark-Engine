#pragma once
#include <volk.h>

namespace Mark::RendererVK
{
    struct VulkanCore
    {
        VulkanCore(const EngineAppInfo& _appInfo);
        ~VulkanCore();
        VulkanCore(const VulkanCore&) = delete;
        VulkanCore& operator=(const VulkanCore&) = delete;

    private:
        void createInstance(const EngineAppInfo& _appInfo);
        void createDebugCallback();

        VkInstance m_instance{ VK_NULL_HANDLE };
        VkDebugUtilsMessengerEXT m_debugMessenger{ VK_NULL_HANDLE };
    };
} // namespace Mark::RendererVK