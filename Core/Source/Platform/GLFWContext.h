#pragma once

namespace Mark::Platform
{
    struct GLFWContext
    {
        GLFWContext();
        ~GLFWContext();
        GLFWContext(const GLFWContext&) = delete;
        GLFWContext& operator=(const GLFWContext&) = delete;
    };
} // namespace Mark::Platform