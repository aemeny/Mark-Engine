#pragma once
#include "MarkVulkanSwapChain.h"

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanCommandBuffers
    {
        VulkanCommandBuffers(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef);
        ~VulkanCommandBuffers() = default;
        VulkanCommandBuffers(const VulkanCommandBuffers&) = delete;
        VulkanCommandBuffers& operator=(const VulkanCommandBuffers&) = delete;
        
        void createCommandPool();
        void createCommandBuffers(); 
        void recordCommandBuffers(VkClearColorValue _clearColour);

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VulkanSwapChain& m_swapChainRef;

        VkCommandPool m_commandPool{ VK_NULL_HANDLE };
        std::vector<VkCommandBuffer> m_commandBuffers;

        void beginCommandBuffer(VkCommandBuffer _cmdBuffer, VkCommandBufferUsageFlags _usageFlags);
    };
} // namespace Mark::RendererVK