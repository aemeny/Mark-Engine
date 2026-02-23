#pragma once
#include <Volk/volk.h>
#include <vector>
#include <memory>
#include <string>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanComputePipeline
    {
        VulkanComputePipeline() = default;
        virtual ~VulkanComputePipeline() = default;
        VulkanComputePipeline(const VulkanComputePipeline&) = delete;
        VulkanComputePipeline& operator=(const VulkanComputePipeline&) = delete;

        void initialize(std::weak_ptr<VulkanCore> _vulkanCoreRef, const char* _debugName, uint32_t _setsPerFrame, const char* _shaderpath);
        void destroyComputePipeline();

        void recordCommandBuffer(VkDescriptorSet _descSet, VkCommandBuffer _commandBuffer,
                                 uint32_t _groupCountX, uint32_t _groupCountY, uint32_t _groupCountZ);

        void allocateDescriptorSets(uint32_t _descCount, std::vector<VkDescriptorSet>& _descSets);

    protected:

        // Derived provides its descriptor bindings and pool sizing
        virtual void getDescriptorSetLayoutBindings(std::vector<VkDescriptorSetLayoutBinding>& _outBindings) const = 0;
        virtual void getDescriptorPoolSizes(uint32_t _setCount, std::vector<VkDescriptorPoolSize>& _outSizes) const = 0;
        
        // Default layout creation uses derived bindings. Derived can override if it needs flags/pNext.
        virtual VkDescriptorSetLayout createDescriptorSetLayout();
        VkDescriptorSetLayout createDescriptorSetLayoutHelper(const std::vector<VkDescriptorSetLayoutBinding>& _bindings);

        VkDevice m_device{ VK_NULL_HANDLE };
        uint32_t m_setsPerFrame{ 0 };

    private:
        std::string m_debugName;

        VkShaderModule m_computeShaderModule{ VK_NULL_HANDLE };
        VkPipeline m_pipeline{ VK_NULL_HANDLE };
        VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
        VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
        VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };

        void createPipelineLayout();
        void createComputePipeline(VkShaderModule _shaderModule);
        void createDescriptorPool();
    };
}