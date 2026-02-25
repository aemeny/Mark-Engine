#pragma once
#include "Mark_TextureHandler.h"
#include "Mark_BufferAndMemoryHelper.h"
#include "Mark_UniformBuffer.h"
#include "Mark_TextureHandler.h"

#include <Volk/volk.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanGraphicsPipeline;
    struct VulkanCommandBuffers;
    struct VulkanSkybox
    {
        VulkanSkybox(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanCommandBuffers* _commandBuffersRef);
        ~VulkanSkybox() = default;
        VulkanSkybox(const VulkanSkybox&) = delete;
        VulkanSkybox& operator=(const VulkanSkybox&) = delete;

        void initialize(uint32_t _numImages, const char* _skyboxTexturePath);
        void destroy();

        void update(int _imageIndex, const glm::mat4& _transformation);
        void recordCommandBUffer(VkCommandBuffer _cmdBuffer, int _imageIndex);

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VulkanCommandBuffers* m_commandBuffersRef{ nullptr };
        VulkanGraphicsPipeline* m_pipelineRef{ nullptr };
        
        void createDescriptorSets();

        TextureHandler m_cubmapTexture{ m_vulkanCoreRef, m_commandBuffersRef };
        VkShaderModule m_vertexShader{ VK_NULL_HANDLE };
        VkShaderModule m_fragmentShader{ VK_NULL_HANDLE };

        std::vector<VkDescriptorSet> m_descriptorSets;
        VulkanUniformBuffer m_uniformBuffer{ m_vulkanCoreRef };
        int m_numImages{ 0 };
    };
} // namespace Mark::RendererVK