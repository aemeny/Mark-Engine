#include "MarkVulkanRenderPass.h"
#include "MarkVulkanCore.h"
#include "MarkVulkanSwapChain.h"
#include "Utils/VulkanUtils.h"
#include "Platform/Window.h"

namespace Mark::RendererVK
{
    VulkanRenderPass::VulkanRenderPass(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef) :
        m_vulkanCoreRef(_vulkanCoreRef), m_swapChainRef(_swapChainRef)
    {}

    VulkanRenderPass::~VulkanRenderPass()
    {
        destroyFrameBuffers();
    }

    void VulkanRenderPass::initWithCache(VulkanRenderPassCache& _cache, const VulkanRenderPassKey& _key)
    {
        VkDevice device = m_vulkanCoreRef.lock()->device();
        VkFormat depthFormat = m_vulkanCoreRef.lock()->physicalDevices().selected().m_depthFormat;
        m_renderPassRef = _cache.acquire(_key, [&](const VulkanRenderPassKey& _k) 
            {
                return createSimpleRenderPassForKey(device, _k);
            });
        destroyFrameBuffers();
        createFrameBuffers(m_renderPassRef.get());
    }

    void VulkanRenderPass::destroyFrameBuffers()
    {
        if (m_vulkanCoreRef.expired())
        {
            MARK_ERROR("VulkanCore reference expired, cannot destroy command buffers");
        }

        VkDevice device = m_vulkanCoreRef.lock()->device();
        for (auto& frameBuffer : m_frameBuffers) 
        {
            if (frameBuffer)
            {
                vkDestroyFramebuffer(device, frameBuffer, nullptr); 
                frameBuffer = VK_NULL_HANDLE;
            }
        }
        m_frameBuffers.clear();
    }

    std::vector<VkFramebuffer> VulkanRenderPass::createFrameBuffers(VkRenderPass _renderPass)
    {
        m_frameBuffers.resize(m_swapChainRef.numImages());
        VkExtent2D extent = m_swapChainRef.extent();

        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = _renderPass,
            .width = extent.width,
            .height = extent.height,
            .layers = 1
        };

        VkResult res;
        for (size_t i = 0; i < m_frameBuffers.size(); i++)
        {
            std::vector<VkImageView> attachments;
            attachments.push_back(m_swapChainRef.swapChainImageViewAt(i));
            attachments.push_back(m_swapChainRef.depthImageAt(i).imageView());

            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();

            res = vkCreateFramebuffer(m_vulkanCoreRef.lock()->device(), &framebufferInfo, nullptr, &m_frameBuffers[i]);
            CHECK_VK_RESULT(res, "Create framebuffer");
        }

        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Framebuffers Created: %zu", m_frameBuffers.size());

        return m_frameBuffers;
    }

    VkRenderPass createSimpleRenderPassForKey(VkDevice _device, const VulkanRenderPassKey& _key)
    {
        VkAttachmentDescription attachColour = {
            .flags = 0,
            .format = _key.m_colorFormat,
            .samples = _key.m_samples,
            .loadOp = _key.m_colorLoad,
            .storeOp = _key.m_colorStore,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };

        VkAttachmentReference attachColourRef = {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };

        VkAttachmentDescription attachDepth = {
            .flags = 0,
            .format = _key.m_depthFormat,
            .samples = _key.m_samples,
            .loadOp = _key.m_depthLoad,
            .storeOp = _key.m_depthStore,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

        VkAttachmentReference depthAttachRef = {
            .attachment = 1,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

        VkSubpassDescription subpassDesc = {
            .flags = 0,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 0,
            .pInputAttachments = nullptr,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachColourRef,
            .pResolveAttachments = nullptr,
            .pDepthStencilAttachment = &depthAttachRef,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments = nullptr
        };

        VkSubpassDependency subpassDep = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        };

        std::vector<VkAttachmentDescription> attachments = { attachColour, attachDepth };

        VkRenderPassCreateInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .subpassCount = 1,
            .pSubpasses = &subpassDesc,
            .dependencyCount = 1,
            .pDependencies = &subpassDep
        };

        VkRenderPass renderPass = VK_NULL_HANDLE;

        VkResult res = vkCreateRenderPass(_device, &renderPassInfo, nullptr, &renderPass);
        CHECK_VK_RESULT(res, "Create render pass");

        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Render Pass Created");

        return renderPass;
    }
}// namespace Mark::RendererVK