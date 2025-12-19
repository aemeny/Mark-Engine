#include "MarkVulkanCommandBuffers.h"
#include "MarkVulkanCore.h"
#include "Utils/VulkanUtils.h"
#include "Utils/MarkUtils.h"
#include <array>

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

            vkFreeCommandBuffers(VkCore->device(),
                m_commandPool,
                1,
                &m_copyCommandBuffer
            );

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
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
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

        cmdBufferAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        res = vkAllocateCommandBuffers(m_vulkanCoreRef.lock()->device(), &cmdBufferAllocInfo, &m_copyCommandBuffer);
        CHECK_VK_RESULT(res, "Allocate copy command buffers");
        MARK_DEBUG_C(Utils::Category::Vulkan, "Vulkan Copy Command Buffer Allocated");
    }

    void VulkanCommandBuffers::recordCommandBuffers(VkClearColorValue _clearColour)
    {
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = _clearColour;
        clearValues[1].depthStencil = { 1.0f, 0 };

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
            .clearValueCount = static_cast<uint32_t>(clearValues.size()),
            .pClearValues = clearValues.data()
        };

        // Record each command buffer
        for (uint32_t i = 0; i < m_commandBuffers.size(); i++)
        {
            beginCommandBuffer(m_commandBuffers[i], VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

            renderPassBeginInfo.framebuffer = m_renderPassRef.frameBufferAt(i);

            vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Dynamic viewport/scissor
            const auto ext = m_swapChainRef.extent();
            VkViewport viewport{ 0.f, 0.f, static_cast<float>(ext.width), static_cast<float>(ext.height), 0.f, 1.f };
            VkRect2D scissor{ {0,0}, ext };
            vkCmdSetViewport(m_commandBuffers[i], 0, 1, &viewport);
            vkCmdSetScissor(m_commandBuffers[i], 0, 1, &scissor);

            const uint32_t instanceCount = 1;
            const uint32_t firstVertex = 0;
            const uint32_t firstInstance = 0;

            for (uint32_t m = 0; m < m_graphicsPipelineRef.meshCount(); m++)
            {
                const uint32_t indexCountForMesh = m_graphicsPipelineRef.indexCountForMesh(m);
                const uint32_t vertexCount = indexCountForMesh > 0
                    ? indexCountForMesh   // SSBO index path
                    : m_graphicsPipelineRef.vertexCountForMesh(m); // fallback if no indices

                if (vertexCount == 0) continue; // Skip empty meshes

                m_graphicsPipelineRef.bindPipeline(m_commandBuffers[i], i, m);
                vkCmdDraw(m_commandBuffers[i], vertexCount, instanceCount, firstVertex, firstInstance);
            }

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