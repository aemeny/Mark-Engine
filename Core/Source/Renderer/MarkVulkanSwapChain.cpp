#include "MarkVulkanSwapChain.h"
#include "MarkVulkanCore.h"
#include "MarkVulkanPhysicalDevices.h"
#include "Utils/VulkanUtils.h"
#include "Utils/ErrorHandling.h"

#include <cassert>

namespace Mark::RendererVK
{
    // Helpers
    static VkSurfaceFormatKHR chooseSurfaceFormatAndColourSpace(const std::vector<VkSurfaceFormatKHR>& _surfaceFormats)
    {
        // Preference VK_FORMAT_B8G8R8A8_SRGB with VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        for (const VkSurfaceFormatKHR& format : _surfaceFormats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return format;
            }
        }

        // Otherwise return the first available format
        return _surfaceFormats[0];
    }
    static uint32_t chooseNumImages(const VkSurfaceCapabilitiesKHR& _surfaceCapabilities)
    {
        uint32_t requestedNumImages = _surfaceCapabilities.minImageCount + 1;
        
        int finalNumImages = 0;
        if ((_surfaceCapabilities.maxImageCount > 0) && (requestedNumImages > _surfaceCapabilities.maxImageCount))
        {
            finalNumImages = _surfaceCapabilities.maxImageCount;
        }
        else
        {
            finalNumImages = requestedNumImages;
        }

        return finalNumImages;
    }
    static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& _presentModes)
    {
        // Preference MAILBOX if available
        for (const VkPresentModeKHR& mode : _presentModes)
        {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return mode;
            }
        }

        // Otherwise use FIFO which is guaranteed to be supported
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    VkImageView createImageView(VkDevice _device, VkImage _image, VkFormat _format, VkImageAspectFlags _aspectFlags, 
        VkImageViewType _viewType, uint32_t _layerCount, uint32_t _mipLevels)
    {
        VkImageViewCreateInfo viewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = _image,
            .viewType = _viewType,
            .format = _format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = _aspectFlags,
                .baseMipLevel = 0,
                .levelCount = _mipLevels,
                .baseArrayLayer = 0,
                .layerCount = _layerCount
            }
        };

        VkImageView imageView;
        VkResult res = vkCreateImageView(_device, &viewCreateInfo, nullptr, &imageView);
        CHECK_VK_RESULT(res, "Create Image View");
        return imageView;
    }


    VulkanSwapChain::VulkanSwapChain(std::weak_ptr<RendererVK::VulkanCore> m_vulkanCoreRef, VkSurfaceKHR& _swapChainRef) :
        m_vulkanCoreRef(m_vulkanCoreRef), m_surfaceRef(_swapChainRef)
    {}

    void VulkanSwapChain::destroySwapChain()
    {
        if (m_vulkanCoreRef.expired())
        {
            MARK_ERROR("VulkanCore reference expired, cannot destroy swap chain");
        }

        for (VkImageView& imageView : m_swapChainImageViews)
        {
            if (imageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_vulkanCoreRef.lock()->device(), imageView, nullptr);
                imageView = VK_NULL_HANDLE;
            }
        }

        if (m_swapChain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_vulkanCoreRef.lock()->device(), m_swapChain, nullptr);
            m_swapChain = VK_NULL_HANDLE;
        }
        MARK_INFO("Vulkan Swap Chain Destroyed");
    }

    void VulkanSwapChain::createSwapChain()
    {
        for (int i = 0; i < m_vulkanCoreRef.lock()->physicalDevices().selected().m_surfacesLinked.size(); i++)
        {
            if (m_vulkanCoreRef.lock()->physicalDevices().selected().m_surfacesLinked[i].m_surface == m_surfaceRef)
            {
                // Found the matching surface
                auto core = m_vulkanCoreRef.lock();
                if (!core) { MARK_ERROR("VulkanCore expired in swap chain creation"); }
                const VulkanPhysicalDevices::SurfaceProperties& surfaceProps = core->physicalDevices().selected().m_surfacesLinked[i];

                const VkSurfaceCapabilitiesKHR& surfaceCapabilities = surfaceProps.m_surfaceCapabilities;

                uint32_t numImages = chooseNumImages(surfaceCapabilities);

                VkPresentModeKHR presentMode = choosePresentMode(surfaceProps.m_presentModes);

                VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormatAndColourSpace(surfaceProps.m_surfaceFormats);

                m_extent = surfaceCapabilities.currentExtent;

                // Create the swap chain & info
                VkSwapchainCreateInfoKHR swapChainCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                    .pNext = nullptr,
                    .flags = 0,
                    .surface = m_surfaceRef,
                    .minImageCount = numImages,
                    .imageFormat = surfaceFormat.format,
                    .imageColorSpace = surfaceFormat.colorSpace,
                    .imageExtent = m_extent,
                    .imageArrayLayers = 1,
                    .imageUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),

                    .imageSharingMode = (core->graphicsQueueFamilyIndex() == core->presentQueueFamilyIndex())
                                        ? VK_SHARING_MODE_EXCLUSIVE
                                        : VK_SHARING_MODE_CONCURRENT,
                    .queueFamilyIndexCount = 0,
                    .pQueueFamilyIndices = nullptr,
                    .preTransform = surfaceCapabilities.currentTransform,
                    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // Currently ignore alpha channel
                    .presentMode = presentMode,
                    .clipped = VK_TRUE,
                };
                uint32_t indices[2];
                if (core->graphicsQueueFamilyIndex() != core->presentQueueFamilyIndex()) 
                {
                    indices[0] = core->graphicsQueueFamilyIndex();
                    indices[1] = core->presentQueueFamilyIndex();
                    swapChainCreateInfo.queueFamilyIndexCount = 2;
                    swapChainCreateInfo.pQueueFamilyIndices = indices;
                }

                VkResult res = vkCreateSwapchainKHR(m_vulkanCoreRef.lock()->device(), &swapChainCreateInfo, nullptr, &m_swapChain);
                CHECK_VK_RESULT(res, "Create Swap Chain");

                MARK_INFO("Vulkan Swap Chain Created");

                uint32_t numSwapChainImages = 0;
                res = vkGetSwapchainImagesKHR(m_vulkanCoreRef.lock()->device(), m_swapChain, &numSwapChainImages, nullptr);
                CHECK_VK_RESULT(res, "Get Swap Chain Images Count");
                assert(numImages == numSwapChainImages);

                MARK_DEBUG("Vulkan Swap Chain number of images: %u", numSwapChainImages);

                m_swapChainImages.resize(numSwapChainImages);
                m_swapChainImageViews.resize(numSwapChainImages);

                res = vkGetSwapchainImagesKHR(m_vulkanCoreRef.lock()->device(), m_swapChain, &numSwapChainImages, m_swapChainImages.data());
                CHECK_VK_RESULT(res, "Get Swap Chain Images");

                int layerCount = 1;
                int mipLevels = 1;
                for (uint32_t imgIdx = 0; imgIdx < numSwapChainImages; imgIdx++)
                {
                    m_swapChainImageViews[imgIdx] = createImageView(
                        m_vulkanCoreRef.lock()->device(),
                        m_swapChainImages[imgIdx],
                        surfaceFormat.format,
                        VK_IMAGE_ASPECT_COLOR_BIT,
                        VK_IMAGE_VIEW_TYPE_2D,
                        layerCount,
                        mipLevels
                    );
                }

                return;
            }
        }
        MARK_ERROR("Failed to find matching surface for swap chain creation");
    }
} // namespace Mark::RendererVK