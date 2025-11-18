#pragma once
#include "MarkVulkanSwapChain.h"
#include "MarkVulkanRenderPass.h"
#include "MarkVulkanGraphicsPipeline.h"
#include "Utils/ErrorHandling.h"

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanCommandBuffers
    {
        VulkanCommandBuffers(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef, VulkanRenderPass& _renderPassRef, VulkanGraphicsPipeline& _graphicsPipelineRef);
        ~VulkanCommandBuffers() = default;
        void destroyCommandBuffers();
        VulkanCommandBuffers(const VulkanCommandBuffers&) = delete;
        VulkanCommandBuffers& operator=(const VulkanCommandBuffers&) = delete;

        void createCommandPool();
        void createCommandBuffers();
        void recordCommandBuffers(VkClearColorValue _clearColour);

        uint32_t imageCount() const { return static_cast<uint32_t>(m_commandBuffers.size()); }
        VkCommandBuffer commandBuffer(uint32_t _index)
        {
            if (_index >= m_commandBuffers.size())
                MARK_ERROR("Command buffer index %u out of range (max %zu)", _index, m_commandBuffers.size());
            return m_commandBuffers[_index];
        }

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VulkanSwapChain& m_swapChainRef;
        VulkanRenderPass& m_renderPassRef;
        VulkanGraphicsPipeline& m_graphicsPipelineRef;

        VkCommandPool m_commandPool{ VK_NULL_HANDLE };
        std::vector<VkCommandBuffer> m_commandBuffers;
        VkCommandBuffer m_copyCommandBuffer{ VK_NULL_HANDLE };

        friend struct VulkanVertexBuffer; // Access within copyBuffer
        void beginCommandBuffer(VkCommandBuffer _cmdBuffer, VkCommandBufferUsageFlags _usageFlags);
    };
} // namespace Mark::RendererVK