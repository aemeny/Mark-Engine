#include "EngineStats.h"
#include "Platform/imguiHandler.h"
#include "Engine/SettingsHandler.h"
#include "Platform/Window.h"

namespace Mark
{
    void EngineStats::initialize(Platform::Window& _mainWindowRef)
    {
        m_markSettings = &Settings::MarkSettings::Get();
        m_mainWindowRef = &_mainWindowRef;
        m_fpsUpdateInterval = m_markSettings->getFpsUpdateInterval();

        reset();
        Platform::ImGuiHandler::registerWindow(
            "Engine Stats",
            [this]()
            {
                drawGUI();
            },
            &m_guiWindowOpen);
    }

    void EngineStats::drawGUI() const
    {
        if (!m_guiWindowOpen)
            return;

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
        m_displayFpsThisFrame = false;
    }

    void EngineStats::update(double _deltaTime)
    {
        m_displayFpsThisFrame = false;

        addFrameTime(_deltaTime); // Controls counting of frames and when to update FPS display

        if (m_markSettings->shouldDisplayFPSInTitleBar()) {
            displayFpsInTitleBar();
        }
        else {
            // If the title bar was changed, reset it to default
            if (m_canResetTitleBar) {
                m_mainWindowRef->setTitle("");
                m_canResetTitleBar = false;
            }
        }

    }

    void EngineStats::addFrameTime(double _deltaTime)
    {
        if (_deltaTime <= 0.0)
            return;
        if (!m_guiWindowOpen) {
            reset();
            return;
        }

        float updateInterval = m_markSettings->getFpsUpdateInterval();
        if (m_fpsUpdateInterval != updateInterval) 
        {
            m_fpsUpdateInterval = updateInterval;
            reset();
        }

        if (updateInterval < 0.01f)
        {
            m_displayFps = 1.0 / _deltaTime;
            m_hasValidFps = true;
            return;
        }

        m_accumTime += _deltaTime;
        m_accumFrames++;

        if (m_accumTime >= static_cast<double>(updateInterval))
        {
            if (m_accumFrames > 0)
            {
                m_displayFps = static_cast<double>(m_accumFrames) / m_accumTime;
                m_hasValidFps = true;
                m_displayFpsThisFrame = true;
            }

            m_accumTime = 0.0;
            m_accumFrames = 0;
        }
    }

    void EngineStats::displayFpsInTitleBar()
    {
        if (!m_displayFpsThisFrame) {
            return;
        }

        char titleFPS[128];

        if (!m_hasValidFps) {
            std::snprintf(titleFPS, sizeof(titleFPS), "FPS: recomputing...");
        }
        else if (m_displayFps > 0.0) {
            std::snprintf(titleFPS, sizeof(titleFPS), "- %.1f FPS", m_displayFps);
        }
        else {
            std::snprintf(titleFPS, sizeof(titleFPS), "- (FPS: ...)");
        }

        m_mainWindowRef->setTitle(titleFPS);

        // Now title has been set to show FPS, we can allow it to be reset to default on the next frame if needed
        m_canResetTitleBar = true;
    }
}