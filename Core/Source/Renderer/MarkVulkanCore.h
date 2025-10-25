#pragma once
#include "MarkVulkanPhysicalDevices.h"
#include "MarkVulkanQueue.h"

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

        const VkInstance& instance() const { return m_instance; }

        // Device selection and getters
        void selectDevicesForSurface(VkSurfaceKHR _surface);
        VulkanPhysicalDevices& physicalDevices() { return m_physicalDevices; }
        VkDevice& device() { return m_device; }

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


    private:
        friend struct Platform::WindowManager;

        void createInstance(const EngineAppInfo& _appInfo);
        void createDebugCallback();
        void createLogicalDevice();
        void initializeQueue();

        VkInstance m_instance{ VK_NULL_HANDLE };
        VkDebugUtilsMessengerEXT m_debugMessenger{ VK_NULL_HANDLE };
        VulkanQueue m_graphicsQueue;
        VulkanQueue m_presentQueue;

        // Devices and their properties
        VulkanPhysicalDevices m_physicalDevices;
        VulkanPhysicalDevices::selectDeviceResult m_selectedDeviceResult;
        VkDevice m_device{ VK_NULL_HANDLE };
    };
} // namespace Mark::RendererVK