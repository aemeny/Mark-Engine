#include "SettingsHandler.h"

#include "Platform/Window.h"

#include <imgui.h>
#include <GLFW/glfw3.h>

namespace Mark::Settings
{
    MarkSettings::MarkSettings(Platform::Window& _mainWindow) :
        m_mainWindowRef(_mainWindow)
    {}

    const char* MarkSettings::KeyNameForDisplay(int _key)
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
        if (!m_showUISettings)
            return;

        if (ImGui::Begin("UI Settings", &m_showUISettings))
        {
            // Input for rebinding the toggle GUI key
            ImGui::SeparatorText("Input");

            ImGui::Text("Toggle GUI key:");
            ImGui::SameLine();
            ImGui::Text("[%s]", KeyNameForDisplay(m_toggleGuiKey));

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
                    if (glfwGetKey(m_mainWindowRef.handle(), key) == GLFW_PRESS)
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

            ImGui::Spacing();
            ImGui::SeparatorText("FPS Toggle");

            ImGui::Text("Display FPS in window title:");
            ImGui::SameLine();
            ImGui::Checkbox(" ", &displayFPSInTitleBar);
        }
        ImGui::End();
    }
}