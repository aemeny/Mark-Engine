#pragma once
#include "Mark_VulkanPhysicalDevices.h"
#include "Mark_VulkanShader.h"
#include "Mark_VulkanQueue.h"
#include "Mark_VulkanGraphicsPipelineCache.h"
#include "Platform/imguiHandler.h"

#include <filesystem>

namespace Mark { struct EngineAppInfo; struct Core; }
namespace Mark::Platform { struct WindowManager; }

namespace Mark::RendererVK
{
    struct VulkanVertexBuffer;
    struct WindowToVulkanHandler;

    struct BindlessCaps
    {
        uint32_t maxMeshes = 0;
        uint32_t maxTextureDescriptors = 0;
        const uint32_t numAttachableTextures = 1; // Max number of textures that the mesh can use
        uint32_t maxDrawIndirectCount = 0;
    };
    struct VulkanCore
    {
        VulkanCore(const EngineAppInfo& _appInfo, Core& _core);
        ~VulkanCore();
        VulkanCore(const VulkanCore&) = delete;
        VulkanCore& operator=(const VulkanCore&) = delete;

        const VkInstance& instance() const { return m_instance; }

        void waitForDeviceIdle() const { vkDeviceWaitIdle(m_device); };

        // Device selection and getters
        void selectDevicesForSurface(VkSurfaceKHR _surface);

        VulkanPhysicalDevices& physicalDevices() { return m_physicalDevices; }
        VkDevice& device() { return m_device; }
        uint32_t getMemoryTypeIndex(uint32_t _memoryTypeBits, VkMemoryPropertyFlags _propertyFlags) const;
        uint32_t getInstanceVersion() const;

        // Queue getters
        uint32_t graphicsQueueFamilyIndex() const { return m_selectedDeviceResult.m_gtxQueueFamilyIndex; }
        uint32_t presentQueueFamilyIndex()  const { return m_selectedDeviceResult.m_presentQueueFamilyIndex; }
        VulkanQueue& graphicsQueue() { return m_graphicsQueue; }
        VulkanQueue& presentQueue() {
            // If families are the same, reuse the graphics queue
            return (m_selectedDeviceResult.m_presentQueueFamilyIndex == m_selectedDeviceResult.m_gtxQueueFamilyIndex)
                ? m_graphicsQueue
                : m_presentQueue;
        }

        // ImGui Getter
        Platform::ImGuiHandler& imguiHandler();

        // Cache getters
        VulkanShaderCache& shaderCache() { return *m_shaderCache; }
        VulkanGraphicsPipelineCache& graphicsPipelineCache() { return *m_graphicsPipelineCache; }

        // Vertex buffer uploader getter
        VulkanVertexBuffer& vertexUploader() { return *m_vertexUploader; }

        BindlessCaps& bindlessCaps() noexcept { return m_bindlessCaps; }

        // TEMP FILE PATH
        // --- Asset root / path helpers ---
        const std::filesystem::path& assetRoot() const { return m_assetRoot; }
        // Returns "<assetRoot>/<file>"
        std::filesystem::path assetPath(const std::string& _file) const;

    private:
        // TEMP SHADER FILE PATH
        std::filesystem::path m_assetRoot;

        friend struct Platform::WindowManager;
        Core& m_core;

        const EngineAppInfo& m_appInfo;

        void createInstance();
        void createInstanceVersion();
        struct {
            uint32_t major{ 0 };
            uint32_t minor{ 0 };
            uint32_t patch{ 0 };
        } m_instanceVersion;

        void createDebugCallback();
        void createLogicalDevice();
        void initializeQueue();
        void createCaches();

        VkInstance m_instance{ VK_NULL_HANDLE };
        VkDebugUtilsMessengerEXT m_debugMessenger{ VK_NULL_HANDLE };
        VulkanQueue m_graphicsQueue;
        VulkanQueue m_presentQueue;

        // Devices and their properties
        VulkanPhysicalDevices m_physicalDevices;
        VulkanPhysicalDevices::selectDeviceResult m_selectedDeviceResult;
        VkDevice m_device{ VK_NULL_HANDLE };

        // Vertex buffer uploader
        std::unique_ptr<VulkanVertexBuffer> m_vertexUploader;

        // Bindless / descriptor indexing caps
        BindlessCaps m_bindlessCaps{};

        // Cache
        std::unique_ptr<VulkanShaderCache> m_shaderCache;
        std::unique_ptr<VulkanGraphicsPipelineCache> m_graphicsPipelineCache;
    };
} // namespace Mark::RendererVK