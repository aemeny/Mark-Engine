#include <Mark/Engine.h>
#include "MarkVulkanCore.h"
#include "MarkVulkanUtil.h"

#include <vector>

namespace Mark::RendererVK
{
    VulkanCore::VulkanCore(const EngineAppInfo& _appInfo)
    {
        createInstance(_appInfo);
    }

    VulkanCore::~VulkanCore()
    {
        if (m_instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
            printf("Vulkan Instance Destroyed\n");
        }
    }

    void VulkanCore::createInstance(const EngineAppInfo& _appInfo)
    {
        std::vector<const char*> layers = {};
        if (_appInfo.enableVulkanValidation)
        {
            layers.push_back("VK_LAYER_KHRONOS_validation"); // Vk validation layer enbled for debug purposes
        }

        std::vector<const char*> extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(_WIN32)
            "VK_KHR_win32_surface",
#endif
#if defined(__APPLE__)
            "VK_MVK_macos_surface",
#endif
#if defined(__linux__)
            "VK_KHR_xcb_surface",
#endif
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            VK_EXT_DEBUG_REPORT_EXTENSION_NAME
        };

        VkApplicationInfo appInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = _appInfo.appName,
            .applicationVersion = VK_MAKE_API_VERSION(_appInfo.appVersion[0], _appInfo.appVersion[1], _appInfo.appVersion[2], _appInfo.appVersion[3]),
            .pEngineName = "Mark",
            .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .apiVersion = VK_API_VERSION_1_4
        };

        VkInstanceCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(layers.size()),
            .ppEnabledLayerNames = layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data()
        };

        VkResult res = vkCreateInstance(&createInfo, nullptr, &m_instance);
        CHECK_VK_RESULT(res, "Create Vk Instance");
        printf("Vulkan Instance Created\n");
    }
} // namespace Mark::RendererVK