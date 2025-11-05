#pragma once
#include "MarkVulkanGraphicsPipelineCache.h" 
#include "MarkVulkanRenderPass.h"
#include "MarkVulkanSwapChain.h"

namespace Mark::Platform{ struct Window; }
namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanGraphicsPipeline
    {
        VulkanGraphicsPipeline(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef, VulkanRenderPass& _renderPassRef);
        ~VulkanGraphicsPipeline() = default;
        void destroyGraphicsPipeline();
        VulkanGraphicsPipeline(const VulkanGraphicsPipeline&) = delete;
        VulkanGraphicsPipeline& operator=(const VulkanGraphicsPipeline&) = delete;

        void createGraphicsPipeline();
        void bindPipeline(VkCommandBuffer _cmdBuffer);

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VulkanSwapChain& m_swapChainRef;
        VulkanRenderPass& m_renderPassRef;

        VulkanGraphicsPipelineRef m_cachedRef{};
        VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
    };
} // namespace Mark::RendererVK