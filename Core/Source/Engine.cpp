#include <Mark/Engine.h>
#include "Core.h"

#include <iostream>

namespace Mark
{
    int Engine::Run(const EngineAppInfo& _appInfo)
    {
        // Initialize the engines core
        Core engineCore(_appInfo);

        // Run the main loop
        try
        {
            engineCore.run();
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
} // namespace Mark