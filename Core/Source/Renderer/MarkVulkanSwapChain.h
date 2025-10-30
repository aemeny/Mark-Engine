#pragma once
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

        void createSwapChain();

        // Getters
        VkSwapchainKHR& swapChain() { return m_swapChain; }
        int numImages() const { return static_cast<int>(m_swapChainImages.size()); }
        std::vector<VkImage>& swapChainImages() { return m_swapChainImages; }
        VkImage& swapChainImageAt(int _index) { return m_swapChainImages[_index]; }
        std::vector<VkImageView>& swapChainImageViews() { return m_swapChainImageViews; }
        VkImageView& swapChainImageViewAt(int _index) { return m_swapChainImageViews[_index]; }
        VkExtent2D extent() const { return m_extent; }
        VkSurfaceFormatKHR surfaceFormat() const { return m_surfaceFormat; }

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VkSurfaceKHR& m_surfaceRef;

        VkSwapchainKHR m_swapChain{ VK_NULL_HANDLE };
        std::vector<VkImage> m_swapChainImages;
        std::vector<VkImageView> m_swapChainImageViews;

        VkExtent2D m_extent{ 0, 0 };
        VkSurfaceFormatKHR m_surfaceFormat{ VK_FORMAT_UNDEFINED };
    };
} // namespace Mark::RendererVK