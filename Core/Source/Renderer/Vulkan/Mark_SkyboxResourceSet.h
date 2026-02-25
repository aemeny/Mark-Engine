#pragma once
#include "Mark_DescriptorSetBundle.h"
#include <Volk/volk.h>

#include <cstdint>
#include <memory>
#include <string>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanSwapChain;
    struct VulkanUniformBuffer;
    struct TextureHandler;

    //  binding 0: UBO (VP / camera rotation)
    //  binding 1: samplerCube (combined image sampler)
    namespace SkyboxBinding
    {
        constexpr uint32_t UBO = 0;
        constexpr uint32_t Cubemap = 1;
    }

    struct VulkanSkyboxResourceSet
    {
        VulkanSkyboxResourceSet() = default;
        ~VulkanSkyboxResourceSet() = default;
        VulkanSkyboxResourceSet(const VulkanSkyboxResourceSet&) = delete;
        VulkanSkyboxResourceSet& operator=(const VulkanSkyboxResourceSet&) = delete;

        void initialize(std::weak_ptr<VulkanCore> _vulkanCoreRef, const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo,  const char* _debugName);

        void destroy(VkDevice _device);

        void recreateForSwapchain(const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo);

        // Update binding 1 across all sets
        void setCubemap(TextureHandler& _cubemap);

        void bind(VkCommandBuffer _cmd, VkPipelineLayout _layout, uint32_t _imageIndex) const;

        VkDescriptorSetLayout layout() const noexcept { return m_set.layout(); }
        uint64_t layoutHash() const noexcept { return m_set.layoutHash(); }

        bool valid() const noexcept { return m_set.hasLayout() && m_set.hasSets(); }

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VkDevice m_device{ VK_NULL_HANDLE };
        std::string m_debugName;

        VulkanDescriptorSetBundle m_set;

        bool m_hasCube{ false };
        VkDescriptorImageInfo m_cubeInfo{};

        void writeUBODescriptors(uint32_t _numImages, const VulkanUniformBuffer& _ubo);
        void writeCubemapDescriptors(uint32_t _numImages);
    };
} // namespace Mark::RendererVK