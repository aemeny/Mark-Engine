#pragma once
#include "MarkVulkanGraphicsPipelineCache.h" 
#include "MarkVulkanRenderPass.h"
#include "MarkVulkanSwapChain.h"
#include "MarkVulkanUniformBuffer.h"
#include <Engine/ModelHandler.h>

namespace Mark::Platform{ struct Window; }
namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanGraphicsPipeline
    {
        VulkanGraphicsPipeline(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef, 
            VulkanRenderPass& _renderPassRef, VulkanUniformBuffer& _uniformBufferRef,
            const std::vector<std::shared_ptr<Engine::SimpleMesh>>* _meshesToDraw);
        ~VulkanGraphicsPipeline() = default;
        void destroyGraphicsPipeline();
        VulkanGraphicsPipeline(const VulkanGraphicsPipeline&) = delete;
        VulkanGraphicsPipeline& operator=(const VulkanGraphicsPipeline&) = delete;

        void createGraphicsPipeline();
        void bindPipeline(VkCommandBuffer _cmdBuffer, uint32_t _imageIndex, uint32_t _meshIndex);

        // When meshes change during runtime
        void rebuildDescriptors();

        uint32_t meshCount() const noexcept { return m_meshCount; }

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VulkanSwapChain& m_swapChainRef;
        VulkanRenderPass& m_renderPassRef;
        VulkanUniformBuffer& m_uniformBufferRef;
        const std::vector<std::shared_ptr<Engine::SimpleMesh>>* m_meshesToDraw;

        VulkanGraphicsPipelineRef m_cachedRef{};
        VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };

        // Descriptor resources
        VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
        VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };
        std::vector<VkDescriptorSet> m_descriptorSets; // size = numImages * meshCount

        uint32_t m_meshCount{ 0 }; // How many meshes currently are exposed via descriptors

        void createDescriptorSets(VkDevice _device);
        void createDescriptorPool(uint32_t _numSets, VkDevice _device);
        void createDescriptorSetLayout(VkDevice _device);
        void allocateDescriptorSets(uint32_t _numSets, VkDevice _device);
        void updateDescriptorSets(uint32_t _numImages, uint32_t _numSets, VkDevice _device);
    
        // The exact bindings used to create m_descriptorSetLayout (for hashing)
        std::vector<VkDescriptorSetLayoutBinding> m_bindings;
        uint64_t m_descSetLayoutHash{ 0 };
    };
} // namespace Mark::RendererVK