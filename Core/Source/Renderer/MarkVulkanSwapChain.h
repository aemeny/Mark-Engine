#pragma once
#include <volk.h>
#include <memory>
#include <vector>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanSwapChain
    {
        VulkanSwapChain(std::weak_ptr<RendererVK::VulkanCore> _vulkanCoreRef, VkSurfaceKHR& _swapChainRef);
        ~VulkanSwapChain() = default;
        void destroySwapChain();
        VulkanSwapChain(const VulkanSwapChain&) = delete;
        VulkanSwapChain& operator=(const VulkanSwapChain&) = delete;

        void createSwapChain();
        VkSwapchainKHR& swapChain() { return m_swapChain; }
    private:
        std::weak_ptr<RendererVK::VulkanCore> m_vulkanCoreRef;
        VkSurfaceKHR& m_surfaceRef;

        VkSwapchainKHR m_swapChain{ VK_NULL_HANDLE };
        std::vector<VkImage> m_swapChainImages;
        std::vector<VkImageView> m_swapChainImageViews;
    };
} // namespace Mark::RendererVK