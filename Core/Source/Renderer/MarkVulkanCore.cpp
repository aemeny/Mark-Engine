#include <Mark/Engine.h>
#include "MarkVulkanCore.h"
#include "Platform/WindowManager.h"
#include "Utils/VulkanUtils.h"
#include "Utils/MarkUtils.h"

#define VOLK_IMPLEMENTATION
#include <volk.h>
#include <GLFW/glfw3.h>

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
        m_physicalDevices.initialize(m_instance);


        // --- Decide asset root once ---
#ifdef MARK_ASSETS_DIR
        m_assetRoot = std::filesystem::path(MARK_ASSETS_DIR); // set via CMake
#else
        // Fallback: tries "<cwd>/App/Assets"
        auto cwd = std::filesystem::current_path();
        if (std::filesystem::exists(cwd / "App" / "Assets"))
            m_assetRoot = cwd / "App" / "Assets";
        else
            MARK_ERROR("MARK_ASSETS_DIR not defined and could not find default asset path");
#endif
        MARK_INFO_C(Utils::Category::System, "Asset root: %s", m_assetRoot.string().c_str());
    }

    std::filesystem::path VulkanCore::shaderPath(const std::string& _file) const
    {
        std::filesystem::path path = (m_assetRoot / "Shaders" / _file).lexically_normal();
        if (!std::filesystem::exists(path)) {
            MARK_WARN_C(Utils::Category::Shader, "Shader not found at: %s", path.string().c_str());
        }
        return path;
    }

    VulkanCore::~VulkanCore()
    {
        if (m_device != VK_NULL_HANDLE)
        {
            if (m_renderPassCache)
            {
                m_renderPassCache->destroyAll();
                m_renderPassCache.reset();
            }
            if (m_shaderCache)
            {
                m_shaderCache->destroy();
                m_shaderCache.reset();
            }
            if (m_graphicsPipelineCache)
            {
                m_graphicsPipelineCache->destroyAll();
                m_graphicsPipelineCache.reset();
            }

            m_presentQueue.destroy();
            m_graphicsQueue.destroy();

            vkDeviceWaitIdle(m_device);
            vkDestroyDevice(m_device, nullptr);
            m_device = VK_NULL_HANDLE;
            MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Logical Device Destroyed");
        }

        if (m_debugMessenger != VK_NULL_HANDLE)
        {
            vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
            m_debugMessenger = VK_NULL_HANDLE;
            MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Debug Callback Destroyed");
        }

        if (m_instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
            MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Instance Destroyed");
        }
    }

    void VulkanCore::selectDevicesForSurface(VkSurfaceKHR _surface)
    {
        if (m_device == VK_NULL_HANDLE)
        {
            // First window: pick families and create the device
            m_selectedDeviceResult = m_physicalDevices.selectDeviceForSurface(VK_QUEUE_GRAPHICS_BIT, _surface);
            createLogicalDevice();
            return;
        }

        // Subsequent windows: verify existing families can present to new surface
        const VulkanPhysicalDevices::DeviceProperties& selectedDeviceProps = m_physicalDevices.selected();
        const VulkanPhysicalDevices::SurfaceProperties* cachedSurfaceProps = nullptr;
        for (const auto& surfaceLinked : selectedDeviceProps.m_surfacesLinked)
        {
            if (surfaceLinked.m_surface == _surface)
            { 
                cachedSurfaceProps = &surfaceLinked;
                break; 
            }
        }
        if (!cachedSurfaceProps)
        {
            MARK_ERROR("Surface properties not cached for new window surface");
        }

        const uint32_t presentIdx = m_selectedDeviceResult.m_presentQueueFamilyIndex;
        if (presentIdx >= cachedSurfaceProps->m_qSupportsPresent.size() || !cachedSurfaceProps->m_qSupportsPresent[presentIdx])
        {
            MARK_ERROR("Existing present queue family %u cannot present to the new surface", presentIdx);
        }
    }

    void VulkanCore::createInstance(const EngineAppInfo& _appInfo)
    {
        // Loader for global function pointers
        VkResult res = volkInitialize();
        CHECK_VK_RESULT(res, "Initialize volk");

        // Create Vulkan Layer and Extension lists
        std::vector<const char*> layers = {};

        uint32_t extCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extCount);
        if (!glfwExtensions || extCount == 0)
        {
            MARK_ERROR("GLFW did not report required Vulkan instance extensions");
        }
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + extCount);

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
            .apiVersion = VK_API_VERSION_1_3
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
        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Instance Created");
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT _severity,
        VkDebugUtilsMessageTypeFlagsEXT _type,
        const VkDebugUtilsMessengerCallbackDataEXT* _pCallbackData,
        void* _pUserData)
    {
        const auto severityLevel = VkSeverityToLevel(_severity);

        MARK_SCOPE_C_L(Utils::Category::Vulkan, severityLevel, "Vulkan Debug Callback:");
        MARK_IN_SCOPE("%s", _pCallbackData->pMessage);
        MARK_IN_SCOPE(MARK_COL_LABEL "Severity: " MARK_COL_RESET "%s", GetDebugSeverityStr(_severity));
        const std::string typeStr = GetDebugType(_type);
        MARK_IN_SCOPE(MARK_COL_LABEL "Type: " MARK_COL_RESET "%s", typeStr.c_str());

        if (_pCallbackData->objectCount == 0) {
            MARK_IN_SCOPE(MARK_COL_LABEL "Objects: " MARK_COL_RESET "<none>");
        }
        else {
            MARK_IN_SCOPE(MARK_COL_LABEL "Objects: " MARK_COL_RESET);
            for (uint32_t i = 0; i < _pCallbackData->objectCount; i++)
            {
                const auto& object = _pCallbackData->pObjects[i];
                const char* typeStr = VkObjectTypeToStr(object.objectType);
                const char* nameStr = (object.pObjectName && *object.pObjectName) ? object.pObjectName : "<unnamed>";
                MARK_IN_SCOPE("    [%s] %s (0x%" PRIx64 ")", typeStr, nameStr, object.objectHandle);
            }
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
                               //VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = &DebugCallback,
            .pUserData = nullptr
        };

        VkResult res = vkCreateDebugUtilsMessengerEXT(m_instance, &messengerCreateInfo, nullptr, &m_debugMessenger);
        CHECK_VK_RESULT(res, "Create Debug Utils Messenger");

        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Debug Callback Created");
    }

    void VulkanCore::createLogicalDevice()
    {
        // Information about the queue to create
        float queuePriorities = 1.0f; // // Priority must be between 0.0 and 1.0, higher is more important
        VkDeviceQueueCreateInfo qInfos[2] {};
        uint32_t qCount = 0;

        qInfos[qCount++] = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = m_selectedDeviceResult.m_gtxQueueFamilyIndex,
            .queueCount = 1, // Controlls parallelism of the GPU, currently only one queue used
            .pQueuePriorities = &queuePriorities
        };
        if (m_selectedDeviceResult.m_presentQueueFamilyIndex != m_selectedDeviceResult.m_gtxQueueFamilyIndex)
        {
            qInfos[qCount++] = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueFamilyIndex = m_selectedDeviceResult.m_presentQueueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &queuePriorities
            };
        }

        // Any device extensions required by Vulkan, enabled here
        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        // VK_KHR_shader_draw_parameters is needed for support in Vulkan under 1.1
        uint32_t api = m_physicalDevices.selected().m_properties.apiVersion;
        if (VK_API_VERSION_MAJOR(api) == 1 && VK_API_VERSION_MINOR(api) == 0) {
            deviceExtensions.push_back(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);
        }

        // Ensure the selected device supports nessesary extensions
        auto supportsExt = [&](const char* _name)
            {
                uint32_t n = 0;
                vkEnumerateDeviceExtensionProperties(m_physicalDevices.selected().m_device, nullptr, &n, nullptr);
                std::vector<VkExtensionProperties> exts(n);
                vkEnumerateDeviceExtensionProperties(m_physicalDevices.selected().m_device, nullptr, &n, exts.data());
                for (const auto& e : exts) if (std::strcmp(e.extensionName, _name) == 0) return true;
                return false;
            };
        for (const char* ext : deviceExtensions)
        {
            if (!supportsExt(ext))
            {
                MARK_ERROR("Device does not support extension %s", ext);
            }
        }

        // Ensure the selected device supports nessesary features
        const VkPhysicalDeviceFeatures& feats = m_physicalDevices.selected().m_features;
        REQ_FEATURE(feats, geometryShader);
        REQ_FEATURE(feats, tessellationShader);

        // Any device features required by Vulkan, enabled here
        VkPhysicalDeviceFeatures deviceFeatures = { 0 };
        deviceFeatures.geometryShader = VK_TRUE;
        deviceFeatures.tessellationShader = VK_TRUE;

        VkDeviceCreateInfo deviceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueCreateInfoCount = qCount,
            .pQueueCreateInfos = qInfos,
            .enabledLayerCount = 0,         // DEPRECATED
            .ppEnabledLayerNames = nullptr, // DEPRECATED
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = &deviceFeatures
        };

        VkResult res = vkCreateDevice(m_physicalDevices.selected().m_device, &deviceCreateInfo, nullptr, &m_device);
        CHECK_VK_RESULT(res, "Create Logical Device");

        // Load device for volk to be able to call device functions
        volkLoadDevice(m_device);

        MARK_INFO_C(Utils::Category::Vulkan, "Logical Device Created");


        // -- After device creation, we can initialize queues and caches --
        initializeQueue();

        m_renderPassCache = std::make_unique<VulkanRenderPassCache>(m_device);
        m_shaderCache = std::make_unique<VulkanShaderCache>(m_device);
        m_graphicsPipelineCache = std::make_unique<VulkanGraphicsPipelineCache>(m_device);
    }

    void VulkanCore::initializeQueue()
    {
        m_graphicsQueue.initialize(m_device, m_selectedDeviceResult.m_gtxQueueFamilyIndex, 0);
        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Graphics Queue Initialized");

        if (m_selectedDeviceResult.m_presentQueueFamilyIndex != m_selectedDeviceResult.m_gtxQueueFamilyIndex) 
        {
            m_presentQueue.initialize(m_device, m_selectedDeviceResult.m_presentQueueFamilyIndex, 0);
            MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Present Queue Initialized");
        }
        else 
        {
            MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Present Queue uses Graphics Queue");
        }
    }

} // namespace Mark::RendererVK