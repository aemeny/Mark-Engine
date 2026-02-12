#pragma once
#include "Platform/imguiHandler.h"

// Forward declarations
struct GLFWwindow;
namespace Mark {
    struct Core;
    namespace Platform{ 
        struct Window; 
    }
}
namespace Mark::Settings
{
    struct MarkSettings
    {
        static MarkSettings& Get()
        {
            static MarkSettings instance;
            return instance;
        }

        /* ---- UI Settings ---- */
        float getFpsUpdateInterval() const { return m_fpsUpdateInterval; }
        void setFpsUpdateInterval(float _interval) { m_fpsUpdateInterval = _interval; }

        bool shouldDisplayFPSInTitleBar() const { return displayFPSInTitleBar; }

        /* ---- Rendering Settings ---- */
        bool isInPerformanceMode() const { return m_runInPerformanceMode; }
        bool requestSwapchainRebuild() const { return m_requestSwapchainRebuild; } 
        void acknowledgeSwapchainRebuildRequest() { m_requestSwapchainRebuild = false; }

    private:
        // Private constructor to prevent instantiation outside of Get()
        MarkSettings() = default;

        // Allow ImGuiHandler to access private members for settings UI
        friend Platform::ImGuiHandler;
        // Allows Core to call upon initialize to link the main window reference
        friend Core;
        void initialize(GLFWwindow* _mainWindowHandle) { m_mainWindowHandle = _mainWindowHandle; }

        // Reference to main window for retreiving GLFW Key press state
        GLFWwindow* m_mainWindowHandle{ nullptr };


        /* ---- Settings Window ---- */
        bool m_showSettings{ false };
        void toggleSettingsWindow() {
            m_showSettings = !m_showSettings;
            m_rebindingGuiKey = false;
        }
        void drawSettingsUI();
        enum class SettingsPanels
        {
            UI,
            Rendering,
            Count
        };
        SettingsPanels m_activePanel{ SettingsPanels::UI };


        /* ---- UI Settings ---- */
        void drawUISettingsPanel();
        /* Input */
        void pollToggleGUIShortCut();
        const char* getKeyName(int _key) const;
        bool IsModifierKey(int _key);
        int m_toggleGuiKey { 297 }; // GLFW_KEY_F8
        bool m_toggleKeyDownLastFrame{ false };
        bool m_rebindingGuiKey{ false };

        /* FPS Tracking */
        bool displayFPSInTitleBar{ false };
        float m_fpsUpdateInterval{ 1.0f };


        /* ---- Rendering Settings ---- */
        void drawRenderingSettingsPanel();
        // Raised when a setting requires swapchain recreation
        bool m_requestSwapchainRebuild{ false };
        // Changes from Mailbox to Immediate presentation mode
        bool m_runInPerformanceMode{ false }; 
    };
}