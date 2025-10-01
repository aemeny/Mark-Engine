#include "WindowManager.h"
#include "GLFWContext.h"
#include "Window.h"

#include <GLFW/glfw3.h>

namespace Mark::Platform
{
    struct WindowManager::Impl
    {
        GLFWContext m_context;
        std::vector<std::unique_ptr<Window> > m_windows;
    };

    WindowManager::WindowManager() :
        m_impl(std::make_unique<Impl>())
    {
        m_impl->m_windows.emplace_back(std::make_unique<Window>(1280, 720, "Mark Editor", true));
    }
    WindowManager::~WindowManager() = default;

    Window& WindowManager::main() { return *m_impl->m_windows.front(); }

    Window& WindowManager::create(int _width, int _height, const char* _title, bool _borderless)
    {
        m_impl->m_windows.emplace_back(std::make_unique<Window>(_width, _height, _title, _borderless));
        return *m_impl->m_windows.back();
    }

    void WindowManager::pollAll()
    {
        // poll GLFW once per frame
        glfwPollEvents();
        // Remove any closed secondary windows
        sweepClosedWindows();
    }

    bool WindowManager::anyOpen() const
    {
        // Main window wants to close, stop running
        if (m_impl->m_windows.empty() || m_impl->m_windows.front()->shouldClose())
            return false;
        return true;
    }


    Window* WindowManager::findByTitle(std::string_view _title)
    {
        for (auto& window : m_impl->m_windows)
            if (window->title() == _title) return window.get();
        return nullptr;
    }

    bool WindowManager::closeByTitle(std::string_view _title, bool _allowMain)
    {
        if (m_impl->m_windows.empty()) return false;

        // Protect main by default
        if (!_allowMain && m_impl->m_windows.front()->title() == _title)
            return false;

        if (auto* window = findByTitle(_title))
        {
            window->requestClose(); // Set flag to close sweep will destroy next poll
            return true;
        }

        return false;
    }

    bool WindowManager::toggleMainFullscreen(bool _borderless)
    {
        if (m_impl->m_windows.empty()) return false;
        m_impl->m_windows.front()->toggleFullscreen(_borderless);
        return true;
    }

    bool WindowManager::setMainFullscreen(bool _enable, bool _borderless)
    {
        if (m_impl->m_windows.empty()) return false;
        m_impl->m_windows.front()->setFullscreen(_enable, _borderless);
        return true;
    }

    bool WindowManager::toggleFullscreenByTitle(std::string_view _title, bool _borderless)
    {
        if (auto* window = findByTitle(_title)) 
        { 
            window->toggleFullscreen(_borderless); 
            return true; 
        }
        return false;
    }

    bool WindowManager::setFullscreenByTitle(std::string_view _title, bool _enable, bool _borderless)
    {
        if (auto* window = findByTitle(_title))
        { 
            window->setFullscreen(_enable, _borderless);
            return true; 
        }
        return false;
    }

    void WindowManager::sweepClosedWindows()
    {
        // Keep index 0 (Main window) alive; delete & closed secondary windows
        for (std::size_t i = 1; i < m_impl->m_windows.size();)
        {
            if (m_impl->m_windows[i]->shouldClose())
            {
                m_impl->m_windows.erase(m_impl->m_windows.begin() + static_cast<long>(i));
                continue; // Do not increment as next element shifted to current index
            }
            i++;
        }
    }

    GLFWmonitor* WindowManager::monitorForWindow(GLFWwindow* _window)
    {
        if (!_window) return glfwGetPrimaryMonitor();

        // If already fullscreen on a monitor, use that one.
        if (GLFWmonitor* monitor = glfwGetWindowMonitor(_window)) return monitor;

        int wx = 0, wy = 0, ww = 0, wh = 0;
        glfwGetWindowPos(_window, &wx, &wy);
        glfwGetWindowSize(_window, &ww, &wh);

        int count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        if (!monitors || count == 0) return glfwGetPrimaryMonitor();

        GLFWmonitor* best = nullptr;
        long bestArea = -1;

        for (int i = 0; i < count; ++i) 
        {
            int mx = 0, my = 0, mw = 0, mh = 0;
            // Workarea accounts for taskbar/dock
            glfwGetMonitorWorkarea(monitors[i], &mx, &my, &mw, &mh);

            const int x1 = std::max(wx, mx);
            const int y1 = std::max(wy, my);
            const int x2 = std::min(wx + ww, mx + mw);
            const int y2 = std::min(wy + wh, my + mh);

            const int ow = std::max(0, x2 - x1);
            const int oh = std::max(0, y2 - y1);
            const long area = static_cast<long>(ow) * static_cast<long>(oh);

            if (area > bestArea) 
            {
                bestArea = area;
                best = monitors[i];
            }
        }
        return best ? best : glfwGetPrimaryMonitor();
    }
} // namespace Mark::Platform