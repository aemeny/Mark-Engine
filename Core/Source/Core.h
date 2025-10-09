#pragma once
#include <Mark/Engine.h>
#include "Platform/WindowManager.h"
#include "Renderer/MarkVulkanCore.h"

#include <memory>

namespace Mark
{
    struct Core
    {
        Core(const EngineAppInfo& _appInfo);
        ~Core();
        Core(const Core&) = delete;
        Core& operator=(const Core&) = delete;

        void run();
        void stop();

    private:
        bool m_terminateApplication{ false };
        const EngineAppInfo& m_appInfo;

        // Members
        Platform::WindowManager m_windows;
        Platform::Window& coreWindow = m_windows.main();

        // Renderer
        RendererVK::VulkanCore m_vulkanCore{ m_appInfo };
    };
} // namespace Mark