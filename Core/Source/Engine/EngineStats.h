#pragma once
#include <cstdint>

namespace Mark
{
    namespace Settings { struct MarkSettings; }
    namespace Platform { struct Window; }
    struct EngineStats 
    {
        EngineStats() = default;

        void initialize(Settings::MarkSettings& _markSettings, Platform::Window& _mainWindowRef);
        void reset();
        void update(double _deltaTime);

    private:
        Settings::MarkSettings* m_markSettings{ nullptr };
        Platform::Window* m_mainWindowRef{ nullptr };

        float m_fpsUpdateInterval; // Held and updated through engine settings
        double m_accumTime = 0.0;
        uint64_t m_accumFrames = 0;
        double m_displayFps = 0.0;
        bool m_hasValidFps = false;

        // Called once per frame to accumulate time and frames
        void addFrameTime(double _deltaTime);

        void displayFpsInTitleBar();
        bool m_displayFpsThisFrame{ false };
        bool m_canResetTitleBar{ false };

        // For GUI
        void drawGUI();
        bool m_guiWindowOpen{ true };
    };
}