#pragma once
#include "Mark_TextureHandler.h"
#include <volk.h>
#include <memory>
#include <vector>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanSwapChain
    {
        VulkanSwapChain(std::weak_ptr<VulkanCore> _vulkanCoreRef, VkSurfaceKHR& _swapChainRef);
        ~VulkanSwapChain() = default;
        void destroySwapChain();
        VulkanSwapChain(const VulkanSwapChain&) = delete;
        VulkanSwapChain& operator=(const VulkanSwapChain&) = delete;

        void createSwapChain(uint32_t _fbWidth, uint32_t _fbHeight);
        void recreateSwapChain(uint32_t _fbWidth, uint32_t _fbHeight);
        void createDepthResources();
        void initImageLayoutsForDynamicRendering();

        // Getters
        VkSwapchainKHR& swapChain() { return m_swapChain; }
        int numImages() const { return static_cast<int>(m_swapChainImages.size()); }

        std::vector<VkImage>& swapChainImages() { return m_swapChainImages; }
        VkImage& swapChainImageAt(int _index) { return m_swapChainImages[_index]; }

        std::vector<VkImageView>& swapChainImageViews() { return m_swapChainImageViews; }
        VkImageView& swapChainImageViewAt(int _index) { return m_swapChainImageViews[_index]; }

        VkExtent2D extent() const { return m_extent; }
        VkSurfaceFormatKHR surfaceFormat() const { return m_surfaceFormat; }

        TextureHandler& depthImageAt(int _index) { return m_depthImages[_index]; }

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VkSurfaceKHR& m_surfaceRef;

        VkSwapchainKHR m_swapChain{ VK_NULL_HANDLE };
        std::vector<VkImage> m_swapChainImages;
        std::vector<VkImageView> m_swapChainImageViews;
        std::vector<TextureHandler> m_depthImages;

        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& _capabilities, uint32_t _fbWidth, uint32_t _fbHeight);
        VkExtent2D m_extent{ 0, 0 };
        VkSurfaceFormatKHR m_surfaceFormat{ VK_FORMAT_UNDEFINED };
    };
} // namespace Mark::RendererVK