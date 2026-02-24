#include "Mark_IndirectRenderingHelper.h"
#include "Mark_VulkanCore.h"
#include "Mark_CommandBuffers.h"
#include "Mark_ModelHandler.h"

#include "Utils/Mark_Utils.h"

namespace Mark::RendererVK
{
    VulkanIndirectRenderingHelper::VulkanIndirectRenderingHelper(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanCommandBuffers& _vulkanCommandBuffersRef) :
        m_vulkanCoreRef(_vulkanCoreRef), m_vulkanCommandBuffersRef(_vulkanCommandBuffersRef)
    {}

    void VulkanIndirectRenderingHelper::initialize()
    {
        createIndirectDrawBuffers();
    }

    void VulkanIndirectRenderingHelper::destroy(VkDevice _device)
    {
        destroyIndirectDrawBuffers(_device);
    }

    void VulkanIndirectRenderingHelper::handleDrawCommands(const std::vector<std::shared_ptr<MeshHandler>>& _meshesToDraw, uint32_t _meshIndex)
    {
        const size_t numMeshes = _meshesToDraw.size();

        // Ensure visibility tracking matches mesh list size
        if (m_meshVisible.size() < numMeshes)
            m_meshVisible.resize(numMeshes, 1);

        buildDrawCommandCPU(_meshesToDraw, _meshIndex);
        uploadDrawCommand(_meshIndex);
        uploadDrawCount(numMeshes);
    }

    void VulkanIndirectRenderingHelper::setMeshVisible(const std::vector<std::shared_ptr<MeshHandler>>& _meshesToDraw, uint32_t _meshIndex, bool _visible)
    {
        const size_t numMeshesToDraw = _meshesToDraw.size();
        if (_meshIndex >= numMeshesToDraw) return;
        if (_meshIndex >= m_maxDraws) return;

        auto VkCore = m_vulkanCoreRef.lock();
        if (VkCore) {
            VkCore->graphicsQueue().waitIdle();
        }

        if (m_meshVisible.size() < numMeshesToDraw)
            m_meshVisible.resize(numMeshesToDraw, 1);

        m_meshVisible[_meshIndex] = _visible ? 1 : 0;
        buildDrawCommandCPU(_meshesToDraw, _meshIndex);
        uploadDrawCommand(_meshIndex);
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
        m_vulkanCommandBuffersRef.setIndirectDrawBuffers(m_indirectCmdBuffer.m_buffer, m_indirectCountBuffer.m_buffer, m_maxDraws);
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

    void VulkanIndirectRenderingHelper::buildDrawCommandCPU(const std::vector<std::shared_ptr<MeshHandler>>& _meshesToDraw, uint32_t _meshIndex)
    {
        if (_meshIndex >= m_maxDraws) return;
        if (_meshIndex >= _meshesToDraw.size()) return;

        const bool visible = (_meshIndex < m_meshVisible.size()) ? (m_meshVisible[_meshIndex] != 0) : true;
        if (!visible || !_meshesToDraw[_meshIndex]) {
            m_drawsCPU[_meshIndex] = VkDrawIndirectCommand{ 0, 0, 0, 0 };
            return;
        }

        const uint32_t indexCount = _meshesToDraw[_meshIndex]->indexCount();
        if (indexCount == 0) {
            m_drawsCPU[_meshIndex] = VkDrawIndirectCommand{ 0, 0, 0, 0 };
            return;
        }

        // firstInstance encodes meshIndex -> gl_InstanceIndex
        m_drawsCPU[_meshIndex] = VkDrawIndirectCommand{
            .vertexCount = indexCount,
            .instanceCount = 1,
            .firstVertex = 0,
            .firstInstance = _meshIndex
        };
    }

    void VulkanIndirectRenderingHelper::uploadDrawCommand(uint32_t _meshIndex)
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) return;
        if (_meshIndex >= m_maxDraws) return;

        const VkDeviceSize stride = sizeof(VkDrawIndirectCommand);
        const VkDeviceSize offset = stride * static_cast<VkDeviceSize>(_meshIndex);
        m_indirectCmdBuffer.updateRange(VkCore->device(), &m_drawsCPU[_meshIndex], sizeof(VkDrawIndirectCommand), offset);
    }

    void VulkanIndirectRenderingHelper::uploadDrawCount(const uint32_t _numMeshes)
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) return;

        if (_numMeshes > m_maxDraws) {
            MARK_WARN(Utils::Category::Vulkan,
                "Mesh count (%u) exceeds indirect capacity (%u). Extra meshes will not be drawn.",
                _numMeshes, m_maxDraws);
        }

        m_drawCount = std::min(_numMeshes, m_maxDraws);
        m_indirectCountBuffer.update(VkCore->device(), &m_drawCount, sizeof(uint32_t));
    }
}