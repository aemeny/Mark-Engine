#include "EngineStats.h"
#include "Platform/imguiHandler.h"

namespace Mark
{
    EngineStats::EngineStats()
    {
        Platform::ImGuiHandler::registerWindow(
            "Engine Stats",
            [this]()
            {
                drawGUI();
            },
            &m_guiWindowOpen);
    }

    void EngineStats::drawGUI()
    {
        if (!m_guiWindowOpen)
            return;

        // Slider to control how often FPS is recomputed
        float prevPeriod = m_fpsUpdateInterval;
        ImGui::Text("Update Interval");
        ImGui::SliderFloat(" ", &m_fpsUpdateInterval,
            0.0f, 10.0f, "%.1f s");

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Shorter updates cost more CPU usage and can make the FPS number flicker.");
        }
        if (m_fpsUpdateInterval != prevPeriod) {
            reset();
        }

        ImGui::Spacing();

        // Display FPS
        if (m_hasValidFps)
            ImGui::Text("FPS: %.1f", m_displayFps);
        else
            ImGui::TextDisabled("FPS: recomputing...");
    }

    void EngineStats::reset()
    {
        m_accumTime = 0.0;
        m_accumFrames = 0;
        m_displayFps = 0.0;
        m_hasValidFps = false;
    }

    void EngineStats::addFrameTime(double _deltaSeconds)
    {
        if (_deltaSeconds <= 0.0)
            return;
        if (!m_guiWindowOpen) {
            reset();
            return;
        }

        if (m_fpsUpdateInterval < 0.01f)
        {
            m_displayFps = 1.0 / _deltaSeconds;
            m_hasValidFps = true;
            return;
        }

        m_accumTime += _deltaSeconds;
        m_accumFrames++;

        if (m_accumTime >= static_cast<double>(m_fpsUpdateInterval))
        {
            if (m_accumFrames > 0)
            {
                m_displayFps = static_cast<double>(m_accumFrames) / m_accumTime;
                m_hasValidFps = true;
            }

            m_accumTime = 0.0;
            m_accumFrames = 0;
        }
    }
}