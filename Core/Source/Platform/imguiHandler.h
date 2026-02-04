#pragma once
#include "Renderer/Mark_imguiRenderer.h"

#include <glm/glm.hpp>

namespace Mark::RendererVK 
{
    struct VulkanCore;
    struct WindowToVulkanHandler;
}
namespace Mark::Platform
{
    using namespace ::Mark::RendererVK;
    struct ImGuiHandler
    {
        ImGuiHandler() = default;
        ~ImGuiHandler() = default;
        ImGuiHandler(const ImGuiHandler&) = delete;
        ImGuiHandler& operator=(const ImGuiHandler&) = delete;

        void initialize(const Settings::ImGuiSettings& _settings, WindowToVulkanHandler* _mainWindowHandler, VulkanCore* _vulkanCoreRef);
        void destroy();

        void updateGUI();

        bool showGUI() const { return m_showGUI; }
        void setShowGUI(bool _show) { m_showGUI = _show; }

        VkCommandBuffer prepareCommandBuffer(uint32_t _imageIndex) {
            return m_imguiRenderer.prepareCommandBuffer(_imageIndex);
        }

    private:
        // References
        friend ImGuiRenderer;
        VulkanCore* m_vulkanCoreRef{ nullptr };
        WindowToVulkanHandler* m_mainWindowHandler{ nullptr };
        const Settings::ImGuiSettings* m_ImGuiSettings{ nullptr };

        // Renderer
        ImGuiRenderer m_imguiRenderer{ *this };

        bool m_showGUI{ true };
        glm::vec3 m_clearColour{ 1.0f };

        glm::vec3 m_position{ 0.0f };
        glm::vec3 m_rotation{ 0.0f };
        float m_scale{ 1.0f };
    };
}