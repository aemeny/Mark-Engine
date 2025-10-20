#pragma once
#include <memory>
#include <vector>
#include <string_view>
#include <optional>

namespace Mark::RendererVK { struct VulkanCore; }
namespace Mark { struct Core; }
struct GLFWmonitor;
struct GLFWwindow;
union VkClearColorValue;

namespace Mark::Platform
{
    struct Window;
    
    struct WindowManager
    {
        WindowManager();
        ~WindowManager();

        Window& main(std::optional<std::weak_ptr<RendererVK::VulkanCore>> _vulkanCoreRef = std::nullopt);
        Window& create(int _width, int _height, const char* _title, VkClearColorValue _clearColour, bool _borderless = true);

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
        
        // Destroy windows before VulkanCore to remove surfaces first
        friend Core;
        void destroyAllWindows();
    };
} // namespace Mark::Platform