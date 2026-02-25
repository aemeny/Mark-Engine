#include "Mark_WindowToVulkanHandler.h"
#include "Mark_VulkanCore.h"
#include "Mark_ModelHandler.h"
#include "Platform/Window.h"
#include "Utils/VulkanUtils.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Mark::RendererVK
{
    WindowToVulkanHandler::WindowToVulkanHandler(std::weak_ptr<RendererVK::VulkanCore> _vulkanCoreRef, Platform::Window& _windowRef, VkClearColorValue _clearColour, bool _renderImGui) :
        m_vulkanCoreRef(_vulkanCoreRef), m_windowRef(_windowRef), m_clearColour(_clearColour), m_renderImGui(_renderImGui)
    {
        createSurface();

        auto VkCore = m_vulkanCoreRef.lock();
        VkCore->physicalDevices().querySurfaceProperties(m_surface);
        VkCore->selectDevicesForSurface(m_surface);

        int fbW = 0, fbH = 0;
        m_windowRef.frameBufferSize(fbW, fbH);
        m_swapChain.createSwapChain(fbW, fbH);
        m_swapChain.createDepthResources();
        m_swapChain.initImageLayoutsForDynamicRendering();

        // Create frame data sync objects
        m_windowQueueHelper.initialize(&VkCore->graphicsQueue(), &VkCore->presentQueue(), VkCore->device());
        m_windowQueueHelper.createFrameSyncObjects(FRAMES_IN_FLIGHT, static_cast<uint32_t>(m_swapChain.numImages()));

        m_uniformBuffer.createUniformBuffers(static_cast<uint32_t>(m_swapChain.numImages()));

        // Bindless resource set: owns descriptor pool/layout/sets + writes UBO + mesh slots
        m_bindlessSet.initialize(
            m_vulkanCoreRef,
            m_swapChain,
            m_uniformBuffer,
            &m_meshesToDraw,
            m_windowRef.title().data()
        );

        // Pipeline layout must be created from the bindless set layout
        m_graphicsPipeline.setResourceLayout(m_bindlessSet.layout(), m_bindlessSet.layoutHash());

        m_graphicsPipeline.createGraphicsPipeline();

        m_indirectRenderingHelper.initialize();

        m_vulkanCommandBuffers.createCommandPool();

        if (m_renderImGui) {
            m_vulkanCommandBuffers.createCommandBuffers(m_swapChain.numImages(), m_vulkanCommandBuffers.commandBuffersWithGUI());
        }
        m_vulkanCommandBuffers.createCommandBuffers(m_swapChain.numImages(), m_vulkanCommandBuffers.commandBuffersWithoutGUI());

        m_vulkanCommandBuffers.createCopyCommandBuffer();
        m_vulkanCommandBuffers.recordCommandBuffers(_clearColour);
    }

    WindowToVulkanHandler::~WindowToVulkanHandler()
    {
        if (m_vulkanCoreRef.expired()) { MARK_FATAL(Utils::Category::Vulkan, "VulkanCore reference expired, cannot destroy surface"); }
        auto VkCore = m_vulkanCoreRef.lock();

        // Ensure GPU finished with this window's work before destroying resources
        VkCore->graphicsQueue().waitIdle();
        VkCore->presentQueue().waitIdle();

        // Destroy frame data sync objects
        m_windowQueueHelper.destroyFrameSyncObjects();

        // Destroy command buffers and pool
        m_vulkanCommandBuffers.destroyCommandBuffers();

        // Destroy bindless descriptor set (pool/sets/layout)
        m_bindlessSet.destroy(VkCore->device());

        // Destroy uniform buffers
        m_uniformBuffer.destroyUniformBuffers(VkCore->device());

        // Destroy indirect draw buffers
        m_indirectRenderingHelper.destroy(VkCore->device());

        // Destroy graphics pipeline
        m_graphicsPipeline.destroyGraphicsPipeline();

        // Explicitly destroy swap chain before surface
        m_swapChain.destroySwapChain();

        if (m_surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(VkCore->instance(), m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
            MARK_INFO(Utils::Category::Vulkan, "GLFW Window Surface Destroyed");
        }
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
        if (!VkCore) { MARK_FATAL(Utils::Category::Vulkan, "VulkanCore expired during renderFrame()"); }

        uint32_t imageIndex = m_windowQueueHelper.acquireNextImage(m_swapChain.swapChain());

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

        m_uniformBuffer.updateUniformBuffer(imageIndex, tempData);

        // Submit the command buffer for this image
        if (m_renderImGui && VkCore->imguiHandler().showGUI()) {
            VkCommandBuffer imguiCmdBuffer = VkCore->imguiHandler().prepareCommandBuffer(imageIndex);
            VkCommandBuffer cmdBuffers[] = { m_vulkanCommandBuffers.commandBufferWithGUI(imageIndex), imguiCmdBuffer };

            m_windowQueueHelper.submitAsync(imageIndex, cmdBuffers, 2);
        }
        else {
            VkCommandBuffer cmdBuffer = m_vulkanCommandBuffers.commandBufferWithoutGUI(imageIndex);
            m_windowQueueHelper.submitAsync(imageIndex , &cmdBuffer, 1);
        }

        m_windowQueueHelper.present(m_swapChain.swapChain(), imageIndex);
    }

    void WindowToVulkanHandler::rebuildRendererResources()
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) { MARK_FATAL(Utils::Category::Vulkan, "VulkanCore expired in rebuildRendererResources"); }

        VkCore->graphicsQueue().waitIdle();
        VkCore->presentQueue().waitIdle();
        m_windowRef.waitUntilFramebufferValid();

        // Re-query surface properties
        VkCore->physicalDevices().querySurfaceProperties(m_surface);

        // Swapchain
        int fbW = 0, fbH = 0;
        m_windowRef.frameBufferSize(fbW, fbH);
        m_swapChain.recreateSwapChain(fbW, fbH);
        m_swapChain.createDepthResources();
        m_swapChain.initImageLayoutsForDynamicRendering();

        // Re-create frame data sync objects 
        m_windowQueueHelper.destroyFrameSyncObjects();
        m_windowQueueHelper.createFrameSyncObjects(FRAMES_IN_FLIGHT, static_cast<uint32_t>(m_swapChain.numImages()));

        // Uniform buffers
        m_uniformBuffer.destroyUniformBuffers(m_vulkanCoreRef.lock()->device());
        m_uniformBuffer.createUniformBuffers(m_swapChain.numImages());

        // Bindless resource set: recreate pool+sets for new swapchain image count and rewrite descriptors
        m_bindlessSet.recreateForSwapchain(m_swapChain, m_uniformBuffer, &m_meshesToDraw);
        
        // Refresh pipeline acquisition
        m_graphicsPipeline.setResourceLayout(m_bindlessSet.layout(), m_bindlessSet.layoutHash());
        m_graphicsPipeline.createGraphicsPipeline();

        // Command buffers
        m_vulkanCommandBuffers.destroyCommandBuffers();
        m_vulkanCommandBuffers.createCommandPool();
        m_vulkanCommandBuffers.createCommandBuffers(m_swapChain.numImages(), m_vulkanCommandBuffers.commandBuffersWithGUI());
        m_vulkanCommandBuffers.createCommandBuffers(m_swapChain.numImages(), m_vulkanCommandBuffers.commandBuffersWithoutGUI());
        m_vulkanCommandBuffers.createCopyCommandBuffer();

        // Indirect rendering buffers
        m_vulkanCommandBuffers.setIndirectDrawBuffers(m_indirectRenderingHelper.indirectCmdBuffer(), m_indirectRenderingHelper.indirectCountBuffer(), m_indirectRenderingHelper.maxDraws());

        m_vulkanCommandBuffers.recordCommandBuffers(m_clearColour);
    }

    void WindowToVulkanHandler::createSurface()
    {
        if (m_surface != VK_NULL_HANDLE) return;
        if (m_vulkanCoreRef.expired())
        {
            MARK_FATAL(Utils::Category::Vulkan, "VulkanCore reference expired, cannot create surface");
        }

        VkResult res = glfwCreateWindowSurface(m_vulkanCoreRef.lock()->instance(), m_windowRef.handle(), nullptr, &m_surface);
        CHECK_VK_RESULT(res, "Create window surface");
        MARK_VK_NAME(m_vulkanCoreRef.lock()->device(), VK_OBJECT_TYPE_SURFACE_KHR, m_surface, "WinToVulk.WinSurface");

        MARK_INFO(Utils::Category::Vulkan, "GLFW Window Surface Created");
    }

    void WindowToVulkanHandler::setMeshVisible(uint32_t _meshIndex, bool _visible)
    {
        m_indirectRenderingHelper.setMeshVisible(m_meshesToDraw, _meshIndex, _visible);
    }

    void WindowToVulkanHandler::removeMesh(uint32_t _meshIndex)
    {
        if (_meshIndex >= m_meshesToDraw.size()) return;
        
        // TODO: PROPER HANDLING OF REMOVING MESHES
        setMeshVisible(_meshIndex, false);
    }

    std::weak_ptr<MeshHandler> WindowToVulkanHandler::addMesh(const char* _meshPath)
    {
        auto rtn = std::make_shared<MeshHandler>(m_vulkanCoreRef, m_vulkanCommandBuffers);

        const auto assetPath = m_vulkanCoreRef.lock()->assetPath(_meshPath);
        rtn->loadFromOBJ(assetPath.string().c_str(), true/*Flip texture vertically for Vulkan*/);
        rtn->uploadToGPU();

        m_meshesToDraw.push_back(rtn);
        const uint32_t newMeshIndex = static_cast<uint32_t>(m_meshesToDraw.size() - 1);

        // Wait for GPU to finish before updating buffers
        m_vulkanCoreRef.lock()->graphicsQueue().waitIdle();

        if (!m_bindlessSet.tryWriteMeshSlot(m_swapChain, m_uniformBuffer, newMeshIndex, *rtn)) {
            m_bindlessSet.recreateForSwapchain(m_swapChain, m_uniformBuffer, &m_meshesToDraw);
        }

        // Update indirect draw commands with new mesh
        m_indirectRenderingHelper.handleDrawCommands(m_meshesToDraw, newMeshIndex);

        // Re-record command buffers to bind the new descriptor set handles
        m_vulkanCommandBuffers.recordCommandBuffers(m_clearColour);

        return rtn;
    }
} // namespace Mark::RendererVK