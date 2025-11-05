#include "MarkVulkanGraphicsPipeline.h"
#include "MarkVulkanCore.h"
#include "MarkVulkanShader.h"
#include "Utils/VulkanUtils.h"
#include "Utils/MarkUtils.h"

namespace Mark::RendererVK
{
    VulkanGraphicsPipeline::VulkanGraphicsPipeline(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef, VulkanRenderPass& _renderPassRef) :
        m_vulkanCoreRef(_vulkanCoreRef), m_renderPassRef(_renderPassRef), m_swapChainRef(_swapChainRef)
    {}

    void VulkanGraphicsPipeline::destroyGraphicsPipeline()
    {
        // Drop cache ref (decrements refcount inside the cache)
        m_cachedRef = {};
        m_pipelineLayout = VK_NULL_HANDLE;

        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Graphics Pipeline Destroyed");
    }

    void VulkanGraphicsPipeline::createGraphicsPipeline()
    {
        auto VkCore = m_vulkanCoreRef.lock();
        VkDevice device = VkCore->device();

        // Get shader modules from the device shader cache
        VkShaderModule vs = VkCore->shaderCache().getOrCreateFromGLSL("Assets/Shaders/Triangle.vert");
        VkShaderModule fs = VkCore->shaderCache().getOrCreateFromGLSL("Assets/Shaders/Triangle.frag");
        if (!vs || !fs)
        {
            MARK_LOG_ERROR_C(Utils::Category::Vulkan, "Failed to load shaders");
            return;
        }

        // Cache key for this render-target + program + baked state
        const VkRenderPass renderPass = m_renderPassRef.renderPass();

        const auto samples = VK_SAMPLE_COUNT_1_BIT;
        auto key = VulkanGraphicsPipelineKey::Make(
            renderPass, 
            vs, fs,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 
            samples
        );

        // Acquire from the device graphics-pipeline cache with a creation lambda
        m_cachedRef = VkCore->graphicsPipelineCache().acquire(
            key,
            [&](const VulkanGraphicsPipelineKey& _key)->GraphicsPipelineCreateResult
            {
                VkPipelineLayout layout = VK_NULL_HANDLE;
                VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

                VkResult res = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout);
                CHECK_VK_RESULT(res, "Failed to create pipeline layout");

                // Shader stages
                VkPipelineShaderStageCreateInfo shaderStageCreateInfo[2] = {
                    {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage = VK_SHADER_STAGE_VERTEX_BIT,
                        .module = _key.m_vertShader,
                        .pName = "main"
                    },
                    {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .module = _key.m_fragShader,
                        .pName = "main"
                    }
                };

                // Dynamic viewport/scissor so one pipeline works for any window size
                VkDynamicState dynStates[] = { 
                    VK_DYNAMIC_STATE_VIEWPORT,
                    VK_DYNAMIC_STATE_SCISSOR 
                };
                VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = { 
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = ARRAY_COUNT(dynStates),
                    .pDynamicStates = dynStates
                };

                VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = { 
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO 
                };

                VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = { 
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                    .topology = _key.m_topology,
                    .primitiveRestartEnable = VK_FALSE
                };
                
                VkPipelineRasterizationStateCreateInfo rasterizeCreateInfo = { 
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                    .polygonMode = VK_POLYGON_MODE_FILL,
                    .cullMode = VK_CULL_MODE_BACK_BIT,
                    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                    .lineWidth = 1.0f
                };
                
                // Viewport state with counts; values ignored when dynamic
                VkViewport dummyViewport{ 0,0,1,1,0.f,1.f };
                VkRect2D dummyScissor{ {0,0},{1,1} };
                VkPipelineViewportStateCreateInfo viewportCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                    .viewportCount = 1,
                    .pViewports = &dummyViewport,
                    .scissorCount = 1,
                    .pScissors = &dummyScissor
                };

                VkPipelineMultisampleStateCreateInfo multisampleInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    .rasterizationSamples = _key.m_samples,
                    .sampleShadingEnable = VK_FALSE,
                    .minSampleShading = 1.0f,
                };

                VkPipelineColorBlendAttachmentState colorBlendAttachment = {
                    .blendEnable = VK_FALSE,
                    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
                };

                VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                    .attachmentCount = 1,
                    .pAttachments = &colorBlendAttachment
                };

                VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .stageCount = ARRAY_COUNT(shaderStageCreateInfo),
                    .pStages = &shaderStageCreateInfo[0],
                    .pVertexInputState = &vertexInputCreateInfo,
                    .pInputAssemblyState = &inputAssemblyCreateInfo,
                    .pViewportState = &viewportCreateInfo,
                    .pRasterizationState = &rasterizeCreateInfo,
                    .pMultisampleState = &multisampleInfo,
                    .pColorBlendState = &colorBlendCreateInfo,
                    .layout = layout,
                    .renderPass = _key.m_renderPass,
                    .subpass = 0,
                    .basePipelineHandle = VK_NULL_HANDLE,
                    .basePipelineIndex = -1
                };

                GraphicsPipelineCreateResult out{};
                res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &out.m_pipeline);
                CHECK_VK_RESULT(res, "Create Graphics Pipeline");

                out.m_layout = layout;
                return out;
            });

        if (!m_cachedRef) 
        {
            MARK_ERROR("Failed to create/acquire graphics pipeline");
        }

        // Keep convenience handle for existing callers
        m_pipelineLayout = m_cachedRef.layout();
        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Graphics Pipeline Created (cached)");
    }

    void VulkanGraphicsPipeline::bindPipeline(VkCommandBuffer _cmdBuffer)
    {
        vkCmdBindPipeline(_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cachedRef.get());
    }

} // namespace Mark::RendererVK