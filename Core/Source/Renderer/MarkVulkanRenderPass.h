#pragma once
#include "Utils/MarkUtils.h"
#include <volk.h>
#include <memory>
#include <vector>

namespace Mark::Platform { struct Window; }
namespace Mark::RendererVK
{
    struct VulkanSwapChain;
    struct VulkanCore;
    struct VulkanRenderPass
    {
        VulkanRenderPass(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef, Platform::Window& _windowRef);
        ~VulkanRenderPass();
        VulkanRenderPass(const VulkanRenderPass&) = delete;
        VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

        const VkRenderPass& renderPass() const { return m_renderPass; }
        const VkFramebuffer& frameBufferAt(uint32_t _index) const 
        { 
            if (_index >= m_frameBuffers.size()) 
                MARK_ERROR("Framebuffer index %u out of range (max %zu)", _index, m_frameBuffers.size()); 
            return m_frameBuffers[_index]; 
        }

        // Create simple render pass with one sub-pass
        VkRenderPass createSimpleRenderPass();
        std::vector<VkFramebuffer> createFrameBuffers(VkRenderPass _renderPass);

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VulkanSwapChain& m_swapChainRef;
        Platform::Window& m_windowRef;

        VkRenderPass m_renderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_frameBuffers;
    };
} // namespace Mark::RendererVK