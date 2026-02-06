#include "Core.h"
#include "Utils/Mark_Utils.h"

#include "Platform/Window.h" // TEMP FOR ACCESSING MAIN WINDOW IN run()
#include "imgui.h" // TEMP FOR TESTING GUI

namespace Mark
{
    Core::Core(const EngineAppInfo& _appInfo) : m_appInfo(_appInfo) {}

    void Core::run()
    {
        initialize();

        while (!m_terminateApplication && m_windows->anyOpen())
        {
            m_windows->pollAll();

            if (m_imguiHandler.showGUI()) {
                m_imguiHandler.updateGUI();
            }

            m_windows->renderAll();
        }

        cleanUp();
    }

    void Core::initialize()
    {
        m_imguiHandler.initialize(m_appInfo.imguiSettings, &m_windows->main().vkHandler(), m_vulkanCore.get());

        // TEMP ADD MESH FOR MAIN WINDOW
        m_windows->main().vkHandler().addMesh("Models/Curuthers.obj");
        m_windows->main().vkHandler().initCameraController();

        // TEMP MULTI-WINDOW TESTING
        //Platform::Window& window2 = m_windows->create(600, 600, "Second", VkClearColorValue{ {0.0f, 1.0f, 0.0f, 1.0f} }, false);
        //window2.vkHandler().addMesh("Models/Curuthers.obj");
        //window2.vkHandler().initCameraController();
        //Platform::Window& window3 = m_windows->create(600, 600, "Third", VkClearColorValue{ {0.0f, 0.0f, 1.0f, 1.0f} }, false);
        //window3.vkHandler().addMesh("Models/Curuthers.obj");
        //window3.vkHandler().initCameraController();
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
        MARK_INFO_C(Utils::Category::Engine, "---Mark Core Stop Called, terminating application---");
    }
} // namespace Mark