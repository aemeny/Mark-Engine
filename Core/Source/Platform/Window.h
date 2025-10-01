#pragma once
#include <memory>
#include <string>
#include <algorithm>

struct GLFWwindow;

namespace Mark::Platform
{
    struct Window 
    {
        Window(int _width, int _height, std::string_view _title, bool _borderless = true);
        ~Window();

        GLFWwindow* handle() const { return m_window; }
        std::string_view title() const noexcept { return m_windowName; }

        bool shouldClose() const;
        void requestClose();

        void setFullscreen(bool _enable, bool _borderless = true);
        void toggleFullscreen(bool _borderless = true);
        bool isFullscreen() const { return m_isFullscreen; }

        void frameBufferSize(int& _width, int& _height) const;

        void waitUntilFramebufferValid() const;

    private:
        /* Properties */
        GLFWwindow* m_window{ nullptr };
        std::string m_windowName{ "Mark" };
        int m_width{ 0 };
        int m_height{ 0 };

        // Window mode
        bool m_isFullscreen{ false };
        bool m_borderless{ false };
        int m_windowCordX{ 0 };
        int m_windowCordY{ 0 };

        static void KeyCallback(GLFWwindow* _window, int _key, int _scancode, int _action, int _mods);
    };
} // namespace Mark::Platform