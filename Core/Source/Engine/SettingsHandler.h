#pragma once
#include "Platform/imguiHandler.h"

namespace Mark::Platform { struct Window; }
namespace Mark::Settings
{
    struct MarkSettings
    {
        MarkSettings(Platform::Window& _mainWindow);
        ~MarkSettings() = default;
        MarkSettings(const MarkSettings&) = delete;
        MarkSettings& operator=(const MarkSettings&) = delete;

        float getFpsUpdateInterval() const { return m_fpsUpdateInterval; }
        void setFpsUpdateInterval(float _interval) { m_fpsUpdateInterval = _interval; }

        bool shouldDisplayFPSInTitleBar() const { return displayFPSInTitleBar; }

    private:
        friend Platform::ImGuiHandler;
        Platform::Window& m_mainWindowRef;

        bool m_showUISettings{ false };
        void drawSettingsUI();
        void toggleUISettings() {
            m_showUISettings = !m_showUISettings;
            m_rebindingGuiKey = false;
        }

        /* Input */
        void pollToggleGUIShortCut();
        const char* KeyNameForDisplay(int _key);
        bool IsModifierKey(int _key);
        int m_toggleGuiKey { 297 }; // GLFW_KEY_F8
        bool m_toggleKeyDownLastFrame = false;
        bool m_rebindingGuiKey = false;

        /* FPS Tracking */
        bool displayFPSInTitleBar{ false };
        float m_fpsUpdateInterval = 1.0f;
    };
}