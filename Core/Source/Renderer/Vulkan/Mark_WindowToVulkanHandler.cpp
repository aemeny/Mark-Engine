#include "Mark_WindowToVulkanHandler.h"
#include "Mark_VulkanCore.h"
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

        m_graphicsPipeline.createGraphicsPipeline();

        createIndirectDrawBuffers();

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

        // Destroy uniform buffers
        m_uniformBuffer.destroyUniformBuffers(VkCore->device());

        // Destroy indirect draw buffers
        destroyIndirectDrawBuffers(VkCore->device());

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

    void WindowToVulkanHandler::createIndirectDrawBuffers()
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) { MARK_FATAL(Utils::Category::Vulkan, "VulkanCore expired in createIndirectDrawBuffers"); }

        // Cap draw count by both bindless mesh cap and device maxDrawIndirectCount.
        const uint32_t maxMeshes = VkCore->bindlessCaps().maxMeshes;
        const uint32_t maxIndirect = VkCore->bindlessCaps().maxDrawIndirectCount;
        m_maxDraws = (maxIndirect > 0) ? std::min(maxMeshes, maxIndirect) : maxMeshes;
        if (m_maxDraws == 0) m_maxDraws = 1;

        const VkDeviceSize cmdBytes = sizeof(VkDrawIndirectCommand) * static_cast<VkDeviceSize>(m_maxDraws);

        // Host-visible, coherent updates (1024 draws = 16KB)
        m_indirectCmdBuffer = BufferAndMemory(
            VkCore,
            cmdBytes,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            "WinToVulk.IndirectCmdBuffer"
        );

        m_indirectCountBuffer = BufferAndMemory(
            VkCore,
            sizeof(uint32_t),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            "WinToVulk.IndirectCountBuffer"
        );

        m_drawsCPU.assign(m_maxDraws, VkDrawIndirectCommand{ 0, 0, 0, 0 });
        m_drawCount = 0;
        m_meshVisible.clear();

        // Upload initial empty contents
        m_indirectCmdBuffer.update(VkCore->device(), m_drawsCPU.data(), sizeof(VkDrawIndirectCommand) * static_cast<size_t>(m_maxDraws));
        m_indirectCountBuffer.update(VkCore->device(), &m_drawCount, sizeof(uint32_t));

        // Let the command buffer know what to use
        m_vulkanCommandBuffers.setIndirectDrawBuffers(m_indirectCmdBuffer.m_buffer, m_indirectCountBuffer.m_buffer, m_maxDraws);
    }

    void WindowToVulkanHandler::destroyIndirectDrawBuffers(VkDevice _device)
    {
        m_indirectCmdBuffer.destroy(_device);
        m_indirectCountBuffer.destroy(_device);
        m_drawsCPU.clear();
        m_meshVisible.clear();
        m_maxDraws = 0;
        m_drawCount = 0;
    }

    void WindowToVulkanHandler::buildDrawCommandCPU(uint32_t _meshIndex)
    {
        if (_meshIndex >= m_maxDraws) return;
        if (_meshIndex >= m_meshesToDraw.size()) return;

        const bool visible = (_meshIndex < m_meshVisible.size()) ? (m_meshVisible[_meshIndex] != 0) : true;
        if (!visible || !m_meshesToDraw[_meshIndex]) {
            m_drawsCPU[_meshIndex] = VkDrawIndirectCommand{ 0, 0, 0, 0 };
            return;
        }

        const uint32_t indexCount = m_meshesToDraw[_meshIndex]->indexCount();
        if (indexCount == 0) {
            m_drawsCPU[_meshIndex] = VkDrawIndirectCommand{ 0, 0, 0, 0 };
            return;
        }

        // firstInstance encodes meshIndex -> gl_InstanceIndex
        m_drawsCPU[_meshIndex] = VkDrawIndirectCommand{
            .vertexCount = indexCount,
            .instanceCount = 1,
            .firstVertex = 0,
            .firstInstance = _meshIndex
        };
    }

    void WindowToVulkanHandler::uploadDrawCommand(uint32_t _meshIndex)
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) return;
        if (_meshIndex >= m_maxDraws) return;
        
        const VkDeviceSize stride = sizeof(VkDrawIndirectCommand);
        const VkDeviceSize offset = stride * static_cast<VkDeviceSize>(_meshIndex);
        m_indirectCmdBuffer.updateRange(VkCore->device(), &m_drawsCPU[_meshIndex], sizeof(VkDrawIndirectCommand), offset);
    }

    void WindowToVulkanHandler::uploadDrawCount()
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) return;

        const uint32_t meshCount = static_cast<uint32_t>(m_meshesToDraw.size());
        if (meshCount > m_maxDraws) {
            MARK_WARN(Utils::Category::Vulkan,
                "Mesh count (%u) exceeds indirect capacity (%u). Extra meshes will not be drawn.",
                meshCount, m_maxDraws);
        }

        m_drawCount = std::min(meshCount, m_maxDraws);
        m_indirectCountBuffer.update(VkCore->device(), &m_drawCount, sizeof(uint32_t));
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

        // Graphics pipeline
        m_graphicsPipeline.rebuildDescriptors();

        // Command buffers
        m_vulkanCommandBuffers.destroyCommandBuffers();
        m_vulkanCommandBuffers.createCommandPool();
        m_vulkanCommandBuffers.createCommandBuffers(m_swapChain.numImages(), m_vulkanCommandBuffers.commandBuffersWithGUI());
        m_vulkanCommandBuffers.createCommandBuffers(m_swapChain.numImages(), m_vulkanCommandBuffers.commandBuffersWithoutGUI());
        m_vulkanCommandBuffers.createCopyCommandBuffer();

        // Indirect rendering buffers
        m_vulkanCommandBuffers.setIndirectDrawBuffers(m_indirectCmdBuffer.m_buffer, m_indirectCountBuffer.m_buffer, m_maxDraws);

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
        if (_meshIndex >= m_meshesToDraw.size()) return;
        if (_meshIndex >= m_maxDraws) return;

        auto VkCore = m_vulkanCoreRef.lock();
        if (VkCore) {
            VkCore->graphicsQueue().waitIdle();
        }

        if (m_meshVisible.size() < m_meshesToDraw.size())
            m_meshVisible.resize(m_meshesToDraw.size(), 1);

        m_meshVisible[_meshIndex] = _visible ? 1 : 0;
        buildDrawCommandCPU(_meshIndex);
        uploadDrawCommand(_meshIndex);
    }

    void WindowToVulkanHandler::removeMesh(uint32_t _meshIndex)
    {
        if (_meshIndex >= m_meshesToDraw.size()) return;
        
        // "soft remove" = stop drawing it
        setMeshVisible(_meshIndex, false);
        
        // If last element pop it and shrink drawCount
        if (_meshIndex + 1 == m_meshesToDraw.size()) {
            m_meshesToDraw.pop_back();
            if (!m_meshVisible.empty()) m_meshVisible.pop_back();
            uploadDrawCount();
        }
    }

    std::weak_ptr<MeshHandler> WindowToVulkanHandler::addMesh(const char* _meshPath)
    {
        auto rtn = std::make_shared<MeshHandler>(m_vulkanCoreRef, m_vulkanCommandBuffers);

        const auto assetPath = m_vulkanCoreRef.lock()->assetPath(_meshPath);
        rtn->loadFromOBJ(assetPath.string().c_str(), true/*Flip texture vertically for Vulkan*/);
        rtn->uploadToGPU();

        m_meshesToDraw.push_back(rtn);

        auto VkCore = m_vulkanCoreRef.lock();
        if (VkCore) {
            VkCore->graphicsQueue().waitIdle();
        }

        const uint32_t newMeshIndex = static_cast<uint32_t>(m_meshesToDraw.size() - 1);

        const bool updatedInPlace = m_graphicsPipeline.tryUpdateDescriptorsWithMesh(newMeshIndex);
        if (!updatedInPlace) {
            m_graphicsPipeline.rebuildDescriptors();
        }

        // Ensure visibility tracking matches mesh list size
        if (m_meshVisible.size() < m_meshesToDraw.size())
            m_meshVisible.resize(m_meshesToDraw.size(), 1);
        
        buildDrawCommandCPU(newMeshIndex);
        uploadDrawCommand(newMeshIndex);
        uploadDrawCount();

        // Re-record command buffers to bind the new descriptor set handles
        m_vulkanCommandBuffers.recordCommandBuffers(m_clearColour);

        return rtn;
    }
} // namespace Mark::RendererVK