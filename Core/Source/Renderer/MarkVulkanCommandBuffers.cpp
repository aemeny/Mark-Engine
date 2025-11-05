#include "MarkVulkanCommandBuffers.h"
#include "MarkVulkanCore.h"
#include "Utils/VulkanUtils.h"
#include "Utils/MarkUtils.h"

namespace Mark::RendererVK
{
    VulkanCommandBuffers::VulkanCommandBuffers(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef, VulkanRenderPass& _renderPassRef, VulkanGraphicsPipeline& _graphicsPipelineRef) :
        m_vulkanCoreRef(_vulkanCoreRef), m_swapChainRef(_swapChainRef), m_renderPassRef(_renderPassRef), m_graphicsPipelineRef(_graphicsPipelineRef)
    {}

    void VulkanCommandBuffers::destroyCommandBuffers()
    {
        if (m_vulkanCoreRef.expired())
        {
            MARK_LOG_ERROR_C(Utils::Category::Vulkan, "VulkanCore reference expired, cannot destroy command buffers");
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

        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Command Buffers & Pool Destroyed");
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

        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Command Pool Created");
    }

    void VulkanCommandBuffers::createCommandBuffers()
    {
        m_commandBuffers.resize(m_swapChainRef.numImages());

        VkCommandBufferAllocateInfo cmdBufferAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size())
        };
        VkResult res = vkAllocateCommandBuffers(m_vulkanCoreRef.lock()->device(), &cmdBufferAllocInfo, m_commandBuffers.data());
        CHECK_VK_RESULT(res, "Allocate command buffers");

        MARK_DEBUG_C(Utils::Category::Vulkan, "Vulkan Command Buffers Allocated: %zu", m_commandBuffers.size());
    }

    void VulkanCommandBuffers::recordCommandBuffers(VkClearColorValue _clearColour)
    {
        VkClearValue clearValue{ .color = _clearColour };
        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = m_renderPassRef.renderPass(),
            .renderArea = {
                .offset = {
                    .x = 0,
                    .y = 0
                },
                .extent = m_swapChainRef.extent()
            },
            .clearValueCount = 1,
            .pClearValues = &clearValue
        };

        // Record each command buffer
        for (uint32_t i = 0; i < m_commandBuffers.size(); i++)
        {
            beginCommandBuffer(m_commandBuffers[i], VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

            renderPassBeginInfo.framebuffer = m_renderPassRef.frameBufferAt(i);

            vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            m_graphicsPipelineRef.bindPipeline(m_commandBuffers[i]);

            // Verbose for now to test draw a simple triangle
            uint32_t vertexCount = 3;
            uint32_t instanceCount = 1;
            uint32_t firstVertex = 0;
            uint32_t firstInstance = 0;

            vkCmdDraw(m_commandBuffers[i], vertexCount, instanceCount, firstVertex, firstInstance);

            vkCmdEndRenderPass(m_commandBuffers[i]);

            VkResult res = vkEndCommandBuffer(m_commandBuffers[i]);
            CHECK_VK_RESULT(res, "End command buffer recording");
        }

        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Command Buffers Recorded");
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
} // namespace Mark::RendererVK