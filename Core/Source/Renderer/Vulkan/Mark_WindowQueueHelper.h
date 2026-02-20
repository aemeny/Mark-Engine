#pragma once
#include <volk.h>
#include <memory>
#include <vector>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanQueue;
    struct VulkanWindowQueueHelper
    {
        VulkanWindowQueueHelper() = default;
        ~VulkanWindowQueueHelper() = default;
        VulkanWindowQueueHelper(const VulkanWindowQueueHelper&) = delete;
        VulkanWindowQueueHelper& operator=(const VulkanWindowQueueHelper&) = delete;

        void initialize(VulkanQueue* _graphicsQueue, VulkanQueue* _presentQueue, VkDevice _device);

        void createFrameSyncObjects(uint32_t _numFrameInFlight);
        void destroyFrameSyncObjects();

        uint32_t acquireNextImage(VkSwapchainKHR _swapchain);
        void submitAsync(VkCommandBuffer* _cmdBuffers, int _numCmdBuffers);
        void present(VkSwapchainKHR _swapchain, uint32_t _imageIndex);

    private:
        VulkanQueue* m_graphicsQueue;
        VulkanQueue* m_presentQueue;
        VkDevice m_device;

        std::vector<VkSemaphore> m_imageAvailableSems;
        std::vector<VkSemaphore> m_renderFinishedSems;
        std::vector<VkFence> m_inFlightFences;
        std::vector<VkFence> m_imagesInFlight;
        uint32_t m_frameIndex{ 0 };
    };
}