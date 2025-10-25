#include "Core.h"
#include "Utils/MarkUtils.h"

namespace Mark
{
    Core::Core(const EngineAppInfo& _appInfo) : m_appInfo(_appInfo) {}
    Core::~Core() = default;

    void Core::run()
    {
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
        MARK_INFO("---Mark Core Stop Called, terminating application---");
    }

} // namespace Mark