#pragma once

#include <chrono>

namespace Mark::Utils
{
    struct TimeTracker
    {
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        TimeTracker() = default;

        // Called once at start before main loop
        void start();

        void beginFrame();
        void endFrame() const;

        inline static double deltaTime = 0.0;
        inline static double totalSeconds = 0.0;
    private:
        TimePoint m_startTime{};
        TimePoint m_frameStartTime{};
    };
}