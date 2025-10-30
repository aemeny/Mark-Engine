#include "GLFWContext.h"
#include "Utils/MarkUtils.h"

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
                MARK_FATAL("Failed to initialize GLFW");
            }
            glfwSetErrorCallback(+[](int _code, const char* _desc) noexcept {
                if (_code == GLFW_OUT_OF_MEMORY) MARK_FATAL("[GLFW] Out of memory: %s", _desc);
                if (_code == GLFW_PLATFORM_ERROR) MARK_LOG_ERROR_C(Utils::Category::GLFW, "(%d) %s", _code, _desc);
                else MARK_WARN_C(Utils::Category::GLFW, "(%d) %s", _code, _desc);
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