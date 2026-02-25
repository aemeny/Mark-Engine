#pragma once
#include "Mark_GraphicsPipelineCache.h" 

namespace Mark::RendererVK
{
    struct PipelineDesc
    {
        VkDevice m_device{ VK_NULL_HANDLE };
        VulkanGraphicsPipelineCache& m_cache;
        VkShaderModule m_vertexShader{ VK_NULL_HANDLE };
        VkShaderModule m_fragmentShader{ VK_NULL_HANDLE };
        VkFormat m_colourFormat{ VK_FORMAT_UNDEFINED };
        VkFormat m_depthFormat{ VK_FORMAT_UNDEFINED };
        VkCompareOp m_depthCompareOp{ VK_COMPARE_OP_LESS };
    };

    struct VulkanGraphicsPipeline
    {
        VulkanGraphicsPipeline() = default;
        ~VulkanGraphicsPipeline() = default;
        VulkanGraphicsPipeline(const VulkanGraphicsPipeline&) = delete;
        VulkanGraphicsPipeline& operator=(const VulkanGraphicsPipeline&) = delete;

        void createGraphicsPipeline(const PipelineDesc& _pipelineDesc);
        void destroyGraphicsPipeline();

        void bindPipeline(VkCommandBuffer _cmdBuffer);

        // Provided by a resource set
        void setResourceLayout(VkDescriptorSetLayout _set0Layout, uint64_t _set0LayoutHash);

        VkPipelineLayout pipelineLayout() const noexcept { return m_pipelineLayout; }

    private:
        VulkanGraphicsPipelineRef m_cachedRef{};
        VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };

        // External resource layout (set 0) provided by VulkanBindlessResourceSet
        VkDescriptorSetLayout m_set0Layout{ VK_NULL_HANDLE };
        uint64_t m_set0LayoutHash{ 0 };
    };
} // namespace Mark::RendererVK