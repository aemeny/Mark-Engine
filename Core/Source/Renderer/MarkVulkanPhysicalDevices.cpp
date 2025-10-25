#include "MarkVulkanPhysicalDevices.h"
#include "Utils/VulkanUtils.h"
#include "Utils/MarkUtils.h"

#include <assert.h>

namespace Mark::RendererVK
{
    static void printImageUsageFlags(const VkImageUsageFlags& _flags)
    {
        if (_flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        {
            MARK_DEBUG("Image usage transfer src is supported");
        }
        if (_flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        {
            MARK_DEBUG("Image usage transfer dest is supported");
        }
        if (_flags & VK_IMAGE_USAGE_SAMPLED_BIT)
        {
            MARK_DEBUG("Image usage sampled is supported");
        }
        if (_flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        {
            MARK_DEBUG("Image usage color attachment is supported");
        }
        if (_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            MARK_DEBUG("Image usage depth stencil attachment is supported");
        }
        if (_flags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
        {
            MARK_DEBUG("Image usage transient attachment is supported");
        }
        if (_flags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
        {
            MARK_DEBUG("Image usage input attachment is supported");
        }
    }
    static void printMemoryProperty(VkMemoryPropertyFlags _PropertyFlags)
    {
        std::string out = "    ";
        if (_PropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        {
            out += "DEVICE LOCAL ";
        }
        if (_PropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            out += "HOST VISIBLE ";
        }
        if (_PropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        {
            out += "HOST COHERENT ";
        }
        if (_PropertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
        {
            out += "HOST CACHED ";
        }
        if (_PropertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
        {
            out += "LAZILY ALLOCATED ";
        }
        if (_PropertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT)
        {
            out += "PROTECTED ";
        }
        MARK_DEBUG("%s", out.c_str());
    }

    void VulkanPhysicalDevices::initialize(const VkInstance& _instance)
    {
        uint32_t deviceCount = 0;

        VkResult res = vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
        CHECK_VK_RESULT(res, "Enumerate Physical Devices Count");

        MARK_DEBUG("Found %u Physical Devices", deviceCount);

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

            MARK_DEBUG("Device name: %s", m_devices[i].m_properties.deviceName);
            uint32_t apiVersion = m_devices[i].m_properties.apiVersion;
            MARK_DEBUG("    API Version: %d.%d.%d.%d",
                VK_API_VERSION_VARIANT(apiVersion),
                VK_API_VERSION_MAJOR(apiVersion),
                VK_API_VERSION_MINOR(apiVersion),
                VK_API_VERSION_PATCH(apiVersion));

            vkGetPhysicalDeviceMemoryProperties(currentDevice, &(m_devices[i].m_memoryProperties));

            MARK_DEBUG("    Num memory types: %d", m_devices[i].m_memoryProperties.memoryTypeCount);

            for (uint32_t j = 0; j < m_devices[i].m_memoryProperties.memoryTypeCount; j++)
            {
                MARK_DEBUG("    %d: flags %x heap %d ", j,
                    m_devices[i].m_memoryProperties.memoryTypes[j].propertyFlags,
                    m_devices[i].m_memoryProperties.memoryTypes[j].heapIndex);

                printMemoryProperty(m_devices[i].m_memoryProperties.memoryTypes[j].propertyFlags);
            }

            MARK_DEBUG("    Num memory heaps: %d", m_devices[i].m_memoryProperties.memoryHeapCount);

            vkGetPhysicalDeviceFeatures(currentDevice, &m_devices[i].m_features);
        }
    }

    void VulkanPhysicalDevices::querySurfaceProperties(const VkSurfaceKHR& _surface)
    {
        for (uint32_t i = 0; i < m_devices.size(); i++)
        {
            VkPhysicalDevice currentDevice = m_devices[i].m_device;
            SurfaceProperties surfaceProps{ .m_surface = _surface };

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(currentDevice, &queueFamilyCount, nullptr);
            MARK_DEBUG("    Queue Family Count: %d", queueFamilyCount);

            m_devices[i].m_queueFamilyProperties.resize(queueFamilyCount);
            surfaceProps.m_qSupportsPresent.resize(queueFamilyCount);

            vkGetPhysicalDeviceQueueFamilyProperties(currentDevice, &queueFamilyCount, m_devices[i].m_queueFamilyProperties.data());

            for (uint32_t q = 0; q < queueFamilyCount; q++)
            {
                const VkQueueFamilyProperties& qFamilyProps = m_devices[i].m_queueFamilyProperties[q];

                MARK_DEBUG("    Family %d Num queues: %d ", q, qFamilyProps.queueCount);
                VkQueueFlags flags = qFamilyProps.queueFlags;
                MARK_DEBUG("    GFX %s, Compute %s, Transfer %s, Sparse binding %s",
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
                MARK_DEBUG("    Format %d: %x colorspace %x", j, surfaceFormat.format, surfaceFormat.colorSpace);
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

            MARK_DEBUG("Number of presentation modes: %d", presentModeCount);

            m_devices[i].m_surfacesLinked.push_back(surfaceProps);
        }
    }

    VulkanPhysicalDevices::selectDeviceResult VulkanPhysicalDevices::selectDeviceForSurface(VkQueueFlags _requiredQueueFlags, VkSurfaceKHR _surface)
    {
        for (uint32_t i = 0; i < m_devices.size(); ++i) 
        {
            DeviceProperties& deviceProps = m_devices[i];

            // Find the SurfaceProperties entry for this surface on this device
            const SurfaceProperties* surfaceProps = nullptr;
            for (const auto& surfaceLinked : deviceProps.m_surfacesLinked)
            {
                if (surfaceLinked.m_surface == _surface)
                {
                    surfaceProps = &surfaceLinked;
                    break;
                }
            }
            if (!surfaceProps) continue;

            // Pick first graphics family
            uint32_t gfx = UINT32_MAX;
            for (uint32_t q = 0; q < deviceProps.m_queueFamilyProperties.size(); q++)
            {
                if (deviceProps.m_queueFamilyProperties[q].queueFlags & _requiredQueueFlags) 
                { 
                    gfx = q; 
                    break; 
                }
            }
            if (gfx == UINT32_MAX) continue;

            // Pick first present-capable family for this surface
            uint32_t present = UINT32_MAX;
            for (uint32_t q = 0; q < surfaceProps->m_qSupportsPresent.size(); q++) 
            {
                if (surfaceProps->m_qSupportsPresent[q]) 
                { 
                    present = q; 
                    break; 
                }
            }
            if (present == UINT32_MAX) continue;

            m_selectedDeviceIndex = static_cast<int>(i);
            MARK_INFO("Using device %u (%s), gfx family %u, present family %u", i, deviceProps.m_properties.deviceName, gfx, present);

            return selectDeviceResult{ 
                .m_deviceIndex = i, 
                .m_gtxQueueFamilyIndex = gfx, 
                .m_presentQueueFamilyIndex = present
            };
        }

        // Unreachable
        MARK_ERROR("No physical device satisfies required queue flags %x with present support for the surface", _requiredQueueFlags);
        return selectDeviceResult();
    }

    const VulkanPhysicalDevices::DeviceProperties& VulkanPhysicalDevices::selected() const
    {
        if (m_selectedDeviceIndex < 0)
        {
            MARK_ERROR("A physical device has not been selected");
        }
        return m_devices[m_selectedDeviceIndex];
    }
} // namespace Mark::RendererVK