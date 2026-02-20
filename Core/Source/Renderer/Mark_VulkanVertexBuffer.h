#pragma once
#include "Mark_VulkanBufferAndMemoryHelper.h"

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanQueue;

    // Device wide uploader for CPU to GPU transfers (Shared across all windows)
    struct VulkanVertexBuffer
    {
        VulkanVertexBuffer(VkDevice _device, uint32_t _gfxQueueFamilyIndex, VulkanQueue& _gfxQueue);
        ~VulkanVertexBuffer() = default;
        VulkanVertexBuffer(const VulkanVertexBuffer&) = delete;
        VulkanVertexBuffer& operator=(const VulkanVertexBuffer&) = delete;

        void init();
        void destroy();

        // Creation of device local buffer and upload CPU data to it
        BufferAndMemory createDeviceLocalFromCPU(std::shared_ptr<VulkanCore> _vulkanCoreRef, 
            const void* _data, VkDeviceSize _size, 
            VkBufferUsageFlags _usageFlags
        );
    private:
        VkDevice m_device{ VK_NULL_HANDLE };
        uint32_t m_gfxQFamily{ 0 };
        VulkanQueue& m_gfxQueue;
        VkCommandPool m_transferPool{ VK_NULL_HANDLE };
        VkCommandBuffer m_transferCmd{ VK_NULL_HANDLE };
        VkFence m_transferFence{ VK_NULL_HANDLE };

        void copyBuffer(VkBuffer _src, VkBuffer _dst, VkDeviceSize _size);
    };
} // namespace Mark::RendererVK