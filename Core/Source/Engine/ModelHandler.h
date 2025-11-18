#pragma once
#include <Volk/volk.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace Mark::RendererVK { 
    struct VulkanCore; 
    struct BufferAndMemory;
}

namespace Mark::Engine
{
    // Temp early simple mesh structure for testing purposes
    struct VertexData
    {
        VertexData(const glm::vec3& _pos, const glm::vec2& _tex) :
            m_position(_pos), m_tex(_tex)
        {}
        glm::vec3 m_position;
        glm::vec2 m_tex;
    };
    struct SimpleMesh
    {
        SimpleMesh();

        // CPU side data (can be streamed from disk in future)
        size_t vertexBufferSize() const { return sizeof(VertexData) * m_vertices.size(); }
        std::vector<VertexData> m_vertices;

        // GPU side buffer and memory created via VulkanCore's VulkanVertexBuffer
        bool hasGPUBuffer() const { return m_bufferAndMemory.m_buffer != VK_NULL_HANDLE; }
        VkBuffer gpuBuffer() const { return m_bufferAndMemory.m_buffer; }
        VkDeviceSize gpuBufferSize() const { return m_bufferAndMemory.m_allocationSize; }

        // Call device uploader to create GPU buffer from CPU data
        void uploadToGPU(std::shared_ptr<RendererVK::VulkanCore> _vulkanCore);
        const void destroyGPUBuffer(VkDevice _device) const;

    private:
        RendererVK::BufferAndMemory m_bufferAndMemory;
    };
} // namespace Mark::Engine