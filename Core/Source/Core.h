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

        /* --== Window Manager ==-- */
        // Must be created before any renderer, as they'll require glfwInit() call
        std::shared_ptr<Platform::WindowManager> m_windows = std::make_shared<Platform::WindowManager>();

        /* --== Renderer ==--*/
        std::shared_ptr<RendererVK::VulkanCore> m_vulkanCore = std::make_shared<RendererVK::VulkanCore>(m_appInfo);

        /* --== Members ==--*/
        Platform::Window& coreWindow = m_windows->main(m_vulkanCore);
    };
} // namespace Mark