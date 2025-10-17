#pragma once
#include <volk.h>

#include <vector>

namespace Mark::RendererVK
{
    struct VulkanPhysicalDevices
    {
        struct SurfaceProperties
        {
            std::vector<VkBool32> m_qSupportsPresent;
            std::vector<VkSurfaceFormatKHR> m_surfaceFormats;
            VkSurfaceCapabilitiesKHR m_surfaceCapabilities;
            std::vector<VkPresentModeKHR> m_presentModes;
        };
        struct DeviceProperties
        {
            std::vector<SurfaceProperties> m_surfacesLinked;

            VkPhysicalDevice m_device{ VK_NULL_HANDLE };
            VkPhysicalDeviceProperties m_properties{};
            std::vector<VkQueueFamilyProperties> m_queueFamilyProperties;
            VkPhysicalDeviceMemoryProperties m_memoryProperties{};
            VkPhysicalDeviceFeatures m_features{};
        };

        VulkanPhysicalDevices() = default;
        ~VulkanPhysicalDevices() = default;

        void initialize(const VkInstance& _instance);
        void querySurfaceProperties(const VkSurfaceKHR& _surface);

        uint32_t selectDevice(VkQueueFlags _requiredQueueType, bool _supportsPresent);

        const DeviceProperties& selected() const;

    private:
        std::vector<DeviceProperties> m_devices;

        int m_selectedDeviceIndex{ -1 };
    };
}