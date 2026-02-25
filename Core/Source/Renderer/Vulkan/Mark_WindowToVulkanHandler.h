#pragma once
#include "Mark_CommandBuffers.h"
#include "Mark_WindowQueueHelper.h"
#include "Mark_IndirectRenderingHelper.h"
#include "Mark_UniformBuffer.h"

#include "Engine/EarlyCameraController.h" // TEMP

namespace Mark::Platform { struct Window; struct ImGuiHandler; }
namespace Mark::RendererVK
{
    struct WindowToVulkanHandler
    {
        WindowToVulkanHandler(std::weak_ptr<VulkanCore> _vulkanCoreRef, Platform::Window& _windowRef, VkClearColorValue _clearColour, bool _renderImGui = false);
        ~WindowToVulkanHandler();
        WindowToVulkanHandler(const WindowToVulkanHandler&) = delete;
        WindowToVulkanHandler& operator=(const WindowToVulkanHandler&) = delete;

        void renderToWindow();
        void rebuildRendererResources();

        void createSurface();
        VkSurfaceKHR surface() const { return m_surface; }

        void setMeshVisible(uint32_t _meshIndex, bool _visible);
        void removeMesh(uint32_t _meshIndex);

        // TEMP FOR TESTING
        std::weak_ptr<MeshHandler> addMesh(const char* _meshPath);
        void initCameraController();

    private:
        friend struct ImGuiRenderer; // Allows access to main windows info for ImGui init
        friend Platform::ImGuiHandler;

        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        Platform::Window& m_windowRef;
        VkClearColorValue m_clearColour{};

        bool m_renderImGui{ false }; // MainwindowMay render ImGui, seperate windows should always be false

        static constexpr uint32_t FRAMES_IN_FLIGHT = 3;

        // TEMP list of meshes to render for this window
        std::vector<std::shared_ptr<MeshHandler>> m_meshesToDraw;
        // TEMP camera controller for testing
        std::shared_ptr<Systems::EarlyCameraController> m_cameraController;

        VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
        VulkanSwapChain m_swapChain{ m_vulkanCoreRef, m_surface };
        VulkanUniformBuffer m_uniformBuffer{ m_vulkanCoreRef };
        VulkanGraphicsPipeline m_graphicsPipeline{ m_vulkanCoreRef, m_swapChain };
        VulkanBindlessResourceSet m_bindlessSet;
        VulkanCommandBuffers m_vulkanCommandBuffers{ m_vulkanCoreRef, m_swapChain, m_graphicsPipeline,  m_bindlessSet };
        VulkanIndirectRenderingHelper m_indirectRenderingHelper{ m_vulkanCoreRef, m_vulkanCommandBuffers };
        VulkanWindowQueueHelper m_windowQueueHelper;
    };
} // namespace Mark::RendererVK