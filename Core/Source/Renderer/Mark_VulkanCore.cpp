#include <Mark/Engine.h>
#include "Mark_VulkanCore.h"
#include "Mark_VulkanVertexBuffer.h"
#include "Mark_WindowToVulkanHandler.h"

#include "Core.h"

#include "Platform/WindowManager.h"

#include "Utils/VulkanUtils.h"
#include "Utils/Mark_Utils.h"

#define VOLK_IMPLEMENTATION
#include <volk.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <inttypes.h>

namespace Mark::RendererVK
{
    VulkanCore::VulkanCore(const EngineAppInfo& _appInfo, Core& _core) : 
        m_appInfo(_appInfo), m_core(_core)
    {
        createInstance();
        if (m_appInfo.enableVulkanValidation)
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

    Platform::ImGuiHandler& VulkanCore::imguiHandler()
    {
        return m_core.imguiHandler();
    }

    std::filesystem::path VulkanCore::assetPath(const std::string& _file) const
    {
        std::filesystem::path path = (m_assetRoot / _file).lexically_normal();
        if (!std::filesystem::exists(path)) {
            MARK_LOG_ERROR_C(Utils::Category::System, "Asset not found at: %s", Utils::ShortPathForLog(path.string()).c_str());
        }
        return path;
    }

    VulkanCore::~VulkanCore()
    {
        if (m_device != VK_NULL_HANDLE)
        {
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
            if (m_vertexUploader) 
            {
                m_vertexUploader->destroy();
                m_vertexUploader.reset();
            }

            m_presentQueue.destroy();
            m_graphicsQueue.destroy();

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

            // After device creation, we can initialize queues, caches and the vertex buffer
            initializeQueue();
            createCaches();

            // Device wide vertex uploader (shared by all windows)
            m_vertexUploader = std::make_unique<VulkanVertexBuffer>(m_device, graphicsQueueFamilyIndex(), m_graphicsQueue);

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

    uint32_t VulkanCore::getMemoryTypeIndex(uint32_t _memoryTypeBits, VkMemoryPropertyFlags _propertyFlags) const
    {
        const VkPhysicalDeviceMemoryProperties& memProps = m_physicalDevices.selected().m_memoryProperties;

        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
        {
            const VkMemoryType& memType = memProps.memoryTypes[i];
            uint32_t currentBitMask = (1 << i);
            bool isCurrMemTypeSupported = (_memoryTypeBits & currentBitMask);
            bool hasRequiredProperties = (memType.propertyFlags & _propertyFlags) == _propertyFlags;

            if (isCurrMemTypeSupported && hasRequiredProperties) {
                return i;
            }
        }
        MARK_ERROR("Failed to find memory type for %x requested memory properties %x", _memoryTypeBits, _propertyFlags);
        return -1;
    }

    uint32_t VulkanCore::getInstanceVersion() const
    {
        return VK_MAKE_API_VERSION(0, m_instanceVersion.major, m_instanceVersion.minor, m_instanceVersion.patch);
    }

    void VulkanCore::createInstance()
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

        if (m_appInfo.enableVulkanValidation)
        {
            layers.push_back("VK_LAYER_KHRONOS_validation"); // Vk validation layer enabled for debug purposes
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        createInstanceVersion();
        VkApplicationInfo appInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = m_appInfo.appName,
            .applicationVersion = VK_MAKE_API_VERSION(m_appInfo.appVersion[0], m_appInfo.appVersion[1], m_appInfo.appVersion[2], m_appInfo.appVersion[3]),
            .pEngineName = "Mark",
            .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .apiVersion = VK_MAKE_API_VERSION(0, m_instanceVersion.major, m_instanceVersion.minor, 0)
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

    void VulkanCore::createInstanceVersion()
    {
        uint32_t instanceVersion = 0;

        VkResult res = vkEnumerateInstanceVersion(&instanceVersion);
        CHECK_VK_RESULT(res, "Enumerate Instance Version");

        m_instanceVersion.major = VK_API_VERSION_MAJOR(instanceVersion);
        m_instanceVersion.minor = VK_API_VERSION_MINOR(instanceVersion);
        m_instanceVersion.patch = VK_API_VERSION_PATCH(instanceVersion);

        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Instance Version: %u.%u.%u",
            m_instanceVersion.major, m_instanceVersion.minor, m_instanceVersion.patch);
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT _severity,
        VkDebugUtilsMessageTypeFlagsEXT _type,
        const VkDebugUtilsMessengerCallbackDataEXT* _pCallbackData,
        void* _pUserData)
    {
        const auto severityLevel = VkSeverityToLevel(_severity);
        const auto category = Utils::Category::Vulkan;

        MARK_SCOPE_C_L(category, severityLevel, "Vulkan Debug Callback:");
        MARK_IN_SCOPE(category, severityLevel, "%s", _pCallbackData->pMessage);
        MARK_IN_SCOPE(category, severityLevel, MARK_COL_LABEL "Severity: " MARK_COL_RESET "%s", GetDebugSeverityStr(_severity));
        const std::string typeStr = GetDebugType(_type);
        MARK_IN_SCOPE(category, severityLevel, MARK_COL_LABEL "Type: " MARK_COL_RESET "%s", typeStr.c_str());

        if (_pCallbackData->objectCount == 0) {
            MARK_IN_SCOPE(category, severityLevel, MARK_COL_LABEL "Objects: " MARK_COL_RESET "<none>");
        }
        else {
            MARK_IN_SCOPE(category, severityLevel, MARK_COL_LABEL "Objects: " MARK_COL_RESET);
            for (uint32_t i = 0; i < _pCallbackData->objectCount; i++)
            {
                const auto& object = _pCallbackData->pObjects[i];
                const char* typeStr = VkObjectTypeToStr(object.objectType);
                const char* nameStr = (object.pObjectName && *object.pObjectName) ? object.pObjectName : "<unnamed>";
                MARK_IN_SCOPE(category, severityLevel, "    [%s] %s (0x%" PRIx64 ")", typeStr, nameStr, object.objectHandle);
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

        const VulkanPhysicalDevices::DeviceProperties& selectedPhysical = m_physicalDevices.selected();

        // Require Vulkan 1.3 for this engine
        uint32_t devApi = selectedPhysical.m_properties.apiVersion;
        uint32_t devMajor = VK_API_VERSION_MAJOR(devApi);
        uint32_t devMinor = VK_API_VERSION_MINOR(devApi);
        bool deviceIsVulkan13OrHigher = (devMajor > 1) || (devMajor == 1 && devMinor >= 3);

        if (!deviceIsVulkan13OrHigher) {
            MARK_LOG_ERROR_C(Utils::Category::Vulkan,
                "Vulkan Version 1.3 or higher is required for Mark. Current: Vulkan %u.%u", devMajor, devMinor);
        }

        // Any device extensions required by Vulkan are enabled here
        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        // Required for imgui to always be active
        if (selectedPhysical.isExtensionSupported(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)) {
            deviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME); 
        }
        else {
            MARK_ERROR("Selected GPU does not support dynamic rendering, which is required for Mark");
        }

        // --- Decide caps ---
        const VkPhysicalDeviceLimits& limits = selectedPhysical.m_properties.limits;

        // Meshes: each mesh requires 2 storage buffer descriptors (vertex + index)
        const uint32_t maxMeshesFromSet = limits.maxDescriptorSetStorageBuffers / 2u;
        const uint32_t maxMeshesFromStage = limits.maxPerStageDescriptorStorageBuffers / 2u;
        const uint32_t maxMeshesHard = std::min(maxMeshesFromSet, maxMeshesFromStage);
        const uint32_t requestedMaxMeshes = 4096u;
        m_bindlessCaps.maxMeshes = std::min(requestedMaxMeshes, maxMeshesHard);
        if (m_bindlessCaps.maxMeshes == 0) {
            MARK_ERROR("Bindless mesh cap resolved to 0 (storage buffer descriptor limits too small)");
        }

        // Textures: combined image sampler counts against both sampler + sampled-image limits.
        const uint32_t maxCombinedSet = std::min(limits.maxDescriptorSetSamplers, limits.maxDescriptorSetSampledImages);
        const uint32_t maxCombinedStage = std::min(limits.maxPerStageDescriptorSamplers, limits.maxPerStageDescriptorSampledImages);
        const uint32_t maxTexturesHard = std::min(maxCombinedSet, maxCombinedStage);
        const uint32_t requestedMaxTextures = m_bindlessCaps.maxMeshes * m_bindlessCaps.numAttachableTextures;
        m_bindlessCaps.maxTextureDescriptors = std::min(requestedMaxTextures, maxTexturesHard);
        if (m_bindlessCaps.maxTextureDescriptors == 0) {
            MARK_ERROR("Bindless texture descriptor cap resolved to 0 (sampler/sample limits too small)");
        }

        MARK_INFO_C(Utils::Category::Vulkan,
            "Bindless feature check. Caps: maxMeshes=%u (requested=%u, set=%u, stage=%u), maxTextureDescriptors=%u",
            m_bindlessCaps.maxMeshes, requestedMaxMeshes, maxMeshesFromSet, maxMeshesFromStage, m_bindlessCaps.maxTextureDescriptors);

        // Enable Vulkan features
        VkPhysicalDeviceVulkan12Features v12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = nullptr,
            .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
            .shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
            .descriptorBindingPartiallyBound = VK_TRUE,
            .descriptorBindingVariableDescriptorCount = VK_TRUE,
            .runtimeDescriptorArray = VK_TRUE
        };

        VkPhysicalDeviceVulkan13Features v13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &v12,
            .synchronization2 = VK_TRUE,
            .dynamicRendering = VK_TRUE
        };

        // Ensure the selected device supports nessesary features
        REQ_FEATURE(selectedPhysical.m_features, geometryShader);
        REQ_FEATURE(selectedPhysical.m_features, tessellationShader);
        VkPhysicalDeviceFeatures deviceFeatures = { 
            .geometryShader = VK_TRUE,
            .tessellationShader = VK_TRUE
        };

        VkDeviceCreateInfo deviceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &v13,
            .flags = 0,
            .queueCreateInfoCount = qCount,
            .pQueueCreateInfos = qInfos,
            .enabledLayerCount = 0,         // DEPRECATED
            .ppEnabledLayerNames = nullptr, // DEPRECATED
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = &deviceFeatures
        };

        VkResult res = vkCreateDevice(selectedPhysical.m_device, &deviceCreateInfo, nullptr, &m_device);
        CHECK_VK_RESULT(res, "Create Logical Device");

        // Load device for volk to be able to call device functions
        volkLoadDevice(m_device);

        MARK_INFO_C(Utils::Category::Vulkan, "Logical Device Created");
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

    void VulkanCore::createCaches()
    {
        m_shaderCache = std::make_unique<VulkanShaderCache>(m_device);
        m_graphicsPipelineCache = std::make_unique<VulkanGraphicsPipelineCache>(m_device);
    }

} // namespace Mark::RendererVK