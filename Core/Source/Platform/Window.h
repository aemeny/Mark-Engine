#pragma once
#include "Renderer/Mark_WindowToVulkanHandler.h"

#include <memory>
#include <string>
#include <algorithm>

namespace Mark::RendererVK { struct VulkanCore; }
struct GLFWwindow;
union VkClearColorValue;

namespace Mark::Platform
{
    struct Window 
    {
        Window(std::weak_ptr<RendererVK::VulkanCore> _vulkanCoreRef, int _width, int _height, std::string_view _title, VkClearColorValue _clearColour, bool _borderless = true);
        ~Window();

        GLFWwindow* handle() const { return m_window; }
        std::string_view title() const noexcept { return m_windowName; }

        bool shouldClose() const;
        void requestClose();

        void setFullscreen(bool _enable, bool _borderless = true);
        void toggleFullscreen(bool _borderless = true);
        bool isFullscreen() const { return m_isFullscreen; }

        void frameBufferSize(int& _width, int& _height) const;
        std::array<int, 2> windowSize() const { return { m_width, m_height }; };
        void getWindowContentScale(float& _scaleX, float& _scaleY) const;

        void waitUntilFramebufferValid() const;

        void setTitle(std::string _title);

        // Vulkan Renderering
        RendererVK::WindowToVulkanHandler& vkHandler() { return *m_vkHandler; }

    private:
        /* --== Properties ==-- */
        GLFWwindow* m_window{ nullptr };
        std::string m_windowName{ "Mark" };
        int m_width{ 0 };
        int m_height{ 0 };

        // Vulkan Renderering
        std::unique_ptr<RendererVK::WindowToVulkanHandler> m_vkHandler;
        
        // Window mode
        bool m_isFullscreen{ false };
        bool m_borderless{ false };
        int m_windowCordX{ 0 };
        int m_windowCordY{ 0 };

        /* --== Functions ==-- */
        static void KeyCallback(GLFWwindow* _window, int _key, int _scancode, int _action, int _mods);
    };
} // namespace Mark::Platform