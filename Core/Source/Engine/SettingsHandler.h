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

    private:
        friend Platform::ImGuiHandler;
        Platform::Window& m_mainWindowRef;

        bool m_showUISettings{ false };
        void drawSettingsUI();
        void toggleUISettings() {
            m_showUISettings = !m_showUISettings;
            m_rebindingGuiKey = false;
        }

        const char* KeyNameForDisplay(int _key);
        bool IsModifierKey(int _key);

        /* Input */
        void pollToggleGUIShortCut();
        int m_toggleGuiKey { NULL };
        bool m_toggleKeyDownLastFrame = false;
        bool m_rebindingGuiKey = false;

        /* FPS Toggle */
        bool displayFPSInTitleBar{ false };
    };
}