#include <Mark/Engine.h>
#include "MarkVulkanCore.h"
#include "MarkVulkanUtil.h"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#include <vector>
#include <inttypes.h>

namespace Mark::RendererVK
{
    VulkanCore::VulkanCore(const EngineAppInfo& _appInfo)
    {
        createInstance(_appInfo);
        if (_appInfo.enableVulkanValidation)
        {
            createDebugCallback();
        }
        //m_physicalDevices.initialize(m_instance, VK_NULL_HANDLE);
        //m_queueFamilyIndex = m_physicalDevices.selectDevice(VK_QUEUE_GRAPHICS_BIT, true);
    }

    VulkanCore::~VulkanCore()
    {
        if (m_debugMessenger != VK_NULL_HANDLE)
        {
            vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
            printf("Vulkan Debug Callback Destroyed\n");
        }

        if (m_instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
            printf("Vulkan Instance Destroyed\n");
        }
    }

    void VulkanCore::createInstance(const EngineAppInfo& _appInfo)
    {
        // Loader for global function pointers
        VkResult res = volkInitialize();
        CHECK_VK_RESULT(res, "Initialize volk");

        // Create Vulkan Layer and Extension lists
        std::vector<const char*> layers = {};

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
        };

        if (_appInfo.enableVulkanValidation)
        {
            layers.push_back("VK_LAYER_KHRONOS_validation"); // Vk validation layer enabled for debug purposes
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

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

        res = vkCreateInstance(&createInfo, nullptr, &m_instance);
        CHECK_VK_RESULT(res, "Create Vk Instance");
        volkLoadInstance(m_instance);
        printf("Vulkan Instance Created\n");
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT _severity,
        VkDebugUtilsMessageTypeFlagsEXT _type,
        const VkDebugUtilsMessengerCallbackDataEXT* _pCallbackData,
        void* _pUserData)
    {
        printf("Vulkan Debug Callback: %s\n", _pCallbackData->pMessage);
        printf("    Severity: %s\n", GetDebugSeverityStr(_severity));
        printf("    Type: %s\n", GetDebugType(_type));
        printf("    Objects ");

        for (uint32_t i = 0; i < _pCallbackData->objectCount; i++)
        {
            printf("%" PRIx64 " ", _pCallbackData->pObjects[i].objectHandle);
        }

        return VK_FALSE; // VK_FALSE indicates that the Vulkan call that triggered the validation layer message should not be aborted
    }

    void VulkanCore::createDebugCallback()
    {
        VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = nullptr,
            .flags = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = &DebugCallback,
            .pUserData = nullptr
        };

        PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger = 
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
        if (!vkCreateDebugUtilsMessenger) {
            
            MARK_ERROR("Cannot find address of vkCreateDebugUtilsMessenger\n");
        }

        VkResult res = vkCreateDebugUtilsMessenger(m_instance, &messengerCreateInfo, nullptr, &m_debugMessenger);
        CHECK_VK_RESULT(res, "Create Debug Utils Messenger");

        printf("Vulkan Debug Callback Created\n");
    }

} // namespace Mark::RendererVK