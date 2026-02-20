#include "Mark_VertexBuffer.h"
#include "Mark_VulkanCore.h"
#include "Utils/VulkanUtils.h"
#include "Utils/Mark_Utils.h"

namespace Mark::RendererVK
{
    VulkanVertexBuffer::VulkanVertexBuffer(VkDevice _device, uint32_t _gfxQueueFamilyIndex, VulkanQueue& _gfxQueue) :
        m_device(_device), m_gfxQFamily(_gfxQueueFamilyIndex), m_gfxQueue(_gfxQueue)
    {
        init();
    }

    void VulkanVertexBuffer::init()
    {
        // Create command pool for transfer commands
        VkCommandPoolCreateInfo poolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_gfxQFamily
        };
        VkResult res = vkCreateCommandPool(m_device, &poolCreateInfo, nullptr, &m_transferPool);
        CHECK_VK_RESULT(res, "Create Vertex Buffer Command Pool");
        MARK_VK_NAME(m_device, VK_OBJECT_TYPE_COMMAND_POOL, m_transferPool, "VulkVertexBuffer.TransferCmdPool");
        
        // Allocate command buffer for transfer operations
        VkCommandBufferAllocateInfo cmdBuffAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_transferPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        res = vkAllocateCommandBuffers(m_device, &cmdBuffAllocInfo, &m_transferCmd);
        CHECK_VK_RESULT(res, "Allocate Vertex Buffer Command Buffer");
        MARK_VK_NAME(m_device, VK_OBJECT_TYPE_COMMAND_BUFFER, m_transferCmd, "VulkVertexBuffer.TransferCmdBuff");

        // Create fence for transfer synchronization
        VkFenceCreateInfo fenceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };
        res = vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_transferFence);
        CHECK_VK_RESULT(res, "Create Vertex Buffer Transfer Fence");
        MARK_VK_NAME(m_device, VK_OBJECT_TYPE_FENCE, m_transferFence, "VulkVertexBuffer.TransferFence");

        MARK_INFO(Utils::Category::Vulkan, "Vulkan Vertex Buffer Uploader Initialized");
    }

    void VulkanVertexBuffer::destroy()
    {
        if (m_transferFence) { 
            vkDestroyFence(m_device, m_transferFence, nullptr); 
            m_transferFence = VK_NULL_HANDLE; 
        }
        if (m_transferCmd) { 
            vkFreeCommandBuffers(m_device, m_transferPool, 1, &m_transferCmd); 
            m_transferCmd = VK_NULL_HANDLE; 
        }
        if (m_transferPool) { 
            vkDestroyCommandPool(m_device, m_transferPool, nullptr); 
            m_transferPool = VK_NULL_HANDLE; 
        }
    }

    BufferAndMemory VulkanVertexBuffer::createDeviceLocalFromCPU(std::shared_ptr<VulkanCore> _vulkanCoreRef, const void* _data, VkDeviceSize _size, VkBufferUsageFlags _usageFlags)
    {
        // Staging
        BufferAndMemory stagingBuffer(
            _vulkanCoreRef, 
            _size, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            "VulkVertexBuffer.StagingBuffer"
        );

        // Map memory of the staging buffer
        void* memory = nullptr;
        VkDeviceSize offset = 0;
        VkMemoryMapFlags flags = 0;
        VkResult result = vkMapMemory(_vulkanCoreRef->device(), stagingBuffer.m_memory, offset, stagingBuffer.m_allocationSize, flags, &memory);
        CHECK_VK_RESULT(result, "Map Staging Buffer Memory");

        // Copy data to mapped memory
        memcpy(memory, _data, _size);

        // Unmap memory after data copy
        vkUnmapMemory(_vulkanCoreRef->device(), stagingBuffer.m_memory);

        // Create the final buffer
        BufferAndMemory deviceLocalBuffer(
            _vulkanCoreRef,
            _size,
            _usageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "VulkVertexBuffer.DeviceLocalBuffer"
        );

        // Copy data from staging buffer to final buffer
        copyBuffer(stagingBuffer.m_buffer, deviceLocalBuffer.m_buffer, _size);

        // Release staging buffer resources
        stagingBuffer.destroy(_vulkanCoreRef->device());

        return deviceLocalBuffer;
    }

    void VulkanVertexBuffer::copyBuffer(VkBuffer _src, VkBuffer _dst, VkDeviceSize _size)
    {
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };
        VkResult res = vkBeginCommandBuffer(m_transferCmd, &beginInfo);
        CHECK_VK_RESULT(res, "Begin Copy Buffer Command Buffer");

        VkBufferCopy copyRegion = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = _size
        };
        vkCmdCopyBuffer(m_transferCmd, _src, _dst, 1, &copyRegion);

        // Make written data available to VERTEX_SHADER stage for SSBO pulling
        VkBufferMemoryBarrier2 barrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = _dst,
            .offset = 0,
            .size = VK_WHOLE_SIZE
        };
        VkDependencyInfo depInfo = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &barrier,
            .imageMemoryBarrierCount = 0,
            .pImageMemoryBarriers = nullptr
        };
        vkCmdPipelineBarrier2(m_transferCmd, &depInfo);

        res = vkEndCommandBuffer(m_transferCmd);
        CHECK_VK_RESULT(res, "End Copy Buffer Command Buffer");

        // Submit command buffer and wait on fence
        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &m_transferCmd,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };
        res = vkQueueSubmit(m_gfxQueue.get(), 1, &submitInfo, m_transferFence);
        CHECK_VK_RESULT(res, "Submit Copy Buffer Command Buffer");

        // Wait for the transfer to complete
        vkWaitForFences(m_device, 1, &m_transferFence, VK_TRUE, UINT64_MAX);
        vkResetFences(m_device, 1, &m_transferFence);
        vkResetCommandBuffer(m_transferCmd, 0);
    }    
} // namespace Mark::RendererVK