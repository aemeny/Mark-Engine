#pragma once
#include "Mark_TextureHandler.h"
#include "Mark_BufferAndMemoryHelper.h"
#include "Mark_UniformBuffer.h"
#include "Mark_SkyboxResourceSet.h"

#include <Volk/volk.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanGraphicsPipeline;
    struct VulkanCommandBuffers;
    struct VulkanSwapChain;
    struct VulkanSkybox
    {
        VulkanSkybox(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanCommandBuffers* _commandBuffersRef);
        ~VulkanSkybox() = default;
        VulkanSkybox(const VulkanSkybox&) = delete;
        VulkanSkybox& operator=(const VulkanSkybox&) = delete;

        void initialize(const VulkanSwapChain& _swapChain, const char* _skyboxTexturePath);
        void destroy();

        void update(int _imageIndex, const glm::mat4& _transformation);
        void recordCommandBuffer(VkCommandBuffer _cmdBuffer, int _imageIndex);

        void recreateForSwapchain(const VulkanSwapChain& _swapChain);

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VulkanCommandBuffers* m_commandBuffersRef{ nullptr };
        VulkanGraphicsPipeline* m_pipelineRef{ nullptr };
        
        TextureHandler m_cubmapTexture{ m_vulkanCoreRef, m_commandBuffersRef };
        VkShaderModule m_vertexShader{ VK_NULL_HANDLE };
        VkShaderModule m_fragmentShader{ VK_NULL_HANDLE };

        VulkanUniformBuffer m_uniformBuffer{ m_vulkanCoreRef };
        VulkanSkyboxResourceSet m_resourceSet{};
        int m_numImages{ 0 };
    };
} // namespace Mark::RendererVK