#include "Core.h"
#include "Utils/Mark_Utils.h"
#include "Renderer/Vulkan/Mark_WindowToVulkanHandler.h"

#include "Platform/Window.h" // TEMP FOR ACCESSING MAIN WINDOW IN run()

namespace Mark
{
    Core::Core(const EngineAppInfo& _appInfo) : m_appInfo(_appInfo) {}

    void Core::run()
    {
        initialize();

        while (!m_terminateApplication && m_windows->anyOpen())
        {
            // Start frame
            m_timeTracker.beginFrame();

            // Poll windows / update engine
            m_windows->pollAll();

            m_imguiHandler.updateGUI();

            // Check for rebuild requests
            auto windowsToRebuild = checkAndReturnForRebuildRequests();
            if (windowsToRebuild.size() > 0) {
                handleRebuildRequests(windowsToRebuild); 
            }

            // Start Rendering
            m_windows->renderAll();

            // End frame
            m_timeTracker.endFrame();
            m_engineStats.update(Utils::TimeTracker::deltaTime);
        }

        cleanUp();
    }

    void Core::initialize()
    {
        m_imguiHandler.initialize(m_appInfo.imguiSettings, &m_windows->main().vkHandler(), m_vulkanCore.get());

        Settings::MarkSettings::Get().initialize(m_windows->main().handle());

        m_engineStats.initialize(m_windows->main());

        m_timeTracker.start();


        // TEMP ADD MESH FOR MAIN WINDOW
        m_windows->main().vkHandler().addMesh("Models/Curuthers1.obj");
        m_windows->main().vkHandler().initCameraController();
        // TEMP MULTI-WINDOW TESTING
        //Platform::Window& window2 = m_windows->create(600, 600, "Second", VkClearColorValue{ {0.0f, 1.0f, 0.0f, 1.0f} }, false);
        //window2.vkHandler().addMesh("Models/Curuthers.obj");
        //window2.vkHandler().initCameraController();
    }

    std::vector<RendererVK::WindowToVulkanHandler*> Core::checkAndReturnForRebuildRequests()
    {
        std::vector<RendererVK::WindowToVulkanHandler*> windowHandlers;
        
        auto& settings = Settings::MarkSettings::Get();
        if (settings.requestSwapchainRebuild())
        { // Rebuild all windows
            settings.acknowledgeSwapchainRebuildRequest();
            for (size_t i = 0; i < m_windows->numberOfWindows(); i++) {
                windowHandlers.push_back(&m_windows->returnByIndex(i)->vkHandler());
            }
        }
        else
        {
            for (size_t i = 0; i < m_windows->numberOfWindows(); i++)
            {
                auto window = m_windows->returnByIndex(i);
                if (window->framebufferResized())
                {
                    window->resetFramebufferResized();
                    windowHandlers.push_back(&window->vkHandler());
                }
            }
        }
        
        return windowHandlers;
    }

    void Core::handleRebuildRequests(std::vector<RendererVK::WindowToVulkanHandler*> _windowHandlers)
    {
        m_vulkanCore->waitForDeviceIdle();

        m_imguiHandler.clearCommandBuffers();
        for (RendererVK::WindowToVulkanHandler* handler : _windowHandlers) 
        { 
            handler->rebuildRendererResources();
        }
        m_imguiHandler.rebuildCommandBuffers();
    }

    void Core::cleanUp()
    {
        m_vulkanCore->waitForDeviceIdle();

        m_imguiHandler.destroy();
        m_windows->destroyAllWindows();
    }

    void Core::stop()
    {
        m_terminateApplication = true;
        MARK_INFO(Utils::Category::Engine, "---Mark Core Stop Called, terminating application---");
    }
} // namespace Mark