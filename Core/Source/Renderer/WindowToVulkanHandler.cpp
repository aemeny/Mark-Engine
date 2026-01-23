#include "WindowToVulkanHandler.h"
#include "MarkVulkanCore.h"
#include "Platform/Window.h"
#include "Utils/ErrorHandling.h"
#include "Utils/VulkanUtils.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

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

    WindowToVulkanHandler::WindowToVulkanHandler(std::weak_ptr<RendererVK::VulkanCore> _vulkanCoreRef, Platform::Window& _windowRef, VkClearColorValue _clearColour) :
        m_vulkanCoreRef(_vulkanCoreRef), m_windowRef(_windowRef), m_clearColour(_clearColour)
    {
        createSurface();

        auto VkCore = m_vulkanCoreRef.lock();
        VkCore->physicalDevices().querySurfaceProperties(m_surface);
        VkCore->selectDevicesForSurface(m_surface);

        m_swapChain.createSwapChain();
        m_swapChain.createDepthResources();
        m_swapChain.initImageLayoutsForDynamicRendering();

        // Create frame data sync objects
        m_imagesInFlight.assign(m_swapChain.numImages(), VK_NULL_HANDLE);
        m_presentSems.resize(m_swapChain.numImages());
        for (VkSemaphore& semaphore : m_presentSems)
        {
            semaphore = createSemaphore(VkCore->device());
        }
        for (FrameSyncData& frameData : m_framesInFlight)
        {
            VkDevice& device = VkCore->device();
            frameData.m_imageAvailableSem = createSemaphore(device);
            frameData.m_inFlightFence = createFence(device);
        }

        m_uniformBuffer.createUniformBuffers(static_cast<uint32_t>(m_swapChain.numImages()));

        m_graphicsPipeline.createGraphicsPipeline();

        m_commandBuffers.createCommandPool();
        m_commandBuffers.createCommandBuffers();
        m_commandBuffers.recordCommandBuffers(_clearColour);
    }

    WindowToVulkanHandler::~WindowToVulkanHandler()
    {
        if (m_vulkanCoreRef.expired()) { MARK_ERROR("VulkanCore reference expired, cannot destroy surface"); }
        auto VkCore = m_vulkanCoreRef.lock();

        // Ensure GPU finished with this window's work before destroying resources
        VkCore->graphicsQueue().waitIdle();
        VkCore->presentQueue().waitIdle();

        // Destroy frame data sync objects
        destroyFrameSyncObjects(VkCore);

        // Destroy command buffers and pool
        m_commandBuffers.destroyCommandBuffers();

        // Destroy uniform buffers
        m_uniformBuffer.destroyUniformBuffers(VkCore->device());

        // TEMP WHILE MESH HANDLING IS BASIC - clean mesh buffers
        for (std::shared_ptr<MeshHandler> mesh : m_meshesToDraw)
        {
            mesh->destroyGPUBuffer(VkCore->device());
        }

        // Destroy graphics pipeline
        m_graphicsPipeline.destroyGraphicsPipeline();

        // Explicitly destroy swap chain before surface
        m_swapChain.destroySwapChain();

        if (m_surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(VkCore->instance(), m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
            MARK_INFO_C(Utils::Category::Vulkan, "GLFW Window Surface Destroyed");
        }
    }

    void WindowToVulkanHandler::destroyFrameSyncObjects(std::shared_ptr<VulkanCore> _VkCoreRef)
    {
        // Destroy frame data sync objects
        for (FrameSyncData& frameData : m_framesInFlight)
        {
            VkDevice& device = _VkCoreRef->device();
            if (frameData.m_imageAvailableSem) vkDestroySemaphore(device, frameData.m_imageAvailableSem, nullptr);
            if (frameData.m_inFlightFence) vkDestroyFence(device, frameData.m_inFlightFence, nullptr);
        }
        for (VkSemaphore& semaphore : m_presentSems)
        {
            if (semaphore) vkDestroySemaphore(_VkCoreRef->device(), semaphore, nullptr);
        }

        MARK_INFO_C(Utils::Category::Vulkan, "Window Frame Sync Objects Destroyed");
    }

    void WindowToVulkanHandler::initCameraController()
    {
        // TEMP camera controller for testing
        float FOV = 45.0f;
        float zNear = 0.1f;
        float zFar = 1000.0f;
        Systems::PersProjInfo projInfo = {
            .m_FOV = FOV,
            .m_windowWidth = static_cast<float>(m_windowRef.windowSize().at(0)),
            .m_windowHeight = static_cast<float>(m_windowRef.windowSize().at(1)),
            .m_zNear = zNear,
            .m_zFar = zFar
        };
        glm::vec3 pos(0.0f, 0.0f, -5.0f);
        glm::vec3 target(0.0f, 0.0f, 0.0f);
        glm::vec3 up(0.0f, 1.0f, 0.0f);

        m_cameraController = std::make_shared<Systems::EarlyCameraController>(pos, target, up, projInfo);
    }

    void WindowToVulkanHandler::renderToWindow()
    {
        // Check for valid extent before rendering
        auto extent = m_swapChain.extent();
        if (extent.width == 0 || extent.height == 0) return;

        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) { MARK_ERROR("VulkanCore expired during renderFrame()"); }

        VulkanQueue& graphicsQueue = VkCore->graphicsQueue();
        VulkanQueue& presentQueue = VkCore->presentQueue();

        // Choose this frame slot's sync objects
        FrameSyncData& frameSyncData = m_framesInFlight[m_frame];

        // Wait for this frame slot to be free on GPU side
        vkWaitForFences(VkCore->device(), 1, &frameSyncData.m_inFlightFence, VK_TRUE, UINT64_MAX);

        // Acquire swapchain image for window
        uint32_t imageIndex = 0;
        graphicsQueue.acquireNextImage(m_swapChain.swapChain(), frameSyncData.m_imageAvailableSem, VK_NULL_HANDLE, &imageIndex);

        // If this image is still in use by a previous frame, wait for that fence
        VkFence oldFence = m_imagesInFlight[imageIndex];
        if (oldFence != VK_NULL_HANDLE && oldFence != frameSyncData.m_inFlightFence)
        {
            vkWaitForFences(VkCore->device(), 1, &oldFence, VK_TRUE, UINT64_MAX);
        }
        // This frame's fence now owns this image
        m_imagesInFlight[imageIndex] = frameSyncData.m_inFlightFence;

        // Reset this frame's fence for the upcoming submit
        vkResetFences(VkCore->device(), 1, &frameSyncData.m_inFlightFence);

        /* TEMP UNIFORM DATA UPDATING FOR TESTING */
        UniformData tempData;
        if (m_cameraController)
        {
            m_cameraController->tick(m_windowRef.handle());
            tempData.WVP = m_cameraController->getVPMatrix();
        }
        else {
            tempData.WVP = glm::mat4(1.0f);
        }

        // Update uniform buffer for this image
        m_uniformBuffer.updateUniformBuffer(imageIndex, tempData);

        // Record the command buffer for this image
        VkCommandBuffer cmdBuffer = m_commandBuffers.commandBuffer(imageIndex);

        // Submit to graphics queue: wait on imageAvailable, signal renderFinished
        VkSemaphore presentSem = m_presentSems[imageIndex];
        if (presentSem == VK_NULL_HANDLE) MARK_ERROR("presentSem is VK_NULL_HANDLE!");

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        graphicsQueue.submit(cmdBuffer, frameSyncData.m_imageAvailableSem, waitStage, presentSem, frameSyncData.m_inFlightFence);

        // Present the window
        presentQueue.present(m_swapChain.swapChain(), imageIndex, presentSem);

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

        VkResult res = glfwCreateWindowSurface(m_vulkanCoreRef.lock()->instance(), m_windowRef.handle(), nullptr, &m_surface);

        CHECK_VK_RESULT(res, "Create window surface");
        MARK_INFO_C(Utils::Category::Vulkan, "GLFW Window Surface Created");
    }

    std::weak_ptr<MeshHandler> WindowToVulkanHandler::addMesh(const char* _meshPath)
    {
        auto rtn = std::make_shared<MeshHandler>(m_vulkanCoreRef, m_commandBuffers);

        const auto assetPath = m_vulkanCoreRef.lock()->assetPath(_meshPath); // Test cat model
        rtn->loadFromOBJ(assetPath.string().c_str(), true/*Flip texture vertically for Vulkan*/);
        rtn->uploadToGPU();

        m_meshesToDraw.push_back(rtn);

        // Rebuild descriptors to include new mesh
        m_graphicsPipeline.rebuildDescriptors();
        // Re-record command buffers to draw the new mesh
        m_commandBuffers.recordCommandBuffers(m_clearColour);

        return rtn;
    }
} // namespace Mark::RendererVK