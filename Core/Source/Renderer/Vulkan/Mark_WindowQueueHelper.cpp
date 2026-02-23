#include "Mark_WindowQueueHelper.h"
#include "Mark_VulkanCore.h"
#include "Utils/VulkanUtils.h"

namespace Mark::RendererVK
{
    // Queue helpers
    static VkSemaphore createSemaphore(VkDevice _device)
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };

        VkSemaphore semaphore{ VK_NULL_HANDLE };
        VkResult res = vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &semaphore);
        CHECK_VK_RESULT(res, "Create Semaphore");

        return semaphore;
    }
    static VkFence createFence(VkDevice _device, bool _signaled = true)
    {
        VkFenceCreateInfo fenceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = _signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u
        };

        VkFence fence{ VK_NULL_HANDLE };
        VkResult res = vkCreateFence(_device, &fenceCreateInfo, nullptr, &fence);
        CHECK_VK_RESULT(res, "Create Fence");

        return fence;
    }

    void VulkanWindowQueueHelper::initialize(VulkanQueue* _graphicsQueue, VulkanQueue* _presentQueue, VkDevice _device)
    {
        m_graphicsQueue = _graphicsQueue;
        m_presentQueue = _presentQueue;
        m_device = _device;
    }

    void VulkanWindowQueueHelper::createFrameSyncObjects(uint32_t _framesInFlight, uint32_t _swapchainImageCount)
    {
        if (_framesInFlight == 0) _framesInFlight = 1;
        m_framesInFlight = _framesInFlight;
        if (_swapchainImageCount == 0) _swapchainImageCount = 1;
        m_frameIndex = 0;

        m_imageAvailableSems.resize(_framesInFlight);
        for (VkSemaphore& semaphore : m_imageAvailableSems) {
            semaphore = createSemaphore(m_device);
        }

        m_renderFinishedSems.resize(_swapchainImageCount);
        for (VkSemaphore& semaphore : m_renderFinishedSems) {
            semaphore = createSemaphore(m_device);
        }

        m_inFlightFences.resize(_framesInFlight);
        for (VkFence& fence : m_inFlightFences) {
            fence = createFence(m_device);
        }

        m_imagesInFlight.resize(_swapchainImageCount, VK_NULL_HANDLE);
    }

    void VulkanWindowQueueHelper::destroyFrameSyncObjects()
    {
        for (VkSemaphore& semaphore : m_imageAvailableSems) {
            if (semaphore) vkDestroySemaphore(m_device, semaphore, nullptr);
        }
        m_imageAvailableSems.clear();

        for (VkSemaphore& semaphore : m_renderFinishedSems) {
            if (semaphore) vkDestroySemaphore(m_device, semaphore, nullptr);
        }
        m_renderFinishedSems.clear();

        for (VkFence& fence : m_inFlightFences) {
            if (fence) vkDestroyFence(m_device, fence, nullptr);
        }
        m_inFlightFences.clear();

        m_imagesInFlight.clear();

        m_frameIndex = 0;
        m_framesInFlight = 0;

        MARK_INFO(Utils::Category::Vulkan, "Window Frame Sync Objects Destroyed");
    }

    uint32_t VulkanWindowQueueHelper::acquireNextImage(VkSwapchainKHR _swapchain)
    {
        vkWaitForFences(m_device, 1, &m_inFlightFences[m_frameIndex], VK_TRUE, UINT64_MAX);
        vkResetFences(m_device, 1, &m_inFlightFences[m_frameIndex]);

        uint32_t imageIndex = 0;

        m_graphicsQueue->acquireNextImage(
            _swapchain,
            m_imageAvailableSems[m_frameIndex],
            VK_NULL_HANDLE, 
            &imageIndex
        );

        if ((m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) &&
            (m_imagesInFlight[imageIndex] != m_inFlightFences[m_frameIndex])) 
        {
            vkWaitForFences(m_device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        m_imagesInFlight[imageIndex] = m_inFlightFences[m_frameIndex];

        return imageIndex;
    }

    void VulkanWindowQueueHelper::submitAsync(uint32_t _imageIndex, VkCommandBuffer* _cmdBuffers, int _numCmdBuffers)
    {
        m_graphicsQueue->submitAsync(
            _cmdBuffers,
            _numCmdBuffers,
            m_imageAvailableSems[m_frameIndex],
            m_renderFinishedSems[_imageIndex],
            m_inFlightFences[m_frameIndex]
        );
    }

    void VulkanWindowQueueHelper::present(VkSwapchainKHR _swapchain, uint32_t _imageIndex)
    {
        m_presentQueue->present(
            _swapchain,
            _imageIndex,
            m_renderFinishedSems[_imageIndex]
        );

        m_frameIndex = (m_frameIndex + 1) % m_framesInFlight;
    }
}