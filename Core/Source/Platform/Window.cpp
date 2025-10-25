#include "Window.h"
#include "WindowManager.h"
#include "Renderer/MarkVulkanCore.h"
#include "Utils/VulkanUtils.h"
#include "Utils/MarkUtils.h"

#include <GLFW/glfw3.h>
#include <volk.h>

namespace Mark::Platform
{
    Window::Window(std::weak_ptr<RendererVK::VulkanCore> _vulkanCoreRef, int _width, int _height, std::string_view _title, VkClearColorValue _clearColour, bool _borderless)
        : m_borderless(_borderless), m_windowName(_title), m_width(_width), m_height(_height)
    {
        // Create GLFW window
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

        m_window = glfwCreateWindow(_width, _height, m_windowName.c_str(), nullptr, nullptr);
        if (!m_window) MARK_ERROR("Failed to create GLFW window");

        glfwSetWindowUserPointer(m_window, this);
        glfwSetKeyCallback(m_window, &Window::KeyCallback);

        MARK_INFO("GLFW Window Created: %s (%dx%d)", m_windowName.c_str(), _width, _height);

        // Create Vulkan handler
        m_vkHandler = std::make_unique<RendererVK::WindowToVulkanHandler>(_vulkanCoreRef, m_window, _clearColour);
    }

    Window::~Window()
    {
        if (m_vkHandler)
        {
            m_vkHandler.reset();
        }

        if (m_window)
        {
            glfwSetWindowUserPointer(m_window, nullptr);
            glfwDestroyWindow(m_window);
            m_window = nullptr;
            MARK_INFO("GLFW Window Destroyed: %s", m_windowName.c_str());
        }
    }

    bool Window::shouldClose() const { return glfwWindowShouldClose(m_window); }
    void  Window::requestClose() { glfwSetWindowShouldClose(m_window, GLFW_TRUE); }

    void Window::frameBufferSize(int& _width, int& _height) const 
    {
        glfwGetFramebufferSize(m_window, &_width, &_height);
    }

    void Window::waitUntilFramebufferValid() const
    {
        int w = 0, h = 0;
        do 
        {
            glfwGetFramebufferSize(m_window, &w, &h);
            if (w == 0 || h == 0) {
                glfwWaitEvents(); // sleep until the window is restored/resized
            }
        } 
        while (w == 0 || h == 0);
    }

    void Window::KeyCallback(GLFWwindow* _window, int _key, int _scancode, int _action, int _mods)
    {
        if (_action != GLFW_PRESS) return;

        Window* self = static_cast<Window*>(glfwGetWindowUserPointer(_window));
        if (!self) return;

        if (_key == GLFW_KEY_F11)
        {
            self->toggleFullscreen(true);
        }
    }

    void Window::setFullscreen(bool _enable, bool _borderless)
    {
        if (_enable == m_isFullscreen) return;

        if (_enable)
        {
            glfwGetWindowPos(m_window, &m_windowCordX, &m_windowCordY);
            glfwGetWindowSize(m_window, &m_width, &m_height);

            GLFWmonitor* monitor = WindowManager::monitorForWindow(m_window);
            if (!monitor) MARK_ERROR("No monitor available for fullscreen");
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (!mode) MARK_ERROR("Failed to get video mode");

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

    void Window::toggleFullscreen(bool _borderless)
    {
        setFullscreen(!m_isFullscreen, _borderless);
    }

} // namespace Mark::Platform