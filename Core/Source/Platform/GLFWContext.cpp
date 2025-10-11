#include "GLFWContext.h"

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
                throw std::runtime_error("Failed to initialize GLFW");
            }
            glfwSetErrorCallback(+[](int e, const char* d){
                std::fprintf(stderr, "[GLFW] (%d) %s\n", e, d);
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