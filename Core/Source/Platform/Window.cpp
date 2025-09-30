#include "Window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace Mark::Platform
{
    Window::Window(int _width, int _height, const std::string_view& _title, bool _borderless)
        : m_borderless(_borderless), m_windowName(_title), m_width(_width), m_height(_height)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

        m_window = glfwCreateWindow(_width, _height, m_windowName.c_str(), nullptr, nullptr);
        if (!m_window) throw std::runtime_error("Failed to create GLFW window");
    }

    Window::~Window()
    {
        if (m_window)
        {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
    }

    void Window::pollEvents() const { glfwPollEvents(); }
    bool Window::shouldClose() const { return glfwWindowShouldClose(m_window); }
    void Window::frameBufferSize(int& _width, int& _height) const 
    {
        glfwGetFramebufferSize(m_window, &_width, &_height);
    }

    void Window::setFullscreen(bool _enable, bool _borderless)
    {
        if (_enable == m_isFullscreen) return;

        if (_enable)
        {
            glfwGetWindowPos(m_window, &m_windowCordX, &m_windowCordY);
            glfwGetWindowSize(m_window, &m_width, &m_height);

            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            if (!monitor) throw std::runtime_error("No monitor available for fullscreen");
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (!mode) throw std::runtime_error("Failed to get video mode");

            m_borderless = _borderless;
            if (m_borderless)
            {
                glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
                glfwSetWindowAttrib(m_window, GLFW_AUTO_ICONIFY, GLFW_FALSE);
                glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            }
            else
            {
                glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
                glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            }
        }
        else
        {
            glfwSetWindowMonitor(m_window, nullptr, m_windowCordX, m_windowCordY, m_width, m_height, GLFW_DONT_CARE);
            glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
        }
        m_isFullscreen = _enable;
    }
} // namespace Mark::Platform