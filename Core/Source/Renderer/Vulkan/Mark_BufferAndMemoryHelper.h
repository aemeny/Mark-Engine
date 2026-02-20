#pragma once
#include <volk.h>
#include <memory>
#include <string>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct BufferAndMemory
    {
        BufferAndMemory() = default;
        BufferAndMemory(std::shared_ptr<VulkanCore> _vulkanCoreRef, VkDeviceSize _size, VkBufferUsageFlags _usageFlags, VkMemoryPropertyFlags _propertyFlags, std::string _objName);

        VkBuffer m_buffer{ VK_NULL_HANDLE };
        VkDeviceMemory m_memory{ VK_NULL_HANDLE };
        VkDeviceSize m_allocationSize{ 0 };

        void update(VkDevice _device, const void* _data, size_t _size);
        void updateRange(VkDevice _device, const void* _data, size_t _size, VkDeviceSize _offset);

        void destroy(VkDevice _device);
    };
} // namespace Mark::RendererVK