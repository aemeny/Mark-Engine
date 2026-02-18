#pragma once
#include "Mark_VulkanCommandBuffers.h"
#include "Mark_VulkanWindowQueueHelper.h"

#include <array>
#include "Engine/EarlyCameraController.h" // TEMP

struct GLFWwindow;
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

        // TEMP list of meshes to render for this window
        std::vector<std::shared_ptr<MeshHandler>> m_meshesToDraw;
        // TEMP camera controller for testing
        std::shared_ptr<Systems::EarlyCameraController> m_cameraController;

        VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
        VulkanSwapChain m_swapChain{ m_vulkanCoreRef, m_surface };
        VulkanUniformBuffer m_uniformBuffer{ m_vulkanCoreRef };
        VulkanGraphicsPipeline m_graphicsPipeline{ m_vulkanCoreRef, m_swapChain, m_uniformBuffer, &m_meshesToDraw };
        VulkanCommandBuffers m_vulkanCommandBuffers{ m_vulkanCoreRef, m_swapChain, m_graphicsPipeline };
        VulkanWindowQueueHelper m_windowQueueHelper;
    };
} // namespace Mark::RendererVK