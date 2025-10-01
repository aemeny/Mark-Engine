#include <Mark/Engine.h>
#include "Platform/WindowManager.h"
#include "Platform/Window.h"

#include <iostream>

namespace Mark
{
    int Engine::Run()
    {
        Platform::WindowManager windows;
        Platform::Window& mainWindow = windows.main();

        windows.create(300, 300, "Second");
        windows.create(300, 300, "Third");

        while (windows.anyOpen())
        {
            windows.pollAll();
        }

        return 0;
    }
} // namespace Mark