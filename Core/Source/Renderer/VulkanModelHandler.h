#pragma once
#include "MarkVulkanVertexBuffer.h"
#include "VulkanTextureHandler.h"

#include <glm/glm.hpp>
#include <vector>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanCommandBuffers;

    // Temp early simple mesh structure for testing purposes
    struct VertexData
    {
        VertexData(const glm::vec3& _pos, const glm::vec2& _tex) :
            m_position(_pos), m_tex(_tex)
        {}
        glm::vec3 m_position;
        glm::vec2 m_tex;
    };
    struct MeshHandler
    {
        MeshHandler(std::weak_ptr<VulkanCore> _vulkanCore, VulkanCommandBuffers& _commandBuffersRef);
        ~MeshHandler();

        // CPU side data (can be streamed from disk in future)
        size_t vertexBufferSize() const { return sizeof(VertexData) * m_vertices.size(); }
        std::vector<VertexData> m_vertices;

        // GPU side buffer and memory created via VulkanCore's VulkanVertexBuffer
        bool hasGPUBuffer() const { return m_bufferAndMemory.m_buffer != VK_NULL_HANDLE; }
        VkBuffer gpuBuffer() const { return m_bufferAndMemory.m_buffer; }
        VkDeviceSize gpuBufferSize() const { return m_bufferAndMemory.m_allocationSize; }

        // Call device uploader to create GPU buffer from CPU data
        void uploadToGPU();
        void destroyGPUBuffer(VkDevice _device);

        // Texture handling
        TextureHandler* texture() const { return m_texture; }
    private:
        std::weak_ptr<VulkanCore> m_vulkanCore;

        RendererVK::BufferAndMemory m_bufferAndMemory;

        TextureHandler* m_texture{ nullptr };
    };
} // namespace Mark::RendererVK