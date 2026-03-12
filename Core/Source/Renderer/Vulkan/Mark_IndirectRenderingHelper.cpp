#include "Mark_IndirectRenderingHelper.h"
#include "Mark_VulkanCore.h"
#include "Mark_CommandBuffers.h"
#include "Mark_ModelHandler.h"

#include "Utils/Mark_Utils.h"

#include <algorithm>

namespace Mark::RendererVK
{
    VulkanIndirectRenderingHelper::VulkanIndirectRenderingHelper(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanCommandBuffers& _vulkanCommandBuffersRef, IndirectDrawPass _drawPass) :
        m_vulkanCoreRef(_vulkanCoreRef), m_vulkanCommandBuffersRef(_vulkanCommandBuffersRef), m_drawPass(_drawPass)
    {}

    void VulkanIndirectRenderingHelper::initialize()
    {
        createIndirectDrawBuffers();
    }

    void VulkanIndirectRenderingHelper::destroy(VkDevice _device)
    {
        destroyIndirectDrawBuffers(_device);
    }

    void VulkanIndirectRenderingHelper::setMeshVisible(const std::vector<std::shared_ptr<MeshHandler>>& _meshesToDraw, uint32_t _meshIndex, bool _visible, const glm::vec3* _cameraPosition)
    {
        const size_t numMeshesToDraw = _meshesToDraw.size();
        if (_meshIndex >= numMeshesToDraw) return;

        auto VkCore = m_vulkanCoreRef.lock();
        if (VkCore) {
            VkCore->graphicsQueue().waitIdle();
        }

        if (m_meshVisible.size() < numMeshesToDraw)
            m_meshVisible.resize(numMeshesToDraw, 1);

        m_meshVisible[_meshIndex] = _visible ? 1 : 0;
        rebuildDrawCommands(_meshesToDraw, _cameraPosition);
    }

    void VulkanIndirectRenderingHelper::createIndirectDrawBuffers()
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) { MARK_FATAL(Utils::Category::Vulkan, "VulkanCore expired in createIndirectDrawBuffers"); }

        // Cap draw count by both bindless mesh cap and device maxDrawIndirectCount.
        const uint32_t maxMeshes = VkCore->bindlessCaps().maxMeshes;
        const uint32_t maxIndirect = VkCore->bindlessCaps().maxDrawIndirectCount;
        m_maxDraws = (maxIndirect > 0) ? std::min(maxMeshes, maxIndirect) : maxMeshes;
        if (m_maxDraws == 0) m_maxDraws = 1;

        const VkDeviceSize cmdBytes = sizeof(VkDrawIndirectCommand) * static_cast<VkDeviceSize>(m_maxDraws);

        // Host-visible, coherent updates (1024 draws = 16KB)
        m_indirectCmdBuffer = BufferAndMemory(
            VkCore,
            cmdBytes,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            "WinToVulk.IndirectCmdBuffer"
        );

        m_indirectCountBuffer = BufferAndMemory(
            VkCore,
            sizeof(uint32_t),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            "WinToVulk.IndirectCountBuffer"
        );

        m_drawsCPU.assign(m_maxDraws, VkDrawIndirectCommand{ 0, 0, 0, 0 });
        m_drawCount = 0;
        m_meshVisible.clear();

        // Upload initial empty contents
        m_indirectCmdBuffer.update(VkCore->device(), m_drawsCPU.data(), sizeof(VkDrawIndirectCommand) * static_cast<size_t>(m_maxDraws));
        m_indirectCountBuffer.update(VkCore->device(), &m_drawCount, sizeof(uint32_t));

        // Let the command buffer know what to use
        if (m_drawPass == IndirectDrawPass::Opaque) {
            m_vulkanCommandBuffersRef.setOpaqueIndirectDrawBuffers(m_indirectCmdBuffer.m_buffer, m_indirectCountBuffer.m_buffer, m_maxDraws);
        }
        else {
            m_vulkanCommandBuffersRef.setTransparentIndirectDrawBuffers(m_indirectCmdBuffer.m_buffer, m_indirectCountBuffer.m_buffer, m_maxDraws);
        }
    }

    void VulkanIndirectRenderingHelper::destroyIndirectDrawBuffers(VkDevice _device)
    {
        m_indirectCmdBuffer.destroy(_device);
        m_indirectCountBuffer.destroy(_device);
        m_drawsCPU.clear();
        m_meshVisible.clear();
        m_maxDraws = 0;
        m_drawCount = 0;
    }

    bool VulkanIndirectRenderingHelper::meshBelongsInThisPass(const MeshHandler& _mesh) const
    {
        switch (m_drawPass)
        {
        case IndirectDrawPass::Opaque:      return _mesh.isOpaque();
        case IndirectDrawPass::Transparent: return _mesh.isTransparent();
        default: return false;
        }
    }

    void VulkanIndirectRenderingHelper::rebuildDrawCommands(const std::vector<std::shared_ptr<MeshHandler>>& _meshesToDraw, const glm::vec3* _cameraPosition)
    {
        const size_t numMeshes = _meshesToDraw.size();
        if (m_meshVisible.size() < numMeshes) {
            m_meshVisible.resize(numMeshes, 1);
        }

        m_drawMeshIndicesCPU.clear();
        std::fill(m_drawsCPU.begin(), m_drawsCPU.end(), VkDrawIndirectCommand{ 0, 0, 0, 0 });

        struct TransparentCandidate
        {
            uint32_t meshIndex;
            float distanceSq;
        };

        std::vector<TransparentCandidate> transparentCandidates;

        for (uint32_t meshIndex = 0; meshIndex < numMeshes; meshIndex++)
        {
            const auto& mesh = _meshesToDraw[meshIndex];
            const bool visible = (meshIndex < m_meshVisible.size()) ? (m_meshVisible[meshIndex] != 0) : true;

            if (!visible || !mesh) {
                continue;
            }
            if (!meshBelongsInThisPass(*mesh)) {
                continue;
            }
            if (mesh->indexCount() == 0) {
                continue;
            }

            if (m_drawPass == IndirectDrawPass::Transparent && _cameraPosition != nullptr)
            {
                const glm::vec3 delta = mesh->sortPosition() - *_cameraPosition;
                transparentCandidates.push_back({meshIndex, glm::dot(delta, delta)});
            }
            else {
                m_drawMeshIndicesCPU.push_back(meshIndex);
            }
        }

        if (m_drawPass == IndirectDrawPass::Transparent && _cameraPosition != nullptr)
        {
            std::sort(transparentCandidates.begin(), transparentCandidates.end(),
                [](const TransparentCandidate& _a, const TransparentCandidate& _b)
                {
                    return _a.distanceSq > _b.distanceSq; // Back-to-front sorting
                });

            for (const auto& candidate : transparentCandidates) {
                m_drawMeshIndicesCPU.push_back(candidate.meshIndex);
            }
        }

        m_drawCount = std::min(static_cast<uint32_t>(m_drawMeshIndicesCPU.size()), m_maxDraws);
        if (m_drawMeshIndicesCPU.size() > m_maxDraws) {
            MARK_WARN(Utils::Category::Vulkan, "Filtered mesh count (%zu) exceeds indirect capacity (%u). Extra meshes will not be drawn.", m_drawMeshIndicesCPU.size(), m_maxDraws);

        }

        for (uint32_t drawSlot = 0; drawSlot < m_drawCount; drawSlot++)
        {
            const uint32_t meshIndex = m_drawMeshIndicesCPU[drawSlot];
            const uint32_t indexCount = _meshesToDraw[meshIndex]->indexCount();

            m_drawsCPU[drawSlot] = VkDrawIndirectCommand{
                .vertexCount = indexCount,
                .instanceCount = 1,
                .firstVertex = 0,
                .firstInstance = meshIndex
            };
        }

        uploadAllDrawCommands();
        uploadDrawCount();
    }

    void VulkanIndirectRenderingHelper::uploadAllDrawCommands()
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) return;

        m_indirectCmdBuffer.update(
            VkCore->device(),
            m_drawsCPU.data(),
            sizeof(VkDrawIndirectCommand) * static_cast<size_t>(m_maxDraws)
        );
    }

    void VulkanIndirectRenderingHelper::uploadDrawCount()
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) return;

        m_indirectCountBuffer.update(VkCore->device(), &m_drawCount, sizeof(uint32_t));
    }
}