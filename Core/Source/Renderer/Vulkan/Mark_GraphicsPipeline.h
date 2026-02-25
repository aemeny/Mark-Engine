#pragma once
#include "Mark_GraphicsPipelineCache.h" 
#include "Mark_SwapChain.h"
#include "Mark_UniformBuffer.h"
#include "Mark_ModelHandler.h"

namespace Mark::Platform{ struct Window; }
namespace Mark::RendererVK
{
    namespace Binding
    {
        constexpr uint32_t verticesSSBO = 0;
        constexpr uint32_t indicesSSBO = 1;
        constexpr uint32_t UBO = 2;
        constexpr uint32_t texture = 3;
    }
    struct PipelineDesc
    {
        VkDevice m_device{ VK_NULL_HANDLE };
        VulkanUniformBuffer& m_uniformBufferRef;
        VkShaderModule m_vertexShader{ VK_NULL_HANDLE };
        VkShaderModule m_fragmentShader{ VK_NULL_HANDLE };
        int m_numImages{ 0 };
        VkFormat m_colourFormat{ VK_FORMAT_UNDEFINED };
        VkFormat m_depthFormat{ VK_FORMAT_UNDEFINED };
        VkCompareOp m_depthCompareOp{ VK_COMPARE_OP_LESS };
        bool m_isTexCube{ false };
    };

    struct BindlessCaps;
    struct VulkanGraphicsPipeline
    {
        VulkanGraphicsPipeline(std::weak_ptr<VulkanCore> _vulkanCoreRef, 
            VulkanSwapChain& _swapChainRef, 
            VulkanUniformBuffer& _uniformBufferRef,
            const std::vector<std::shared_ptr<MeshHandler>>* _meshesToDraw);
        ~VulkanGraphicsPipeline() = default;
        void destroyGraphicsPipeline();
        VulkanGraphicsPipeline(const VulkanGraphicsPipeline&) = delete;
        VulkanGraphicsPipeline& operator=(const VulkanGraphicsPipeline&) = delete;

        void createGraphicsPipeline();

        void bindPipeline(VkCommandBuffer _cmdBuffer, uint32_t _imageIndex);
        
        // When meshes change during runtime
        bool tryUpdateDescriptorsWithMesh(const uint32_t _meshIndex);
        void rebuildDescriptors();

        VkPipelineLayout pipelineLayout() const noexcept { return m_pipelineLayout; }

        // Mesh getters
        uint32_t meshCount() const noexcept { return m_meshCount; }
        uint32_t vertexCountForMesh(uint32_t _meshIndex) const { return m_meshesToDraw->at(_meshIndex)->vertexCount(); }
        uint32_t indexCountForMesh(uint32_t _meshIndex) const { return m_meshesToDraw->at(_meshIndex)->indexCount(); }

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VulkanSwapChain& m_swapChainRef;
        VulkanUniformBuffer& m_uniformBufferRef;
        const std::vector<std::shared_ptr<MeshHandler>>* m_meshesToDraw;

        VulkanGraphicsPipelineRef m_cachedRef{};
        VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };

        // Descriptor resources
        VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
        VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };
        std::vector<VkDescriptorSet> m_descriptorSets; // size = numImages

        uint32_t m_meshCount{ 0 }; // How many meshes currently are exposed via descriptors
        // Bindless / descriptor indexing caps
        const BindlessCaps& m_bindlessCaps;
        uint32_t m_maxMeshesLayout{ 0 };        // descriptorCount for bindings 0/1
        uint32_t m_maxTexturesLayout{ 0 };      // descriptorCount for binding 3 (layout max)
        uint32_t m_textureDescriptorCount{ 1 }; // variable descriptor count capacity allocated for binding 3

        void createDescriptorSets(VkDevice _device);
        void createDescriptorPool(uint32_t _numImages, VkDevice _device);
        void createDescriptorSetLayout(VkDevice _device);
        void allocateDescriptorSets(uint32_t _numImages, VkDevice _device);
        void updateDescriptorSets(uint32_t _numImages, VkDevice _device);
    
        // The exact bindings used to create m_descriptorSetLayout (for hashing)
        std::vector<VkDescriptorSetLayoutBinding> m_bindings;
        std::vector<VkDescriptorBindingFlags> m_bindingFlags;
        uint64_t m_descSetLayoutHash{ 0 };
    };
} // namespace Mark::RendererVK