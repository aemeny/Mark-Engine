#include "MarkVulkanQueue.h"
#include "Utils/VulkanUtils.h"
#include "Utils/MarkUtils.h"

#include <stdio.h>

namespace Mark::RendererVK
{
    void VulkanQueue::initialize(VkDevice _device, uint32_t _queueFamily, uint32_t _queueIndex)
    {
        m_device = _device;

        // Create the queue
        vkGetDeviceQueue(m_device, _queueFamily, _queueIndex, &m_queue);
        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan queue acquired");
    }

    void VulkanQueue::destroy()
    {
        m_queue = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan queue destroyed");
    }

    void VulkanQueue::waitIdle()
    {
        if (m_queue) vkQueueWaitIdle(m_queue);
    }

    void VulkanQueue::acquireNextImage(VkSwapchainKHR _swapchain, VkSemaphore _imageAvailable, VkFence _fence, uint32_t* _outImageIndex)
    {
        VkResult res = vkAcquireNextImageKHR(m_device, _swapchain, UINT64_MAX, _imageAvailable, _fence, _outImageIndex);
        CHECK_VK_RESULT(res, "Acquire Next Image");
    }

    void VulkanQueue::submit(VkCommandBuffer _cmdBuffer, VkSemaphore _waitSemaphore, VkPipelineStageFlags _waitStage, VkSemaphore _signalSemaphore, VkFence _fence)
    {
        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = _waitSemaphore ? 1u : 0u,
            .pWaitSemaphores = _waitSemaphore ? &_waitSemaphore : nullptr,
            .pWaitDstStageMask = _waitSemaphore ? &_waitStage : nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &_cmdBuffer,
            .signalSemaphoreCount = _signalSemaphore ? 1u : 0u,
            .pSignalSemaphores = _signalSemaphore ? &_signalSemaphore : nullptr
        };

        VkResult res = vkQueueSubmit(m_queue, 1, &submitInfo, _fence);
        CHECK_VK_RESULT(res, "Queue Submit");
    }

    void VulkanQueue::present(VkSwapchainKHR _swapchain, uint32_t _imageIndex, VkSemaphore _waitSemaphore)
    {
        VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = _waitSemaphore ? 1u : 0u,
            .pWaitSemaphores = _waitSemaphore ? &_waitSemaphore : nullptr,
            .swapchainCount = 1,
            .pSwapchains = &_swapchain,
            .pImageIndices = &_imageIndex
        };

        VkResult res =  vkQueuePresentKHR(m_queue, &presentInfo);
        CHECK_VK_RESULT(res, "Queue Present");
    }
} // namespace Mark::RendererVK