#include "Mark_ComputePipeline.h"
#include "Mark_VulkanCore.h"

#include "Utils/VulkanUtils.h"
#include "Utils/Mark_Utils.h"

namespace Mark::RendererVK
{
    void VulkanComputePipeline::initialize(std::weak_ptr<VulkanCore> _vulkanCoreRef, const char* _debugName, uint32_t _setsPerFrame, const char* _shaderpath)
    {
        m_device = _vulkanCoreRef.lock()->device();
        m_debugName = _debugName ? _debugName : "UnnamedCompute";
        m_setsPerFrame = _setsPerFrame;
        if (m_setsPerFrame == 0) {
            MARK_ERROR(Utils::Category::Vulkan, "Compute pipeline '%s' initialized with 0 setsPerFrame", m_debugName.c_str());
            m_setsPerFrame = 1;
        }
        
        createDescriptorPool();

        m_descriptorSetLayout = createDescriptorSetLayout();

        createPipelineLayout();

        const auto computeAbsPath = _vulkanCoreRef.lock()->assetPath(_shaderpath);
        m_computeShaderModule = _vulkanCoreRef.lock()->shaderCache().getOrCreateFromGLSL(computeAbsPath.string().c_str());
        if (!m_computeShaderModule) {
            MARK_ERROR(Utils::Category::Vulkan, "Failed to load compute shader '%s'", computeAbsPath.string().c_str());
            return;
        }

        createComputePipeline(m_computeShaderModule);

        MARK_INFO(Utils::Category::Vulkan, "Compute Pipeline Initialized");
    }

    void VulkanComputePipeline::destroyComputePipeline()
    {
        if (m_device == VK_NULL_HANDLE) {
            MARK_FATAL(Utils::Category::Vulkan, "Attempted to destroy compute pipeline with null device");
        }

        m_computeShaderModule = VK_NULL_HANDLE;
        if (m_pipeline            != VK_NULL_HANDLE) { vkDestroyPipeline(m_device, m_pipeline, nullptr); m_pipeline = VK_NULL_HANDLE; }
        if (m_descriptorPool      != VK_NULL_HANDLE) { vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr); m_descriptorPool = VK_NULL_HANDLE; }
        if (m_pipelineLayout      != VK_NULL_HANDLE) { vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr); m_pipelineLayout = VK_NULL_HANDLE; }
        if (m_descriptorSetLayout != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr); m_descriptorSetLayout = VK_NULL_HANDLE; }
    }

    void VulkanComputePipeline::recordCommandBuffer(VkDescriptorSet _descSet, VkCommandBuffer _commandBuffer, uint32_t _groupCountX, uint32_t _groupCountY, uint32_t _groupCountZ)
    {
        vkCmdBindPipeline(_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

        vkCmdBindDescriptorSets(_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &_descSet, 0, nullptr);
        
        vkCmdDispatch(_commandBuffer, _groupCountX, _groupCountY, _groupCountZ);
    }

    void VulkanComputePipeline::allocateDescriptorSets(uint32_t _descCount, std::vector<VkDescriptorSet>& _descSets)
    {
        if (_descCount == 0) {
            MARK_ERROR(Utils::Category::Vulkan, "Requested to allocate 0 descriptor sets for compute shader");
            return;
        }
        if (m_descriptorPool == VK_NULL_HANDLE || m_descriptorSetLayout == VK_NULL_HANDLE) {
            MARK_ERROR(Utils::Category::Vulkan, "Attempted to allocate descriptor sets with uninitialized pool or layout");
            return;
        }

        std::vector<VkDescriptorSetLayout> layouts(_descCount, m_descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = m_descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data()
        };

        _descSets.resize(_descCount);

        VkResult res = vkAllocateDescriptorSets(m_device, &allocInfo, _descSets.data());
        CHECK_VK_RESULT(res, "Allocate Descriptor Sets");

        for (uint32_t i = 0; i < _descSets.size(); i++) {
            MARK_VK_NAME_F(m_device, VK_OBJECT_TYPE_DESCRIPTOR_SET, _descSets[i], "ComputePipeline.%s.DescSet[%u]", m_debugName.c_str(), i);
        }
    }

    VkDescriptorSetLayout VulkanComputePipeline::createDescriptorSetLayout()
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        getDescriptorSetLayoutBindings(bindings);
        if (bindings.empty()) {
            MARK_ERROR(Utils::Category::Vulkan, "Compute pipeline '%s' provided 0 descriptor bindings", m_debugName.c_str());
        }
        return createDescriptorSetLayoutHelper(bindings);
    }

    VkDescriptorSetLayout VulkanComputePipeline::createDescriptorSetLayoutHelper(const std::vector<VkDescriptorSetLayoutBinding>& _bindings)
    {
        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = static_cast<uint32_t>(_bindings.size()),
            .pBindings = _bindings.data()
        };

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

        VkResult res = vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &descriptorSetLayout);
        CHECK_VK_RESULT(res, "Create Descriptor Set Layout");

        MARK_VK_NAME(m_device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, descriptorSetLayout, ("ComputePipeline." + m_debugName + ".DescSetLayout").c_str());

        return descriptorSetLayout;
    }

    void VulkanComputePipeline::createPipelineLayout()
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 1, // Assumed to be one descriptor set layout, can be increased to a vector to support multiple layouts
            .pSetLayouts = &m_descriptorSetLayout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr
        };

        VkResult res = vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
        CHECK_VK_RESULT(res, "Create Compute Pipeline Layout");

        MARK_VK_NAME(m_device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, m_pipelineLayout, ("ComputePipeline." + m_debugName + ".PipelineLayout").c_str());
    }

    void VulkanComputePipeline::createComputePipeline(VkShaderModule _shaderModule)
    {
        VkPipelineShaderStageCreateInfo shaderStageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = _shaderModule,
            .pName = "main",
            .pSpecializationInfo = nullptr
        };

        const VkComputePipelineCreateInfo pipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = shaderStageInfo,
            .layout = m_pipelineLayout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        VkResult res = vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_pipeline);
        CHECK_VK_RESULT(res, "Create Compute Pipeline");

        MARK_VK_NAME(m_device, VK_OBJECT_TYPE_PIPELINE, m_pipeline, ("ComputePipeline." + m_debugName + ".Pipeline").c_str());
    }

    void VulkanComputePipeline::createDescriptorPool()
    {
        std::vector<VkDescriptorPoolSize> sizes;
        getDescriptorPoolSizes(m_setsPerFrame, sizes);
        if (sizes.empty()) {
            MARK_ERROR(Utils::Category::Vulkan, "Compute pipeline '%s' provided 0 descriptor pool sizes", m_debugName.c_str());
        }

        VkDescriptorPoolCreateInfo poolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = 0,
            .maxSets = m_setsPerFrame, // one set per frame-in-flight
            .poolSizeCount = static_cast<uint32_t>(sizes.size()),
            .pPoolSizes = sizes.data()
        };

        VkResult res = vkCreateDescriptorPool(m_device, &poolCreateInfo, nullptr, &m_descriptorPool);
        CHECK_VK_RESULT(res, "Create Compute Descriptor Pool");

        MARK_VK_NAME(m_device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, m_descriptorPool, ("ComputePipeline." + m_debugName + ".DescPool").c_str());
    }
}