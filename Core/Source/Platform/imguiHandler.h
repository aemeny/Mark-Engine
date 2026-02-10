#pragma once
#include "Renderer/Mark_imguiRenderer.h"

#include <string>
#include <functional>

#include <glm/glm.hpp>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

// Forward declarations
struct ImFont;
namespace Mark::RendererVK {
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

        /* Handling ImGui setup and rendering */
        void initialize(const Settings::ImGuiSettings& _settings, WindowToVulkanHandler* _mainWindowHandler, VulkanCore* _vulkanCoreRef);
        void destroy();

        void updateGUI();

        bool showGUI() const { return m_showGUI; }
        // Show all Mark GUI. With false game scene may see increased performance
        void setShowGUI(bool _show) { m_showGUI = _show; }

        VkCommandBuffer prepareCommandBuffer(uint32_t _imageIndex) {
            return m_imguiRenderer.prepareCommandBuffer(_imageIndex);
        }

        
        /* Handling adding windows */
        using DrawFunction = std::function<void()>;

        struct GUIWindow
        {
            std::string title;
            DrawFunction drawFunction;
            // Whether this GUIWindow can be open/closed independantly. nullptr = always open
            bool* isOpen;
        };

        static void registerWindow(const std::string& _title, DrawFunction _drawFunction, bool* _isOpen = nullptr)
        {
            m_GUIWindows.push_back(GUIWindow{_title, _drawFunction, _isOpen});
        }

    private:
        // References
        VulkanCore* m_vulkanCoreRef{ nullptr };
        WindowToVulkanHandler* m_mainWindowHandler{ nullptr };
        const Settings::ImGuiSettings* m_ImGuiSettings{ nullptr };

        // Renderer
        friend ImGuiRenderer;
        ImGuiRenderer m_imguiRenderer{ *this };

        // Show all Mark GUI. With false game scene may see increased performance
        bool m_showGUI{ true };

        // Handles UI
        void setScreenDocking();
        void handleUISettings();
        void applyTheme();

        void drawMainMenuBar();
        void setMainMenuStyle() const;
        void drawMainToolbar() const;
        void drawDockSpace();

        /* Handling adding windows */
        inline static std::vector<GUIWindow> m_GUIWindows{};

        // Font handling
        struct MarkFonts
        {
            ImFont* body = nullptr;
            ImFont* menu = nullptr;
            ImFont* bold = nullptr;
        };
        MarkFonts m_fonts;


        // TEMP TEST VARIABLES
        bool m_rendererWindowOpen{ true };
        bool m_rendererWindowOpen2{ true };
        float m_rendererFPS{ 100.0f };
        float m_rendererExposure{ 1.0f };
        bool m_rendererVSync{ true };
    };
}