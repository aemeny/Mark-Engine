#pragma once
#include <volk.h>

namespace Mark::RendererVK
{
    struct VulkanQueue
    {
        VulkanQueue() = default;
        ~VulkanQueue() = default;
        VulkanQueue(const VulkanQueue&) = delete;
        VulkanQueue& operator=(const VulkanQueue&) = delete;

        void initialize(VkDevice _device, uint32_t _queueFamily, uint32_t _queueIndex);
        void destroy();

        void waitIdle();

        // Acquire for a specific window
        void acquireNextImage(VkSwapchainKHR _swapchain,
            VkSemaphore _imageAvailable,
            VkFence _fence, // (can be VK_NULL_HANDLE)
            uint32_t* _outImageIndex);

        // Submit one command buffer
        void submit(VkCommandBuffer* _cmdBuffers,
            int _numCmdBuffers,
            VkSemaphore _waitSemaphore,  // imageAvailable
            VkSemaphore _signalSemaphore, // renderFinished
            VkFence _fence); // (can be VK_NULL_HANDLE)

        // Present one window
        void present(VkSwapchainKHR _swapchain,
            uint32_t _imageIndex,
            VkSemaphore _waitSemaphore); // renderFinished

         VkQueue get() const { return m_queue; }

    private:
        VkQueue m_queue{ VK_NULL_HANDLE };
        VkDevice m_device{ VK_NULL_HANDLE };
    };
} // namespace Mark::RendererVK