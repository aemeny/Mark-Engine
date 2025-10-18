#pragma once
#include <volk.h>

#include <vector>

namespace Mark::RendererVK
{
    struct VulkanPhysicalDevices
    {
        struct SurfaceProperties
        {
            const VkSurfaceKHR& m_surface;
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
        
        struct selectDeviceResult
        {
            uint32_t m_deviceIndex{ UINT32_MAX };
            uint32_t m_gtxQueueFamilyIndex{ UINT32_MAX };
            uint32_t m_presentQueueFamilyIndex{ UINT32_MAX };
        };

        VulkanPhysicalDevices() = default;
        ~VulkanPhysicalDevices() = default;

        void initialize(const VkInstance& _instance);
        void querySurfaceProperties(const VkSurfaceKHR& _surface);

        selectDeviceResult selectDeviceForSurface(VkQueueFlags _requiredQueueFlags, VkSurfaceKHR _surface);
        const DeviceProperties& selected() const;

    private:
        std::vector<DeviceProperties> m_devices;

        int m_selectedDeviceIndex{ -1 };
    };
}