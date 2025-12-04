#include "VulkanModelHandler.h"
#include "Renderer/MarkVulkanCore.h"
#include "Utils/MarkUtils.h"

namespace Mark::RendererVK
{
    MeshHandler::MeshHandler(std::weak_ptr<VulkanCore> _vulkanCore) :
        m_vulkanCore(_vulkanCore),
        m_vertices{ // Example vertex data initialization
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } }, // bottom-left
        { {  -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } }, // top-left
        { {  1.0f,  1.0f, 0.0f }, { 1.0f, 1.0f } }, // top-right
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } }, // bottom-left
        { {  1.0f,  1.0f, 0.0f }, { 1.0f, 1.0f } }, // top-right
        { {  1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } }  // bottom-right
        }
    {
        const auto textPath = _vulkanCore.lock()->texturePath("TestTexture1.png");
        m_texture = new TextureHandler(textPath.string().c_str());
    }

    void MeshHandler::uploadToGPU()
    {
        auto VkCore = m_vulkanCore.lock();
        if (!VkCore) {
            MARK_ERROR("VulkanCore is null for mesh upload");
        }
        if (m_vertices.empty()) {
            MARK_WARN_C(Utils::Category::Vulkan, "uploadToGPU called with empty vertex list");
        }

        const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(vertexBufferSize());

        // Device local buffer creation from CPU data
        m_bufferAndMemory = VkCore->vertexUploader().createDeviceLocalFromCPU(
            VkCore,
            m_vertices.data(),
            bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT // STORAGE_BUFFER for programmable vertex pulling
        );

        MARK_INFO_C(Utils::Category::Vulkan, "Mesh uploaded: %zu vertices, %llu bytes",
            m_vertices.size(), static_cast<size_t>(bufferSize));
    }

    void MeshHandler::destroyGPUBuffer(VkDevice _device)
    {
        if (hasGPUBuffer())
        {
            m_bufferAndMemory.destroy(_device);
            MARK_INFO_C(Utils::Category::Vulkan, "Mesh GPU buffer destroyed");
        }
    }
} // namespace Mark::RendererVK