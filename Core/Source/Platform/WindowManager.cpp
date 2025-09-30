#include "WindowManager.h"
#include "GLFWContext.h"
#include "Window.h"

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

    void WindowManager::pollAll() const 
    {
        for (auto& window : m_impl->m_windows)
        {
            window->pollEvents();
        }
    }

    bool WindowManager::anyOpen() const 
    {
        for (auto& window : m_impl->m_windows)
        {
            if (!window->shouldClose()) 
                return true;
        }
        return false;
    }
} // namespace Mark::Platform