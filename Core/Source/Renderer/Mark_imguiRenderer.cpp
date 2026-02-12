#include "Mark_imguiRenderer.h"
#include "Mark_VulkanCore.h"
#include "Platform/Window.h"

#include "Utils/Mark_Utils.h"
#include "Utils/ErrorHandling.h"
#include "Utils/VulkanUtils.h"

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"

static void checkVKResult(VkResult _err)
{
    if (_err == 0) return;

    MARK_LOG_ERROR_C(Mark::Utils::Category::imgui, "VkResult: %d", _err);

    if (_err < 0) {
        MARK_FATAL("imgui Vulkan Renderer VkResult wrong error: %d", _err);
    }
}

namespace Mark::RendererVK
{
    ImGuiRenderer::ImGuiRenderer(Mark::Platform::ImGuiHandler& m_imguiHandler) :
        m_imguiHandler(m_imguiHandler)
    {}

    void ImGuiRenderer::initialize()
    {
        createDescriptorPool();
        initImGui();
    }

    void ImGuiRenderer::rebuildCommandBuffers()
    {
        auto mainWindowHandler = m_imguiHandler.m_mainWindowHandler;
        VulkanCommandBuffers& commandBufferHandler = mainWindowHandler->m_vulkanCommandBuffers;

        // Recreate to match new swapchain
        commandBufferHandler.createCommandBuffers(static_cast<uint32_t>(mainWindowHandler->m_swapChain.numImages()), m_commandBuffers);
    }

    void ImGuiRenderer::clearCommandBuffers()
    {
        VulkanCommandBuffers& commandBufferHandler = m_imguiHandler.m_mainWindowHandler->m_vulkanCommandBuffers;

        // Free old ImGui command buffers
        if (!m_commandBuffers.empty()) {
            commandBufferHandler.freeCommandBuffers(m_commandBuffers.size(), m_commandBuffers.data());
            m_commandBuffers.clear();
        }
    }

    void ImGuiRenderer::createDescriptorPool()
    {
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        VkDescriptorPoolCreateInfo poolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 1000 * ARRAY_COUNT(poolSizes),
            .poolSizeCount = static_cast<uint32_t>(ARRAY_COUNT(poolSizes)),
            .pPoolSizes = poolSizes
        };

        VkResult res = vkCreateDescriptorPool(m_imguiHandler.m_vulkanCoreRef->device(), &poolCreateInfo, nullptr, &m_descriptorPool);
        CHECK_VK_RESULT(res, "Failed to create descriptor pool for ImGui");
    }

    void ImGuiRenderer::initImGui()
    {
        auto mainWindowHandler = m_imguiHandler.m_mainWindowHandler;
        auto vulkanCoreRef = m_imguiHandler.m_vulkanCoreRef;

        if (!vulkanCoreRef || !mainWindowHandler) {
            MARK_ERROR("Reference expired, cannot initialize ImGui");
        }

        bool installGLFWCallbacks = true;
        ImGui_ImplGlfw_InitForVulkan(mainWindowHandler->m_windowRef.handle(), installGLFWCallbacks);

        VkFormat colourFormat = mainWindowHandler->m_swapChain.surfaceFormat().format;
        const VulkanPhysicalDevices::DeviceProperties& deviceProps = vulkanCoreRef->physicalDevices().selected();

        ImGui_ImplVulkan_LoadFunctions(deviceProps.m_properties.apiVersion,
            [](const char* function_name, void* user_data) -> PFN_vkVoidFunction
            {   // This goes through Volk's vkGetInstanceProcAddr
                VkInstance instance = (VkInstance)user_data;
                return vkGetInstanceProcAddr(instance, function_name);
            },
            (void*)vulkanCoreRef->instance()
        );

        ImGui_ImplVulkan_InitInfo initInfo = {
            .ApiVersion = deviceProps.m_properties.apiVersion,
            .Instance = vulkanCoreRef->instance(),
            .PhysicalDevice = deviceProps.m_device,
            .Device = vulkanCoreRef->device(),
            .QueueFamily = vulkanCoreRef->graphicsQueueFamilyIndex(),
            .Queue = vulkanCoreRef->graphicsQueue().get(),
            .DescriptorPool = m_descriptorPool,
            .MinImageCount = vulkanCoreRef->physicalDevices().getSurfaceProperties(vulkanCoreRef->physicalDevices().selected(), mainWindowHandler->m_surface).m_surfaceCapabilities.minImageCount,
            .ImageCount = static_cast<uint32_t>(mainWindowHandler->m_swapChain.numImages()),
            .PipelineCache = VK_NULL_HANDLE,
            .PipelineInfoMain = {
                .RenderPass = VK_NULL_HANDLE, // Ignored as using dynamic rendering
                .Subpass = 0,
                .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
                .PipelineRenderingCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                    .pNext = nullptr,
                    .viewMask = 0,
                    .colorAttachmentCount = 1,
                    .pColorAttachmentFormats = &colourFormat,
                    .depthAttachmentFormat = deviceProps.m_depthFormat,
                    .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
                }
            },
            .UseDynamicRendering = vulkanCoreRef->usesDynamicRendering(),
            .Allocator = nullptr,
            .CheckVkResultFn = checkVKResult,
            .MinAllocationSize = (1024 * 1024)
        };
        MARK_INFO_C(Utils::Category::imgui,
            "ImGui: UseDynamicRendering = %s, ApiVersion = %u.%u",
            vulkanCoreRef->usesDynamicRendering() ? "true" : "false",
            VK_API_VERSION_MAJOR(initInfo.ApiVersion),
            VK_API_VERSION_MINOR(initInfo.ApiVersion));

        ImGui_ImplVulkan_Init(&initInfo);

        mainWindowHandler->m_vulkanCommandBuffers.createCommandBuffers(mainWindowHandler->m_swapChain.numImages(), m_commandBuffers);
    }

    void ImGuiRenderer::destroy()
    {
        if (m_commandBuffers.empty() && m_descriptorPool == VK_NULL_HANDLE)
            return;

        if (!m_imguiHandler.m_vulkanCoreRef || !m_imguiHandler.m_mainWindowHandler) {
            MARK_ERROR("Reference expired, cannot destroy ImGui");
        }

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        m_imguiHandler.m_mainWindowHandler->m_vulkanCommandBuffers.freeCommandBuffers(m_commandBuffers.size(), m_commandBuffers.data());
        m_commandBuffers.clear();

        vkDestroyDescriptorPool(m_imguiHandler.m_vulkanCoreRef->device(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;

        MARK_INFO_C(Utils::Category::imgui, "ImGui Vulkan Renderer destroyed");
    }

    VkCommandBuffer ImGuiRenderer::prepareCommandBuffer(uint32_t _imageIndex)
    {
        VulkanCommandBuffers& commandBufferHandler = m_imguiHandler.m_mainWindowHandler->m_vulkanCommandBuffers;

        if (_imageIndex >= m_commandBuffers.size())
            MARK_ERROR("ImGui command buffer index out of range after swapchain rebuild");

        commandBufferHandler.beginCommandBuffer(m_commandBuffers[_imageIndex], VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        commandBufferHandler.beginDynamicRendering(m_commandBuffers[_imageIndex], _imageIndex, nullptr, nullptr, false);

        ImDrawData* drawData = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(drawData, m_commandBuffers[_imageIndex]);

        commandBufferHandler.endDynamicRendering(m_commandBuffers[_imageIndex], _imageIndex, true);

        return m_commandBuffers[_imageIndex];
    }
}