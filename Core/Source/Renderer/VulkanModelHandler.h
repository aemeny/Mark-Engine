#pragma once
#include "MarkVulkanVertexBuffer.h"
#include "VulkanTextureHandler.h"

#include <glm/glm.hpp>
#include <vector>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanCommandBuffers;

    struct VertexData
    {
        glm::vec3 m_position{};
        glm::vec3 m_colour{};
        glm::vec3 m_normal{};
        glm::vec2 m_uv{};

        bool operator==(const VertexData& _other) const
        {
            return m_position == _other.m_position &&
                m_colour == _other.m_colour &&
                m_normal == _other.m_normal &&
                m_uv == _other.m_uv;
        }
    };
    struct MeshHandler
    {
        MeshHandler(std::weak_ptr<VulkanCore> _vulkanCore, VulkanCommandBuffers& _commandBuffersRef);
        ~MeshHandler();
        void destroyGPUBuffer(VkDevice _device);

        // CPU side data (can be streamed from disk in future)
        size_t vertexBufferSize() const { return sizeof(VertexData) * m_vertices.size(); }
        size_t indexBufferSize()  const { return sizeof(uint32_t) * m_indices.size(); }

        // Vertex and index accessors
        uint32_t vertexCount() const noexcept { return static_cast<uint32_t>(m_vertices.size()); }
        uint32_t indexCount() const noexcept { return static_cast<uint32_t>(m_indices.size()); }
        const std::vector<VertexData>& vertices() const noexcept { return m_vertices; }
        const std::vector<uint32_t>& indices() const noexcept { return m_indices; }

        // GPU side buffer and memory created via VulkanCore's VulkanVertexBuffer
        bool hasVertexBuffer() const { return m_vertexBuffer.m_buffer != VK_NULL_HANDLE; }
        VkBuffer vertexBuffer() const { return m_vertexBuffer.m_buffer; }
        VkDeviceSize vertexBufferAllocSize() const { return m_vertexBuffer.m_allocationSize; }

        bool hasIndexBuffer() const { return m_indexBuffer.m_buffer != VK_NULL_HANDLE; }
        VkBuffer indexBuffer() const { return m_indexBuffer.m_buffer; }
        VkDeviceSize indexBufferAllocSize() const { return m_indexBuffer.m_allocationSize; }

        // Texture handling
        TextureHandler* texture() const { return m_texture; }
    private:
        std::weak_ptr<VulkanCore> m_vulkanCore;

        RendererVK::BufferAndMemory m_vertexBuffer;
        RendererVK::BufferAndMemory m_indexBuffer;

        std::vector<VertexData> m_vertices;
        std::vector<uint32_t> m_indices{};
        TextureHandler* m_texture{ nullptr };

        bool m_usingFallBack{ false };

        // Call device uploader to create GPU buffer from CPU data
        friend struct WindowToVulkanHandler;
        void uploadToGPU();
        void loadFromOBJ(const char* _meshPath, bool _flipV = true);
    };
} // namespace Mark::RendererVK