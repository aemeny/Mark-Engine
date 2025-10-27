#include "MarkVulkanRenderPass.h"
#include "MarkVulkanCore.h"
#include "MarkVulkanSwapChain.h"
#include "Utils/VulkanUtils.h"
#include "Platform/Window.h"

namespace Mark::RendererVK
{
    VulkanRenderPass::VulkanRenderPass(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef, Platform::Window& _windowRef) :
        m_vulkanCoreRef(_vulkanCoreRef), m_swapChainRef(_swapChainRef), m_windowRef(_windowRef) 
    {}

    VulkanRenderPass::~VulkanRenderPass()
    {
        vkDestroyRenderPass(m_vulkanCoreRef.lock()->device(), m_renderPass, nullptr);
    }

    VkRenderPass VulkanRenderPass::createSimpleRenderPass()
    {
        VkAttachmentDescription attachDesc = {
            .flags = 0,
            .format = m_swapChainRef.surfaceFormat().format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };
        
        VkAttachmentReference attachRef = {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };

        VkSubpassDescription subpassDesc = {
            .flags = 0,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 0,
            .pInputAttachments = nullptr,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachRef,
            .pResolveAttachments = nullptr,
            .pDepthStencilAttachment = nullptr,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments = nullptr
        };

        VkRenderPassCreateInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .attachmentCount = 1,
            .pAttachments = &attachDesc,
            .subpassCount = 1,
            .pSubpasses = &subpassDesc,
            .dependencyCount = 0,
            .pDependencies = nullptr
        };

        VkRenderPass renderPass;

        VkResult res = vkCreateRenderPass(m_vulkanCoreRef.lock()->device(), &renderPassInfo, nullptr, &renderPass);
        CHECK_VK_RESULT(res, "Create render pass");

        MARK_INFO("Vulkan Render Pass Created");

        return renderPass;
    }

    std::vector<VkFramebuffer> VulkanRenderPass::createFrameBuffers(VkRenderPass _renderPass)
    {
        m_frameBuffers.resize(m_swapChainRef.numImages());
        std::array<int, 2> windowSize = m_windowRef.windowSize();

        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = _renderPass,
            .attachmentCount = 1,
            .width = windowSize[0],
            .height = windowSize[1],
            .layers = 1
        };

        VkResult res;
        for (size_t i = 0; i < m_frameBuffers.size(); i++)
        {
            framebufferInfo.pAttachments = &m_swapChainRef.swapChainImageViewAt(i); 

            res = vkCreateFramebuffer(m_vulkanCoreRef.lock()->device(), &framebufferInfo, nullptr, &m_frameBuffers[i]);
            CHECK_VK_RESULT(res, "Create framebuffer");
        }

        MARK_INFO("Vulkan Framebuffers Created: %zu", m_frameBuffers.size());

        return m_frameBuffers;
    }
}// namespace Mark::RendererVK