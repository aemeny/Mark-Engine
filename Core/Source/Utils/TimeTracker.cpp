#include "TimeTracker.h"

namespace Mark::Utils
{
    void TimeTracker::start()
    {
        m_startTime = Clock::now();
        m_frameStartTime = m_startTime;
    }

    void TimeTracker::beginFrame()
    {
        m_frameStartTime = Clock::now();
    }

    void TimeTracker::endFrame() const
    {
        auto now = Clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - m_frameStartTime);
        auto total = std::chrono::duration_cast<std::chrono::duration<double>>(now - m_startTime);

        deltaTime = dt.count();
        totalSeconds = total.count();
    }
}