#pragma once
#include "MarkVulkanVertexBuffer.h"
#include <glm/glm.hpp>
#include <vector>

namespace Mark::RendererVK
{
    struct UniformData {
        glm::mat4 WVP;
    };
    static_assert(sizeof(UniformData) % 16 == 0, "UBO must be 16-byte aligned");

    struct VulkanUniformBuffer
    {
        VulkanUniformBuffer(std::weak_ptr<VulkanCore> _vulkanCoreRef);
        ~VulkanUniformBuffer() = default;
        void destroyUniformBuffers(VkDevice _device);
        VulkanUniformBuffer(const VulkanUniformBuffer&) = delete;
        VulkanUniformBuffer& operator=(const VulkanUniformBuffer&) = delete;

        void createUniformBuffers(const uint32_t _numImages);
        void updateUniformBuffer(uint32_t _imageIndex, const UniformData& _data);
        VkDescriptorBufferInfo descriptorInfo(uint32_t _imageIndex) const;

        VkDeviceSize uniformDataSize() const { return sizeof(UniformData); }

        uint32_t bufferCount() const { return static_cast<uint32_t>(m_uniformBuffers.size()); }
        bool hasGPUBufferAt(uint32_t _imageIndex) const { return m_uniformBuffers[_imageIndex].m_buffer != VK_NULL_HANDLE; }
        VkBuffer gpuBufferAt(uint32_t _imageIndex) const { return m_uniformBuffers[_imageIndex].m_buffer; }
        VkDeviceSize gpuBufferSizeAt(uint32_t _imageIndex) const { return m_uniformBuffers[_imageIndex].m_allocationSize; }

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;

        std::vector<BufferAndMemory> m_uniformBuffers; // One per swap chain image
        std::vector<void*> m_mappedPtrs; // Persistently-mapped CPU pointers
    };

} // namespace Mark::RendererVK