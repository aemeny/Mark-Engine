#include "MarkVulkanPhysicalDevices.h"
#include "MarkVulkanUtil.h"

#include <assert.h>

namespace Mark::RendererVK
{
    static void printImageUsageFlags(const VkImageUsageFlags& _flags)
    {
        if (_flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        {
            printf("Image usage transfer src is supported\n");
        }
        if (_flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        {
            printf("Image usage transfer dest is supported\n");
        }
        if (_flags & VK_IMAGE_USAGE_SAMPLED_BIT)
        {
            printf("Image usage sampled is supported\n");
        }
        if (_flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        {
            printf("Image usage color attachment is supported\n");
        }
        if (_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            printf("Image usage depth stencil attachment is supported\n");
        }
        if (_flags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
        {
            printf("Image usage transient attachment is supported\n");
        }
        if (_flags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
        {
            printf("Image usage input attachment is supported\n");
        }
    }
    static void printMemoryProperty(VkMemoryPropertyFlags _PropertyFlags)
    {
        if (_PropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        {
            printf("DEVICE LOCAL ");
        }
        if (_PropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            printf("HOST VISIBLE ");
        }
        if (_PropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        {
            printf("HOST COHERENT ");
        }
        if (_PropertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
        {
            printf("HOST CACHED ");
        }
        if (_PropertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
        {
            printf("LAZILY ALLOCATED ");
        }
        if (_PropertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT)
        {
            printf("PROTECTED ");
        }
    }

    void VulkanPhysicalDevices::initialize(const VkInstance& _instance)
    {
        uint32_t deviceCount = 0;

        VkResult res = vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
        CHECK_VK_RESULT(res, "Enumerate Physical Devices Count");

        printf("Found %u Physical Devices\n\n", deviceCount);

        m_devices.resize(deviceCount);

        std::vector<VkPhysicalDevice> devices;
        devices.resize(deviceCount);

        res = vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());
        CHECK_VK_RESULT(res, "Enumerate Physical Devices");

        for (uint32_t i = 0; i < deviceCount; i++)
        {
            VkPhysicalDevice currentDevice = devices[i];
            m_devices[i].m_device = currentDevice;

            vkGetPhysicalDeviceProperties(currentDevice, &m_devices[i].m_properties);

            printf("Device name: %s\n", m_devices[i].m_properties.deviceName);
            uint32_t apiVersion = m_devices[i].m_properties.apiVersion;
            printf("    API Version: %d.%d.%d.%d\n",
                VK_API_VERSION_VARIANT(apiVersion),
                VK_API_VERSION_MAJOR(apiVersion),
                VK_API_VERSION_MINOR(apiVersion),
                VK_API_VERSION_PATCH(apiVersion));

            vkGetPhysicalDeviceMemoryProperties(currentDevice, &(m_devices[i].m_memoryProperties));

            printf("Num memory types: %d\n", m_devices[i].m_memoryProperties.memoryTypeCount);

            for (uint32_t j = 0; j < m_devices[i].m_memoryProperties.memoryTypeCount; j++)
            {
                printf("%d: flags %x heap %d ", j,
                    m_devices[i].m_memoryProperties.memoryTypes[j].propertyFlags,
                    m_devices[i].m_memoryProperties.memoryTypes[j].heapIndex);

                printMemoryProperty(m_devices[i].m_memoryProperties.memoryTypes[j].propertyFlags);
                printf("\n");
            }

            printf("Num memory heaps: %d\n", m_devices[i].m_memoryProperties.memoryHeapCount);
            printf("\n");

            vkGetPhysicalDeviceFeatures(currentDevice, &m_devices[i].m_features);
        }
    }

    void VulkanPhysicalDevices::querySurfaceProperties(const VkSurfaceKHR& _surface)
    {
        for (uint32_t i = 0; i < m_devices.size(); i++)
        {
            VkPhysicalDevice currentDevice = m_devices[i].m_device;
            SurfaceProperties surfaceProps{};

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(currentDevice, &queueFamilyCount, nullptr);
            printf("    Queue Family Count: %d\n", queueFamilyCount);

            m_devices[i].m_queueFamilyProperties.resize(queueFamilyCount);
            surfaceProps.m_qSupportsPresent.resize(queueFamilyCount);

            vkGetPhysicalDeviceQueueFamilyProperties(currentDevice, &queueFamilyCount, m_devices[i].m_queueFamilyProperties.data());

            for (uint32_t q = 0; q < queueFamilyCount; q++)
            {
                const VkQueueFamilyProperties& qFamilyProps = m_devices[i].m_queueFamilyProperties[q];

                printf("    Family %d Num queues: %d ", q, qFamilyProps.queueCount);
                VkQueueFlags flags = qFamilyProps.queueFlags;
                printf("    GFX %s, Compute %s, Transfer %s, Sparse binding %s\n",
                    (flags & VK_QUEUE_GRAPHICS_BIT) ? "Yes" : "No",
                    (flags & VK_QUEUE_COMPUTE_BIT) ? "Yes" : "No",
                    (flags & VK_QUEUE_TRANSFER_BIT) ? "Yes" : "No",
                    (flags & VK_QUEUE_SPARSE_BINDING_BIT) ? "Yes" : "No");

                VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(currentDevice, q, _surface, &(surfaceProps.m_qSupportsPresent[q]));
                CHECK_VK_RESULT(res, "Get Physical Device Surface Support");
            }

            uint32_t formatCount = 0;
            VkResult res = vkGetPhysicalDeviceSurfaceFormatsKHR(currentDevice, _surface, &formatCount, nullptr);
            CHECK_VK_RESULT(res, "Get Physical Device Surface Formats Count");
            assert(formatCount > 0);

            surfaceProps.m_surfaceFormats.resize(formatCount);

            res = vkGetPhysicalDeviceSurfaceFormatsKHR(currentDevice, _surface, &formatCount, surfaceProps.m_surfaceFormats.data());
            CHECK_VK_RESULT(res, "Get Physical Device Surface Formats");

            for (uint32_t j = 0; j < formatCount; j++)
            {
                const VkSurfaceFormatKHR& surfaceFormat = surfaceProps.m_surfaceFormats[j];
                printf("    Format %d: %x colorspace %x\n", j, surfaceFormat.format, surfaceFormat.colorSpace);
            }

            res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(currentDevice, _surface, &(surfaceProps.m_surfaceCapabilities));
            CHECK_VK_RESULT(res, "Get Physical Device Surface Capabilities");

            printImageUsageFlags(surfaceProps.m_surfaceCapabilities.supportedUsageFlags);

            uint32_t presentModeCount = 0;
            res = vkGetPhysicalDeviceSurfacePresentModesKHR(currentDevice, _surface, &presentModeCount, nullptr);
            CHECK_VK_RESULT(res, "Get Physical Device Surface Present Modes Count");
            assert(presentModeCount != 0);

            surfaceProps.m_presentModes.resize(presentModeCount);

            res = vkGetPhysicalDeviceSurfacePresentModesKHR(currentDevice, _surface, &presentModeCount, surfaceProps.m_presentModes.data());
            CHECK_VK_RESULT(res, "Get Physical Device Surface Present Modes");

            printf("Number of presentation modes: %d\n", presentModeCount);

            m_devices[i].m_surfacesLinked.push_back(surfaceProps);
        }
    }

    uint32_t VulkanPhysicalDevices::selectDevice(VkQueueFlags _requiredQueueType, bool _supportsPresent)
    {
        for (uint32_t i = 0; i < m_devices.size(); i++)
        {
            for (uint32_t j = 0; j < m_devices[i].m_queueFamilyProperties.size(); j++)
            {
                const VkQueueFamilyProperties& qFamilyProps = m_devices[i].m_queueFamilyProperties[j];

                if (qFamilyProps.queueFlags & _requiredQueueType)
                {
                    for (uint32_t k = 0; k < m_devices[i].m_surfacesLinked.size(); k++)
                    {
                        if ((bool)m_devices[i].m_surfacesLinked[k].m_qSupportsPresent[j] == _supportsPresent)
                        {
                            m_selectedDeviceIndex = i;
                            int queueFamily = j;
                            printf("Using GFX device %d (%s) and queue family %d\n", m_selectedDeviceIndex, m_devices[i].m_properties.deviceName, queueFamily);
                            return queueFamily;
                        }
                    }
                }
            }
        }

        MARK_ERROR("Required queue type %x and supports present %d not found\n", _requiredQueueType, _supportsPresent);

        return 0;
    }

    const VulkanPhysicalDevices::DeviceProperties& VulkanPhysicalDevices::selected() const
    {
        if (m_selectedDeviceIndex < 0)
        {
            MARK_ERROR("A physical device has not been selected\n");
        }

        return m_devices[m_selectedDeviceIndex];
    }
}