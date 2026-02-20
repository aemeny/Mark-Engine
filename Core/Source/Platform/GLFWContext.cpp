#include "GLFWContext.h"
#include "Utils/Mark_Utils.h"

#include <GLFW/glfw3.h>

#include <stdexcept>
#include <atomic>
#include <cstdio>

namespace Mark::Platform
{
    static std::atomic<int> g_ref{ 0 };

    GLFWContext::GLFWContext()
    {
        if (g_ref.fetch_add(1) == 0)
        {
            if (!glfwInit() || !glfwVulkanSupported())
            {
                MARK_FATAL(Utils::Category::GLFW, "Failed to initialize GLFW");
            }
            glfwSetErrorCallback(+[](int _code, const char* _desc) noexcept {
                if (_code == GLFW_OUT_OF_MEMORY) MARK_FATAL(Utils::Category::GLFW, "[GLFW] Out of memory: %s", _desc);
                if (_code == GLFW_PLATFORM_ERROR) MARK_FATAL(Utils::Category::GLFW, "(%d) %s", _code, _desc);
                else MARK_WARN(Utils::Category::GLFW, "(%d) %s", _code, _desc);
            });
        }
    }

    GLFWContext::~GLFWContext()
    {
        if (g_ref.fetch_sub(1) == 1)
        {
            glfwTerminate();
        }
    }
} // namespace Mark::Platform