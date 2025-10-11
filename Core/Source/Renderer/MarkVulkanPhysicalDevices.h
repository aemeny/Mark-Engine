#pragma once
#include <volk.h>

#include <vector>

namespace Mark::RendererVK
{
    struct PhysicalDeviceProperties
    {
        VkPhysicalDevice m_device{ VK_NULL_HANDLE };
        VkPhysicalDeviceProperties m_properties;
        std::vector<VkQueueFamilyProperties> m_queueFamilyProperties;
        std::vector<VkBool32> m_qSupportsPresent;
        std::vector<VkSurfaceFormatKHR> m_surfaceFormats;
        VkSurfaceCapabilitiesKHR m_surfaceCapabilities;
        VkPhysicalDeviceMemoryProperties m_memoryProperties;
        std::vector<VkPresentModeKHR> m_presentModes;
    };

    struct VulkanPhysicalDevices
    {
        VulkanPhysicalDevices() = default;
        ~VulkanPhysicalDevices() = default;

        void initialize(const VkInstance& _instance, const VkSurfaceKHR& _surface);

        uint32_t selectDevice(VkQueueFlags _requiredQueueType, bool _supportsPresent);

        const PhysicalDeviceProperties& Selected() const;

    private:
        std::vector<PhysicalDeviceProperties> m_devices;

        int m_selectedDeviceIndex{ -1 };
    };
}