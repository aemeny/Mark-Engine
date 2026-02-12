#include "SettingsHandler.h"

#include "Platform/Window.h"

#include <imgui.h>
#include <GLFW/glfw3.h>

namespace Mark::Settings
{
    const char* MarkSettings::getKeyName(int _key) const
    {
        if (!m_toggleGuiKey)
            return "Not Set";

        // Try GLFW’s name first
        if (const char* n = glfwGetKeyName(_key, 0))
            return n;

        switch (_key)
        {
        case GLFW_KEY_F1:  return "F1";
        case GLFW_KEY_F2:  return "F2";
        case GLFW_KEY_F3:  return "F3";
        case GLFW_KEY_F4:  return "F4";
        case GLFW_KEY_F5:  return "F5";
        case GLFW_KEY_F6:  return "F6";
        case GLFW_KEY_F7:  return "F7";
        case GLFW_KEY_F8:  return "F8";
        case GLFW_KEY_F9:  return "F9";
        case GLFW_KEY_F10: return "F10";
        case GLFW_KEY_F11: return "F11";
        case GLFW_KEY_F12: return "F12";
        default:           return "Unknown";
        }
    }

    bool MarkSettings::IsModifierKey(int _key)
    {
        return _key == GLFW_KEY_LEFT_SHIFT || _key == GLFW_KEY_RIGHT_SHIFT ||
               _key == GLFW_KEY_LEFT_CONTROL || _key == GLFW_KEY_RIGHT_CONTROL ||
               _key == GLFW_KEY_LEFT_ALT || _key == GLFW_KEY_RIGHT_ALT ||
               _key == GLFW_KEY_LEFT_SUPER || _key == GLFW_KEY_RIGHT_SUPER;
    }

    void MarkSettings::drawSettingsUI()
    {
        if (!m_showSettings)
            return;

        ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);

        const ImVec4 settingsBg = ImVec4(0.08f, 0.08f, 0.08f, 1.0f); // Black
        if (ImGui::Begin("Project Setttings", &m_showSettings))
        {
            // ---- Left: sidebar with category buttons ----
            ImGui::PushStyleColor(ImGuiCol_ChildBg, settingsBg);
            ImGui::BeginChild("##SettingsSidebar", ImVec2(170.0f, 0.0f), true);

            const int numPanels = static_cast<int>(SettingsPanels::Count);
            for (int i = 0; i < numPanels; i++)
            {
                SettingsPanels panel = static_cast<SettingsPanels>(i);

                const char* label = "";
                switch (panel)
                {
                default:
                case SettingsPanels::UI:        label = "UI";        break;
                case SettingsPanels::Rendering: label = "Rendering"; break;
                }

                bool selected = (m_activePanel == panel);
                if (ImGui::Selectable(label, selected))
                {
                    m_activePanel = panel;
                }
            }

            ImGui::EndChild();
            ImGui::SameLine();

            // ---- Right: panel content ----
            ImGui::BeginChild("##SettingsContent", ImVec2(0.0f, 0.0f), false);

            switch (m_activePanel)
            {
            default:
            case SettingsPanels::UI:
                drawUISettingsPanel();
                break;

            case SettingsPanels::Rendering:
                drawRenderingSettingsPanel();
                break;
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        ImGui::End();
    }

    void MarkSettings::drawUISettingsPanel()
    {
        // Input for rebinding the toggle GUI key
        ImGui::SeparatorText("Input");

        ImGui::Text("Toggle GUI key:");
        ImGui::SameLine();
        ImGui::Text("[%s]", getKeyName(m_toggleGuiKey));

        ImGui::SameLine();
        if (!m_rebindingGuiKey)
        {
            if (ImGui::Button("Rebind")) {
                m_rebindingGuiKey = true;
            }
        }
        else
        {
            ImGui::SameLine();
            ImGui::TextUnformatted("Press a key...");

            for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; key++)
            {
                if (glfwGetKey(m_mainWindowHandle, key) == GLFW_PRESS)
                {
                    if (!IsModifierKey(key)) {
                        m_toggleGuiKey = key;
                    }

                    m_rebindingGuiKey = false;
                    m_toggleKeyDownLastFrame = true;
                    break;
                }
            }
        }

        // Tick box for displaying FPS in title bar
        ImGui::Spacing();
        ImGui::SeparatorText("FPS Tracking");

        ImGui::Text("Display FPS in window title:");
        ImGui::SameLine();
        ImGui::Checkbox("##ShowFPSInTitle", &displayFPSInTitleBar);

        // Slider to control how often FPS is recomputed
        ImGui::Spacing();

        float prevPeriod = m_fpsUpdateInterval;
        ImGui::Text("Update Interval");
        ImGui::SliderFloat("##FpsInterval", &m_fpsUpdateInterval,
            0.0f, 10.0f, "%.1f s");

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Shorter updates cost more CPU usage and can make the FPS number flicker.");
        }
    }

    void MarkSettings::drawRenderingSettingsPanel()
    {
        ImGui::SeparatorText("Rendering");

        ImGui::Text("Run Mark in performance mode:");
        ImGui::SameLine();
        const bool prev = m_runInPerformanceMode;
        if (ImGui::Checkbox("##PerformanceModeToggle", &m_runInPerformanceMode)) 
        {
            if (prev != m_runInPerformanceMode) {
                m_requestSwapchainRebuild = true;
            }
        }
    }
}