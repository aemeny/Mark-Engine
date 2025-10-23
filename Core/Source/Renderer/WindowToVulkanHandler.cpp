#include "WindowToVulkanHandler.h"
#include "MarkVulkanCore.h"
#include "Utils/ErrorHandling.h"
#include "Utils/VulkanUtils.h"

#include <GLFW/glfw3.h>

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

    WindowToVulkanHandler::WindowToVulkanHandler(std::weak_ptr<RendererVK::VulkanCore> _vulkanCoreRef, GLFWwindow* _windowRef, VkClearColorValue _clearColour) :
        m_vulkanCoreRef(_vulkanCoreRef), m_window(_windowRef)
    {
        createSurface();

        auto VkCore = m_vulkanCoreRef.lock();
        VkCore->physicalDevices().querySurfaceProperties(m_surface);
        VkCore->selectDevicesForSurface(m_surface);

        m_swapChain.createSwapChain();

        // Create frame data sync objects
        for (FrameSyncData& frameData : m_framesInFlight)
        {
            VkDevice& device = VkCore->device();
            frameData.m_imageAvailableSem = createSemaphore(device);
            frameData.m_renderFinishedSem = createSemaphore(device);
            frameData.m_inFlightFence = createFence(device);
        }

        m_commandBuffers.createCommandPool();
        m_commandBuffers.createCommandBuffers();
        m_commandBuffers.recordCommandBuffers(_clearColour);
    }

    WindowToVulkanHandler::~WindowToVulkanHandler()
    {
        // Destroy frame data sync objects
        for (FrameSyncData& frameData : m_framesInFlight)
        {
            VkDevice& device = m_vulkanCoreRef.lock()->device();
            if (frameData.m_imageAvailableSem) vkDestroySemaphore(device, frameData.m_imageAvailableSem, nullptr);
            if (frameData.m_renderFinishedSem) vkDestroySemaphore(device, frameData.m_renderFinishedSem, nullptr);
            if (frameData.m_inFlightFence) vkDestroyFence(device, frameData.m_inFlightFence, nullptr);
        }

        // Explicitly destroy swap chain before surface
        m_swapChain.destroySwapChain();

        if (m_surface != VK_NULL_HANDLE)
        {
            if (m_vulkanCoreRef.expired())
            {
                MARK_ERROR("VulkanCore reference expired, cannot destroy surface");
            }
            vkDestroySurfaceKHR(m_vulkanCoreRef.lock()->instance(), m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
            printf("GLFW Window Surface Destroyed\n");
        }
    }

    void WindowToVulkanHandler::renderToWindow()
    {
        // Check for valid extent before rendering
        auto extent = m_swapChain.extent();
        if (extent.width == 0 || extent.height == 0) return;

        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) { MARK_ERROR("VulkanCore expired during renderFrame()"); }
        VulkanQueue& queue = VkCore->graphicsQueue();

        // Choose this frame slot's sync objects
        FrameSyncData& frameSyncData = m_framesInFlight[m_frame];

        // Wait for this frame slot to be free on GPU side
        vkWaitForFences(VkCore->device(), 1, &frameSyncData.m_inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(VkCore->device(), 1, &frameSyncData.m_inFlightFence);

        // Acquire swapchain image for window
        uint32_t imageIndex = 0;
        queue.acquireNextImage(m_swapChain.swapChain(), frameSyncData.m_imageAvailableSem, VK_NULL_HANDLE, &imageIndex);

        // Record the command buffer for this image
        VkCommandBuffer cmdBuffer = m_commandBuffers.commandBuffer(imageIndex);

        // Submit to graphics queue: wait on imageAvailable, signal renderFinished
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        queue.submit(cmdBuffer, frameSyncData.m_imageAvailableSem, waitStage, frameSyncData.m_renderFinishedSem, frameSyncData.m_inFlightFence);

        // Present the window
        queue.present(m_swapChain.swapChain(), imageIndex, frameSyncData.m_renderFinishedSem);

        // Increment frames-in-flight index
        m_frame = (m_frame + 1) % static_cast<uint32_t>(m_framesInFlight.size());
    }

    void WindowToVulkanHandler::createSurface()
    {
        if (m_surface != VK_NULL_HANDLE) return;
        if (m_vulkanCoreRef.expired())
        {
            MARK_ERROR("VulkanCore reference expired, cannot create surface");
        }

        VkResult res = glfwCreateWindowSurface(m_vulkanCoreRef.lock()->instance(), m_window, nullptr, &m_surface);

        CHECK_VK_RESULT(res, "Create window surface");
        printf("GLFW Window Surface Created\n");
    }
} // namespace Mark::RendererVK