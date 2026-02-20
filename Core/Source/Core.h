#pragma once
#include <Mark/Engine.h>
#include "Platform/WindowManager.h"
#include "Renderer/Vulkan/Mark_VulkanCore.h"
#include "Utils/TimeTracker.h"
#include "Engine/EngineStats.h"
#include "Engine/SettingsHandler.h"

#include <memory>

namespace Mark
{
    struct Core
    {
        Core(const EngineAppInfo& _appInfo);
        ~Core() = default;
        Core(const Core&) = delete;
        Core& operator=(const Core&) = delete;

        void run();
        void stop();

    private:
        bool m_terminateApplication{ false };
        const EngineAppInfo& m_appInfo;

        friend RendererVK::VulkanCore;
        Platform::ImGuiHandler& imguiHandler() { return m_imguiHandler; }

        void cleanUp();
        void initialize();

        std::vector<RendererVK::WindowToVulkanHandler*> checkAndReturnForRebuildRequests();
        void handleRebuildRequests(std::vector<RendererVK::WindowToVulkanHandler*> _windowHandlers);

        /* --== Window Manager ==-- */
        // Must be created before any renderer, as they'll require glfwInit() call
        std::shared_ptr<Platform::WindowManager> m_windows = std::make_shared<Platform::WindowManager>();

        /* --== Renderer ==--*/
        std::shared_ptr<RendererVK::VulkanCore> m_vulkanCore = std::make_shared<RendererVK::VulkanCore>(m_appInfo, *this);

        /* --== Members ==--*/
        Platform::ImGuiHandler m_imguiHandler;
        Utils::TimeTracker m_timeTracker;
        EngineStats m_engineStats;
        // Load shaders before any windows are created with graphics pipelines and command buffers

        // References and creates the main window
        Platform::Window& coreWindow = m_windows->main(m_vulkanCore);
    };
} // namespace Mark