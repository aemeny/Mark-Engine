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

        /* --== Renderer ==--*/
        // Must be created before any window, as windows will need the Vulkan instance
        std::shared_ptr<RendererVK::VulkanCore> m_vulkanCore = std::make_shared<RendererVK::VulkanCore>(m_appInfo);

        /* --== Members ==--*/
        Platform::WindowManager m_windows{ m_vulkanCore };
        Platform::Window& coreWindow = m_windows.main();
    };
} // namespace Mark