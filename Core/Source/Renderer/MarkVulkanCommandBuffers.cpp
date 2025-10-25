#include "MarkVulkanCommandBuffers.h"
#include "MarkVulkanCore.h"
#include "Utils/VulkanUtils.h"

namespace Mark::RendererVK
{
    VulkanCommandBuffers::VulkanCommandBuffers(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef) :
        m_vulkanCoreRef(_vulkanCoreRef), m_swapChainRef(_swapChainRef)
    {}

    void VulkanCommandBuffers::destroyCommandBuffers()
    {
        if (m_vulkanCoreRef.expired())
        {
            MARK_ERROR("VulkanCore reference expired, cannot destroy command buffers");
        }
        auto VkCore = m_vulkanCoreRef.lock();

        if (m_commandPool != VK_NULL_HANDLE)
        {
            if (!m_commandBuffers.empty())
            {
                vkFreeCommandBuffers(VkCore->device(),
                    m_commandPool,
                    static_cast<uint32_t>(m_commandBuffers.size()),
                    m_commandBuffers.data()
                );
                m_commandBuffers.clear();
            }

            vkDestroyCommandPool(VkCore->device(), m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }

        m_firstUseFlags.clear();

        printf("Vulkan Command Buffers & Pool Destroyed\n");
    }

    void VulkanCommandBuffers::createCommandPool()
    {
        VkCommandPoolCreateInfo cmdPoolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = m_vulkanCoreRef.lock()->graphicsQueueFamilyIndex()
        };
        VkResult res = vkCreateCommandPool(m_vulkanCoreRef.lock()->device(), &cmdPoolCreateInfo, nullptr, &m_commandPool);
        CHECK_VK_RESULT(res, "Create command pool");

        printf("Vulkan Command Pool Created\n");
    }

    void VulkanCommandBuffers::createCommandBuffers()
    {
        m_commandBuffers.resize(m_swapChainRef.numImages());
        m_firstUseFlags.assign(m_swapChainRef.numImages(), 1u);

        VkCommandBufferAllocateInfo cmdBufferAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size())
        };
        VkResult res = vkAllocateCommandBuffers(m_vulkanCoreRef.lock()->device(), &cmdBufferAllocInfo, m_commandBuffers.data());
        CHECK_VK_RESULT(res, "Allocate command buffers");

        printf("Vulkan Command Buffers Allocated: %zu\n", m_commandBuffers.size());
    }

    void VulkanCommandBuffers::recordCommandBuffers(VkClearColorValue _clearColour)
    {
        for (uint32_t i = 0; i < m_commandBuffers.size(); i++)
        {
            beginCommandBuffer(m_commandBuffers[i], VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

            recordClearForImage(i, _clearColour);

            VkResult res = vkEndCommandBuffer(m_commandBuffers[i]);
            CHECK_VK_RESULT(res, "End command buffer recording");
        }

        printf("Vulkan Command Buffers Recorded\n");
    }

    void VulkanCommandBuffers::beginCommandBuffer(VkCommandBuffer _cmdBuffer, VkCommandBufferUsageFlags _usageFlags)
    {
        VkCommandBufferBeginInfo cmdBufferBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = _usageFlags,
            .pInheritanceInfo = nullptr
        };

        VkResult res = vkBeginCommandBuffer(_cmdBuffer, &cmdBufferBeginInfo);
        CHECK_VK_RESULT(res, "Begin command buffer recording");
    }

    void VulkanCommandBuffers::recordClearForImage(uint32_t _imageIndex, VkClearColorValue _clearColour)
    {
        if (_imageIndex >= m_commandBuffers.size())
            MARK_ERROR("recordClearForImage: image index %u out of range (max %zu)", _imageIndex, m_commandBuffers.size());

        const VkImage image = m_swapChainRef.swapChainImageAt(static_cast<int>(_imageIndex));
        VkCommandBuffer cmdBuffer = m_commandBuffers[_imageIndex];

        VkImageSubresourceRange imageRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0, 
            .levelCount = 1,
            .baseArrayLayer = 0, 
            .layerCount = 1
        };

        // Barrier: OLD -> TRANSFER_DST
        VkImageMemoryBarrier toTransfer{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = m_firstUseFlags[_imageIndex] ? VK_IMAGE_LAYOUT_UNDEFINED
             : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = imageRange
        };
        vkCmdPipelineBarrier(cmdBuffer,
            m_firstUseFlags[_imageIndex] ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, // Dependency Flags
            0, nullptr, // Memory Barriers
            0, nullptr, // Buffer Memory Barriers
            1, &toTransfer // Image Memory Barriers
        ); 

        m_firstUseFlags[_imageIndex] = 0u; // Mark image as used

        // Clear Image
        vkCmdClearColorImage(cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &_clearColour, 1, &imageRange);

        // Barrier: TRANSFER_DST -> PRESENT
        VkImageMemoryBarrier toPresent{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = imageRange
        };
        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toPresent);
    }

} // namespace Mark::RendererVK