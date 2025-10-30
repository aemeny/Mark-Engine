#include "Core.h"
#include "Utils/MarkUtils.h"

namespace Mark
{
    Core::Core(const EngineAppInfo& _appInfo) : m_appInfo(_appInfo) {}
    Core::~Core() = default;

    void Core::run()
    {
        //m_windows->create(600, 600, "Second", VkClearColorValue{ {0.0f, 1.0f, 0.0f, 1.0f} }, false);
        //m_windows->create(600, 600, "Third", VkClearColorValue{ {0.0f, 0.0f, 1.0f, 1.0f} }, false);
        while (!m_terminateApplication && m_windows->anyOpen())
        {
            m_windows->pollAll();
            m_windows->renderAll();
        }
        m_windows->destroyAllWindows();
    }

    void Core::stop()
    {
        m_terminateApplication = true;
        MARK_INFO_C(Utils::Category::Engine, "---Mark Core Stop Called, terminating application---");
    }

} // namespace Mark