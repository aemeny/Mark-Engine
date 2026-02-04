#include "imguiHandler.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"

namespace Mark::Platform
{
    void ImGuiHandler::initialize(const Settings::ImGuiSettings& _settings, WindowToVulkanHandler* _mainWindowHandler, VulkanCore* _vulkanCoreRef)
    {
        m_ImGuiSettings = &_settings;
        m_mainWindowHandler = _mainWindowHandler;
        m_vulkanCoreRef = _vulkanCoreRef;

        ImGui::CreateContext();

        m_imguiRenderer.initialize();
    }

    void ImGuiHandler::destroy()
    {
        m_imguiRenderer.destroy();

        ImGui::DestroyContext();

        m_mainWindowHandler = nullptr;
        m_vulkanCoreRef = nullptr;
        m_ImGuiSettings = nullptr;
    }

    void ImGuiHandler::updateGUI()
    {
        ImGuiIO& io = ImGui::GetIO();
        
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();

        ImGui::Begin("Hello, World!", NULL, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("This is some text");

        static float test = 0.0f;
        ImGui::SliderFloat("Float Test", &test, 0.0f, 1.0f);
        ImGui::ColorEdit3("Clear Colour", (float*)&m_clearColour);

        ImGui::End();

        ImGui::Render();
    }
}