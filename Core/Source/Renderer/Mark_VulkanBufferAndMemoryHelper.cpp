#include "Mark_VulkanBufferAndMemoryHelper.h"
#include "Mark_VulkanCore.h"
#include "Utils/VulkanUtils.h"
#include "Utils/Mark_Utils.h"

namespace Mark::RendererVK
{
    BufferAndMemory::BufferAndMemory(std::shared_ptr<VulkanCore> _vulkanCoreRef, VkDeviceSize _size, VkBufferUsageFlags _usageFlags, VkMemoryPropertyFlags _propertyFlags, std::string _objName)
    {
        // Create buffer
        VkBufferCreateInfo bufferCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = _size,
            .usage = _usageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        VkDevice device = _vulkanCoreRef->device();

        VkResult res = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &m_buffer);
        CHECK_VK_RESULT(res, "Create Buffer");
        MARK_VK_NAME(device, VK_OBJECT_TYPE_BUFFER, m_buffer, (_objName + ".BufferMemory.Buffer").c_str());

        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Buffer Created");

        // Get buffer memory requirements
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, m_buffer, &memRequirements);
        MARK_DEBUG_C(Utils::Category::Vulkan, "Buffer requires %d bytes", memRequirements.size);

        m_allocationSize = memRequirements.size;

        // Get memory type index
        uint32_t memoryTypeIndex = _vulkanCoreRef->getMemoryTypeIndex(memRequirements.memoryTypeBits, _propertyFlags);
        MARK_DEBUG_C(Utils::Category::Vulkan, "Buffer memory type index: %d", memoryTypeIndex);

        // Allocate memory for the buffer
        VkMemoryAllocateInfo memoryAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = memoryTypeIndex
        };

        res = vkAllocateMemory(device, &memoryAllocInfo, nullptr, &m_memory);
        CHECK_VK_RESULT(res, "Allocate Buffer Memory");
        MARK_VK_NAME(device, VK_OBJECT_TYPE_DEVICE_MEMORY, m_memory, (_objName + ".BufferMemory.Memory").c_str());

        // Bind buffer to allocated memory
        res = vkBindBufferMemory(device, m_buffer, m_memory, 0);
        CHECK_VK_RESULT(res, "Bind Buffer Memory");
    }

    void BufferAndMemory::update(VkDevice _device, const void* _data, size_t _size)
    {
        updateRange(_device, _data, _size, 0);
    }

    void BufferAndMemory::updateRange(VkDevice _device, const void* _data, size_t _size, VkDeviceSize _offset)
    {
        if (_offset + _size > static_cast<size_t>(m_allocationSize))
        {
            MARK_ERROR("BufferAndMemory::updateRange out of bounds (offset=%zu size=%zu alloc=%zu)",
                static_cast<size_t>(_offset), _size, static_cast<size_t>(m_allocationSize));
        }

        void* mem = nullptr;
        VkResult res = vkMapMemory(_device, m_memory, _offset, _size, 0, &mem);
        CHECK_VK_RESULT(res, "Map Buffer Memory for Update");
        memcpy(mem, _data, _size);
        vkUnmapMemory(_device, m_memory);
    }

    void BufferAndMemory::destroy(VkDevice _device)
    {
        if (m_buffer)
        {
            vkDestroyBuffer(_device, m_buffer, nullptr);
            m_buffer = VK_NULL_HANDLE;
        }
        if (m_memory)
        {
            vkFreeMemory(_device, m_memory, nullptr);
            m_memory = VK_NULL_HANDLE;
        }
        m_allocationSize = 0;
    }
} // namespace Mark::RendererVK