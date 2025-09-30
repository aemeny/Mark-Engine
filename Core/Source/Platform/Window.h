#pragma once
#include <memory>
#include <string>

struct GLFWwindow;

namespace Mark::Platform
{
    struct Window 
    {
        Window(int _width, int _height, const std::string_view& _title, bool _borderless = true);
        ~Window();

        GLFWwindow* handle() const { return m_window; }
        void pollEvents() const;
        bool shouldClose() const;

        void setFullscreen(bool _enable, bool _borderless = true);
        bool isFullscreen() const { return m_isFullscreen; }

        void frameBufferSize(int& _width, int& _height) const;

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
    };
} // namespace Mark::Platform