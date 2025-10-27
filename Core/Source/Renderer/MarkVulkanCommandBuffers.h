#pragma once
#include "MarkVulkanSwapChain.h"
#include "MarkVulkanRenderPass.h"
#include "Utils/ErrorHandling.h"

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanCommandBuffers
    {
        VulkanCommandBuffers(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef, VulkanRenderPass& _renderPassRef);
        ~VulkanCommandBuffers() = default;
        void destroyCommandBuffers();
        VulkanCommandBuffers(const VulkanCommandBuffers&) = delete;
        VulkanCommandBuffers& operator=(const VulkanCommandBuffers&) = delete;
        
        void createCommandPool();
        void createCommandBuffers(); 
        void recordCommandBuffers(VkClearColorValue _clearColour);

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

        VkCommandPool m_commandPool{ VK_NULL_HANDLE };
        std::vector<VkCommandBuffer> m_commandBuffers;

        void beginCommandBuffer(VkCommandBuffer _cmdBuffer, VkCommandBufferUsageFlags _usageFlags);
        
        // Track first-use per image so we know the correct oldLayout
        std::vector<uint8_t> m_firstUseFlags; // 1 = first use, 0 = used before
    };
} // namespace Mark::RendererVK