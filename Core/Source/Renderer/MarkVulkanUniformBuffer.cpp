#include "MarkVulkanUniformBuffer.h"
#include "MarkVulkanCore.h"
#include "Utils/MarkUtils.h"
#include "Utils/VulkanUtils.h"

namespace Mark::RendererVK
{
    VulkanUniformBuffer::VulkanUniformBuffer(std::weak_ptr<VulkanCore> _vulkanCoreRef) :
        m_vulkanCoreRef(_vulkanCoreRef)
    {}

    void VulkanUniformBuffer::createUniformBuffers(const uint32_t _numImages)
    {
        auto vkCore = m_vulkanCoreRef.lock();
        if (!vkCore) { 
            MARK_ERROR("VulkanCore expired in createUniformBuffers"); 
        }
        if (!m_uniformBuffers.empty()) {
            destroyUniformBuffers(vkCore->device());
        }

        m_uniformBuffers.reserve(_numImages);
        m_mappedPtrs.assign(_numImages, nullptr);

        const VkDevice device = vkCore->device();
        const VkDeviceSize dataSize = sizeof(UniformData);

        VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        
        for (uint32_t i = 0; i < _numImages; i++)
        {
            m_uniformBuffers.emplace_back(BufferAndMemory(vkCore, dataSize, usage, properties));

            void* ptr = nullptr;
            VkResult res = vkMapMemory(device, m_uniformBuffers.back().m_memory, 0, dataSize, 0, &ptr);
            CHECK_VK_RESULT(res, "vkMapMemory (uniform buffer)");
            m_mappedPtrs[i] = ptr;
        }
        MARK_INFO_C(::Mark::Utils::Category::Vulkan, "Created %u uniform buffers", _numImages);
    }

    void VulkanUniformBuffer::updateUniformBuffer(uint32_t _imageIndex, const UniformData& _data)
    {
        if (_imageIndex >= m_mappedPtrs.size() || m_mappedPtrs[_imageIndex] == nullptr) {
            MARK_ERROR("UniformBuffer::update: invalid image index %u", _imageIndex);
        }
        std::memcpy(m_mappedPtrs[_imageIndex], &_data, sizeof(UniformData));
    }

    VkDescriptorBufferInfo VulkanUniformBuffer::descriptorInfo(uint32_t _imageIndex) const
    {
        if (_imageIndex >= m_uniformBuffers.size()) {
            MARK_ERROR("UniformBuffer::descriptorInfo: invalid image index %u", _imageIndex);
        }

        return VkDescriptorBufferInfo{
            m_uniformBuffers[_imageIndex].m_buffer,
            0,
            sizeof(UniformData)
        };
    }

    void VulkanUniformBuffer::destroyUniformBuffers(VkDevice _device)
    {
        for (size_t i = 0; i < m_uniformBuffers.size(); i++)
        {
            if (m_mappedPtrs[i])
            {
                vkUnmapMemory(_device, m_uniformBuffers[i].m_memory);
                m_mappedPtrs[i] = nullptr;
            }
            if (m_uniformBuffers[i].m_buffer || m_uniformBuffers[i].m_memory)
            {
                m_uniformBuffers[i].destroy(_device);
            }
        }
        m_uniformBuffers.clear();
        m_mappedPtrs.clear();
    }

} // namespace Mark::RendererVK