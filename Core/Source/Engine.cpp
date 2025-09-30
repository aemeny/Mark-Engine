#include <Mark/Engine.h>
#include "Platform/WindowManager.h"
#include "Platform/Window.h"

namespace Mark
{
    int Engine::Run()
    {
        Platform::WindowManager windows;
        Platform::Window& mainWindow = windows.main();

        windows.create(800, 800, "Second");

        while (windows.anyOpen())
        {
            int fbw = 0, fbh = 0;
            mainWindow.frameBufferSize(fbw, fbh);
            if (fbw == 0 || fbh == 0) 
            {
                //glfwWaitEvents();
                continue;
            }

            windows.pollAll();
        }

        return 0;
    }
} // namespace Mark