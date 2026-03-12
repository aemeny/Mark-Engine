#pragma once
#include "Mark_BufferAndMemoryHelper.h"

#include <Volk/volk.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanCommandBuffers;
    struct MeshHandler;
    enum class IndirectDrawPass : uint8_t
    {
        Opaque,
        Transparent
    };
    struct VulkanIndirectRenderingHelper
    {
        VulkanIndirectRenderingHelper(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanCommandBuffers& _vulkanCommandBuffersRef, IndirectDrawPass _drawPass);
        ~VulkanIndirectRenderingHelper() = default;
        VulkanIndirectRenderingHelper(const VulkanIndirectRenderingHelper&) = delete;
        VulkanIndirectRenderingHelper& operator=(const VulkanIndirectRenderingHelper&) = delete;

        void initialize();
        void destroy(VkDevice _device);

        void rebuildDrawCommands(const std::vector<std::shared_ptr<MeshHandler>>& _meshesToDraw, const glm::vec3* _renderingCameraPosition = nullptr);
        void setMeshVisible(const std::vector<std::shared_ptr<MeshHandler>>& _meshesToDraw, uint32_t _meshIndex, bool _visible, const glm::vec3* _renderingCameraPosition = nullptr);

        const VkBuffer indirectCmdBuffer() const { return m_indirectCmdBuffer.m_buffer; }
        const VkBuffer indirectCountBuffer() const { return m_indirectCountBuffer.m_buffer; }
        const uint32_t maxDraws() const { return m_maxDraws; }

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VulkanCommandBuffers& m_vulkanCommandBuffersRef;
        IndirectDrawPass m_drawPass;

        BufferAndMemory m_indirectCmdBuffer;   // VkDrawIndirectCommand[]
        BufferAndMemory m_indirectCountBuffer; // uint32 drawCount
        std::vector<VkDrawIndirectCommand> m_drawsCPU;
        std::vector<uint8_t> m_meshVisible; // 1 = visible, 0 = culled/removed
        std::vector<uint32_t> m_drawMeshIndicesCPU;
        uint32_t m_maxDraws{ 0 };
        uint32_t m_drawCount{ 0 };

        void createIndirectDrawBuffers();
        void destroyIndirectDrawBuffers(VkDevice _device);

        bool meshBelongsInThisPass(const MeshHandler& _mesh) const;
        void uploadAllDrawCommands();
        void uploadDrawCount();
    };
}