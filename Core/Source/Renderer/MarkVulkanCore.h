#pragma once
#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>

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

        VkInstance m_instance{ VK_NULL_HANDLE };
    };
} // namespace Mark::RendererVK