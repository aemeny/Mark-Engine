#include <Mark/Engine.h>
#include "Core.h"
#include "Utils/MarkUtils.h"

#include <iostream>

namespace Mark
{
    int Engine::Run(const EngineAppInfo& _appInfo)
    {
        // Initialize Error handling if used
        Utils::Logger::init(
            nullptr, // File path
            Utils::Level::Trace, // Debug Level
            true, // Use colour in logs
            false // Append to file?
        );
        MARK_INFO("Program Boot");

        // Initialize the engines core
        MARK_INFO("Core Initialization");
        Core engineCore(_appInfo);

        // Run the main loop
        try
        {
            MARK_INFO("Core Run() Called");
            engineCore.run();
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            MARK_LOG_ERROR("Program Exit Failure");
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
} // namespace Mark