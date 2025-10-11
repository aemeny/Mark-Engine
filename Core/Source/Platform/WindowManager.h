#pragma once
#include <memory>
#include <vector>
#include <string_view>

namespace Mark::RendererVK { struct VulkanCore; }
struct GLFWmonitor;
struct GLFWwindow;

namespace Mark::Platform
{
    struct Window;
    
    struct WindowManager
    {
        WindowManager(std::weak_ptr<RendererVK::VulkanCore> _vulkanCoreRef);
        ~WindowManager();

        Window& main();
        Window& create(int _width, int _height, const char* _title, bool _borderless = true);

        void pollAll();
        bool anyOpen() const;

        Window* findByTitle(std::string_view _title);
        bool closeByTitle(std::string_view _title, bool _allowMain = false);

        // Fullscreen controls
        bool toggleMainFullscreen(bool _borderless = true);
        bool setMainFullscreen(bool _enable, bool _borderless = true);
        bool toggleFullscreenByTitle(std::string_view _title, bool _borderless = true);
        bool setFullscreenByTitle(std::string_view _title, bool _enable, bool _borderless = true);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
        std::weak_ptr<RendererVK::VulkanCore> m_vulkanCoreRef;

        void sweepClosedWindows();

        // Helper: choose the monitor the window is mostly on.
        static GLFWmonitor* monitorForWindow(GLFWwindow* _window);
        friend struct Window;
    };
} // namespace Mark::Platform