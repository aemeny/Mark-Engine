#pragma once
#include <Mark/Settings/imguiSettings.h>

#include <volk.h>
#include <vector>

namespace Mark::Platform { struct ImGuiHandler; }
namespace Mark::RendererVK
{
    struct VulkanCore;
    struct WindowToVulkanHandler;
    struct ImGuiRenderer 
    {
        ImGuiRenderer(Mark::Platform::ImGuiHandler& m_imguiHandler);
        ~ImGuiRenderer() = default;
        ImGuiRenderer(const ImGuiRenderer&) = delete;
        ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;

        void initialize();
        void destroy();

    private:
        friend Mark::Platform::ImGuiHandler;
        Mark::Platform::ImGuiHandler& m_imguiHandler;

        void createDescriptorPool();
        void initImGui();

        // Called every frame to record ImGui draw commands when GUI is visible
        // returns command buffer to submit to the queue
        VkCommandBuffer prepareCommandBuffer(uint32_t _imageIndex);

        int m_frameBufferWidth{ 0 };
        int m_frameBufferHeight{ 0 };
        std::vector<VkCommandBuffer> m_commandBuffers;
        VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
    };
}