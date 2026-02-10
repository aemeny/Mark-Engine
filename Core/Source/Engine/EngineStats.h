#pragma once
#include <cstdint>

namespace Mark
{
    struct EngineStats 
    {
        EngineStats();

        void reset();

        // Called once per frame
        void addFrameTime(double _deltaSeconds);

    private:
        float m_fpsUpdateInterval = 1.0f;

        double m_accumTime = 0.0;
        uint64_t m_accumFrames = 0;

        double m_displayFps = 0.0;
        bool m_hasValidFps = false;

        // For GUI
        void drawGUI();
        bool m_guiWindowOpen{ true };
    };
}