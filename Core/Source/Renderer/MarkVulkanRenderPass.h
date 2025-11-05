#pragma once
#include "MarkVulkanRenderPassCache.h"
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
        VulkanRenderPass(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef);
        ~VulkanRenderPass();
        VulkanRenderPass(const VulkanRenderPass&) = delete;
        VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

        VkRenderPass renderPass() const { return m_renderPassRef.get(); }
        const VkFramebuffer& frameBufferAt(uint32_t _index) const
        {
            if (_index >= m_frameBuffers.size()) MARK_ERROR("Framebuffer index %u out of range (max %zu)", _index, m_frameBuffers.size());
            return m_frameBuffers[_index];
        }

        // Acquire a VkRenderPass from the device cache, then build framebuffers
        void initWithCache(VulkanRenderPassCache& _cache, const VulkanRenderPassKey& _key);
        void destroyFrameBuffers();
        std::vector<VkFramebuffer> createFrameBuffers(VkRenderPass _renderPass);

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VulkanSwapChain& m_swapChainRef;

        VulkanRenderPassRef m_renderPassRef; // From cache
        std::vector<VkFramebuffer> m_frameBuffers;
    };

    // Factory used by the cache to create the pass for a given key
    VkRenderPass CreateSimpleRenderPassForKey(VkDevice _device, const VulkanRenderPassKey& _key);
} // namespace Mark::RendererVK