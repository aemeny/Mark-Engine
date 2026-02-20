#include "Mark_VulkanSwapChain.h"
#include "Mark_VulkanCore.h"
#include "Mark_VulkanPhysicalDevices.h"
#include "Utils/VulkanUtils.h"
#include "Utils/ErrorHandling.h"
#include "Engine/SettingsHandler.h"

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
        bool inPerformanceMode = Mark::Settings::MarkSettings::Get().isInPerformanceMode();

        // Preference MAILBOX if available or Immediate in performance mode
        for (const VkPresentModeKHR& mode : _presentModes)
        {
            if (inPerformanceMode) {
                if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                    return mode;
                }
            }
            else {
                if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    return mode;
                }
            }
        }

        // Otherwise use FIFO which is guaranteed to be supported
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    static VkImageView createImageView(VkDevice _device, VkImage _image, VkFormat _format, VkImageAspectFlags _aspectFlags, 
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
        MARK_VK_NAME(_device, VK_OBJECT_TYPE_IMAGE_VIEW, imageView, "VulkSwapChain.ImageView");

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
        VkDevice device = m_vulkanCoreRef.lock()->device();

        for (VkImageView& imageView : m_swapChainImageViews)
        {
            if (imageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device, imageView, nullptr);
                imageView = VK_NULL_HANDLE;
            }
        }

        // Destroy depth images/views first (framebuffers already destroyed by the render-pass owner)
        for (TextureHandler& depthImage : m_depthImages) 
        {
            depthImage.destroyTextureHandler(device);
        }
        m_depthImages.clear();

        if (m_swapChain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device, m_swapChain, nullptr);
            m_swapChain = VK_NULL_HANDLE;
        }
        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Swap Chain Destroyed");
    }

    void VulkanSwapChain::createSwapChain(uint32_t _fbWidth, uint32_t _fbHeight)
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

                m_surfaceFormat = chooseSurfaceFormatAndColourSpace(surfaceProps.m_surfaceFormats);

                m_extent = surfaceCapabilities.currentExtent;

                // Create the swap chain & info
                VkSwapchainCreateInfoKHR swapChainCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                    .pNext = nullptr,
                    .flags = 0,
                    .surface = m_surfaceRef,
                    .minImageCount = numImages,
                    .imageFormat = m_surfaceFormat.format,
                    .imageColorSpace = m_surfaceFormat.colorSpace,
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
                    .oldSwapchain = VK_NULL_HANDLE
                };
                uint32_t indices[2];
                if (core->graphicsQueueFamilyIndex() != core->presentQueueFamilyIndex()) 
                {
                    indices[0] = core->graphicsQueueFamilyIndex();
                    indices[1] = core->presentQueueFamilyIndex();
                    swapChainCreateInfo.queueFamilyIndexCount = 2;
                    swapChainCreateInfo.pQueueFamilyIndices = indices;
                }

                VkDevice device = core->device();

                VkResult res = vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &m_swapChain);
                CHECK_VK_RESULT(res, "Create Swap Chain");

                MARK_VK_NAME(device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, m_swapChain, "VulkSwapChain.SwapChain");
                MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Swap Chain Created");

                uint32_t numSwapChainImages = 0;
                res = vkGetSwapchainImagesKHR(device, m_swapChain, &numSwapChainImages, nullptr);
                CHECK_VK_RESULT(res, "Get Swap Chain Images Count");
                assert(numImages == numSwapChainImages);

                MARK_DEBUG_C(Utils::Category::Vulkan, "Vulkan Swap Chain number of images: %u", numSwapChainImages);

                m_swapChainImages.resize(numSwapChainImages);
                m_swapChainImageViews.resize(numSwapChainImages);

                res = vkGetSwapchainImagesKHR(device, m_swapChain, &numSwapChainImages, m_swapChainImages.data());
                CHECK_VK_RESULT(res, "Get Swap Chain Images");
                for (uint32_t img = 0; img < numSwapChainImages; img++)
                    MARK_VK_NAME_F(device, VK_OBJECT_TYPE_IMAGE, m_swapChainImages[img], "VulkSwapChain.Image[%u]", img);

                int layerCount = 1;
                int mipLevels = 1;
                for (uint32_t imgIdx = 0; imgIdx < numSwapChainImages; imgIdx++)
                {
                    m_swapChainImageViews[imgIdx] = createImageView(
                        device,
                        m_swapChainImages[imgIdx],
                        m_surfaceFormat.format,
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

    void VulkanSwapChain::recreateSwapChain(uint32_t _fbWidth, uint32_t _fbHeight)
    {
        auto core = m_vulkanCoreRef.lock();
        if (!core) { 
            MARK_ERROR("VulkanCore expired in swapchain recreate"); 
        }

        // Avoid recreation while minimized
        if (_fbWidth == 0 || _fbHeight == 0)
            return;

        destroySwapChain();
        createSwapChain(_fbWidth, _fbHeight);
        createDepthResources();
        initImageLayoutsForDynamicRendering();
    }

    void VulkanSwapChain::createDepthResources()
    {
        int swapChainImageCount = numImages();

        VkFormat depthFormat = m_vulkanCoreRef.lock()->physicalDevices().selected().m_depthFormat;

        for (size_t i = 0; i < swapChainImageCount; i++)
        {
            TextureHandler& depthImage = m_depthImages.emplace_back(TextureHandler{ m_vulkanCoreRef });

            VkImageUsageFlagBits usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            VkMemoryPropertyFlagBits properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            depthImage.createImage(m_extent.width, m_extent.height, depthFormat, usage, properties);

            VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (depthImage.hasStencilComponent(depthFormat)) {
                aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }

            depthImage.m_textureImageView = depthImage.createImageView(depthFormat, aspect);
        }
    }

    void VulkanSwapChain::initImageLayoutsForDynamicRendering()
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore)
        {
            MARK_ERROR("VulkanCore expired in VulkanSwapChain::initImageLayoutsForDynamicRendering");
            return;
        }
        VkDevice device = VkCore->device();

        // Command pool for one-time setup
        VkCommandPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = VkCore->graphicsQueueFamilyIndex()
        };

        VkCommandPool pool = VK_NULL_HANDLE;
        VkResult res = vkCreateCommandPool(device, &poolInfo, nullptr, &pool);
        CHECK_VK_RESULT(res, "Create command pool for initImageLayoutsForDynamicRendering");
        MARK_VK_NAME(device, VK_OBJECT_TYPE_COMMAND_POOL, pool, "VulkSwapChain.DynamicRender.CmdPool");

        VkCommandBufferAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        res = vkAllocateCommandBuffers(device, &allocInfo, &cmd);
        CHECK_VK_RESULT(res, "Allocate command buffer for initImageLayoutsForDynamicRendering");
        MARK_VK_NAME(device, VK_OBJECT_TYPE_COMMAND_BUFFER, cmd, "VulkSwapChain.DynamicRender.CmdBuffer");

        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };
        vkBeginCommandBuffer(cmd, &beginInfo);

        std::vector<VkImageMemoryBarrier> barriers;
        // Colour images: UNDEFINED -> PRESENT_SRC_KHR
        for (VkImage image : m_swapChainImages)
        {
            VkImageMemoryBarrier b{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };
            barriers.push_back(b);
        }

        // Depth images: UNDEFINED -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        VkFormat depthFormat = VkCore->physicalDevices().selected().m_depthFormat;
        for (TextureHandler& depthImage : m_depthImages)
        {
            VkImageMemoryBarrier b{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = depthImage.image(),
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };

            if (TextureHandler::hasStencilComponent(depthFormat))
            {
                b.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }

            barriers.push_back(b);
        }

        if (!barriers.empty())
        {
            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                0,
                0, nullptr,
                0, nullptr,
                static_cast<uint32_t>(barriers.size()),
                barriers.data());
        }

        vkEndCommandBuffer(cmd);

        // Submit and wait
        VulkanQueue& graphicsQueue = VkCore->graphicsQueue();
        graphicsQueue.submitAsync(&cmd, 1, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
        graphicsQueue.waitIdle();

        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
    }

    VkExtent2D VulkanSwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& _capabilities, uint32_t _fbWidth, uint32_t _fbHeight)
    {
        if (_capabilities.currentExtent.width != UINT32_MAX)
            return _capabilities.currentExtent;

        VkExtent2D actual{ _fbWidth, _fbHeight };
        actual.width = std::clamp(actual.width, _capabilities.minImageExtent.width, _capabilities.maxImageExtent.width);
        actual.height = std::clamp(actual.height, _capabilities.minImageExtent.height, _capabilities.maxImageExtent.height);
        return actual;
    }
} // namespace Mark::RendererVK