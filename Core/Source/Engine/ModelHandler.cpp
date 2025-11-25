#include "ModelHandler.h"
#include "Renderer/MarkVulkanCore.h"
#include "Utils/MarkUtils.h"

namespace Mark::Engine
{
    SimpleMesh::SimpleMesh() :
        m_vertices{ // Example vertex data initialization
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } }, // top-left
        { {  1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } }, // top-right
        { {  0.0f,  1.0f, 0.0f }, { 1.0f, 1.0f } }  // bottom-center
        }
    {}

    void SimpleMesh::uploadToGPU(std::shared_ptr<RendererVK::VulkanCore> _vulkanCore)
    {
        if (!_vulkanCore) {
            MARK_ERROR("VulkanCore is null for mesh upload");
        }
        if (m_vertices.empty()) {
            MARK_WARN_C(Utils::Category::Vulkan, "uploadToGPU called with empty vertex list");
        }

        const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(vertexBufferSize());

        // Device local buffer creation from CPU data
        m_bufferAndMemory = _vulkanCore->vertexUploader().createDeviceLocalFromCPU(
            _vulkanCore,
            m_vertices.data(),
            bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT // STORAGE_BUFFER for programmable vertex pulling
        );

        MARK_INFO_C(Utils::Category::Vulkan, "Mesh uploaded: %zu vertices, %llu bytes",
            m_vertices.size(), static_cast<size_t>(bufferSize));
    }

    void SimpleMesh::destroyGPUBuffer(VkDevice _device)
    {
        if (hasGPUBuffer())
        {
            m_bufferAndMemory.destroy(_device);
            MARK_INFO_C(Utils::Category::Vulkan, "Mesh GPU buffer destroyed");
        }
    }
} // namespace Mark::Engine