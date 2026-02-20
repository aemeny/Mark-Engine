#include <Mark/Engine.h>
#include "Core.h"
#include "Utils/Mark_Utils.h"

#include <iostream>

namespace Mark
{
    int Engine::Run(const EngineAppInfo& _appInfo)
    {
        // Initialize Error handling if used
        Utils::Logger::init(
            nullptr, // File path
            Utils::Level::Info, // Debug Level
            true, // Use colour in logs
            false // Append to file?
        );

        // Initialize the engines core
        MARK_INFO(Utils::Category::Engine, "Core Initialization");
        Core engineCore(_appInfo);

        // Run the main loop
        try
        {
            MARK_INFO(Utils::Category::Engine, "Core Run() Called");
            engineCore.run();
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            MARK_ERROR(Utils::Category::Engine, "Program Exit Failure");
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
} // namespace Mark