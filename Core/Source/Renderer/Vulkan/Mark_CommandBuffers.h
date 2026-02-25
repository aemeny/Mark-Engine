#pragma once
#include "Mark_SwapChain.h"
#include "Mark_GraphicsPipeline.h"
#include "Mark_BindlessResourceSet.h"
#include "Utils/Mark_Utils.h"

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanCommandBuffers
    {
        VulkanCommandBuffers(std::weak_ptr<VulkanCore> _vulkanCoreRef, 
            VulkanSwapChain& _swapChainRef, 
            VulkanGraphicsPipeline& _graphicsPipelineRef,
            VulkanBindlessResourceSet& _bindlessSetRef);
        ~VulkanCommandBuffers() = default;
        void destroyCommandBuffers();
        VulkanCommandBuffers(const VulkanCommandBuffers&) = delete;
        VulkanCommandBuffers& operator=(const VulkanCommandBuffers&) = delete;

        void freeCommandBuffers(size_t _bufferSize, const VkCommandBuffer* _bufferData);

        void createCommandPool();
        void createCommandBuffers(uint32_t _numImages, std::vector<VkCommandBuffer>& _commandBuffers);
        void createCopyCommandBuffer();
        void recordCommandBuffers(VkClearColorValue _clearColour);

        // Must be set before recording command buffers.
        void setIndirectDrawBuffers(VkBuffer _indirectCmdBuffer, VkBuffer _indirectCountBuffer, uint32_t _maxDrawCount);

        void beginCommandBuffer(VkCommandBuffer _cmdBuffer, VkCommandBufferUsageFlags _usageFlags);
        void beginDynamicRendering(VkCommandBuffer _cmdBuffer, uint32_t _imageIndex, VkClearValue* _clearColour, VkClearValue* _depthValue, bool _transitionFromPresent = true);
        void endDynamicRendering(VkCommandBuffer _cmdBuffer, uint32_t _imageIndex, bool _withSecondBarrier);

        uint32_t imageCountBufferWithGUI() const { return static_cast<uint32_t>(m_commandBuffers.withGUI.size()); }
        std::vector<VkCommandBuffer>& commandBuffersWithGUI() { return m_commandBuffers.withGUI; }
        VkCommandBuffer commandBufferWithGUI(uint32_t _index) {
            if (_index >= m_commandBuffers.withGUI.size())
                MARK_FATAL(Utils::Category::Vulkan, "Command buffer index %u out of range (max %zu)", _index, m_commandBuffers.withGUI.size());
            return m_commandBuffers.withGUI[_index];
        }

        uint32_t imageCountBufferWithoutGUI() const { return static_cast<uint32_t>(m_commandBuffers.withoutGUI.size()); }
        std::vector<VkCommandBuffer>& commandBuffersWithoutGUI() { return m_commandBuffers.withoutGUI; }
        VkCommandBuffer commandBufferWithoutGUI(uint32_t _index) {
            if (_index >= m_commandBuffers.withoutGUI.size())
                MARK_FATAL(Utils::Category::Vulkan, "Command buffer index %u out of range (max %zu)", _index, m_commandBuffers.withoutGUI.size());
            return m_commandBuffers.withoutGUI[_index];
        }

        VkCommandBuffer& copyCommandBuffer() { return m_copyCommandBuffer; }

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VulkanSwapChain& m_swapChainRef;
        VulkanGraphicsPipeline& m_graphicsPipelineRef;
        VulkanBindlessResourceSet& m_bindlessSetRef;

        VkCommandPool m_commandPool{ VK_NULL_HANDLE };
        struct {
            std::vector<VkCommandBuffer> withGUI;
            std::vector<VkCommandBuffer> withoutGUI;
        } m_commandBuffers;
        VkCommandBuffer m_copyCommandBuffer{ VK_NULL_HANDLE };

        // Indirect draw buffers
        VkBuffer m_indirectCmdBuffer{ VK_NULL_HANDLE };
        VkBuffer m_indirectCountBuffer{ VK_NULL_HANDLE };
        uint32_t m_maxDrawCount{ 0 };

        void setViewportAndScissor(VkCommandBuffer _cmdBuffer, const VkExtent2D& _extent);

        void recordCommanBuffersInternal(VkClearColorValue _clearColour, std::vector<VkCommandBuffer> _commandBuffers, bool _withSecondBarrier);
    };
} // namespace Mark::RendererVK