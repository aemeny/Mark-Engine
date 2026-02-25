#include "Mark_GraphicsPipeline.h"
#include "Mark_VulkanCore.h"
#include "Mark_Shader.h"
#include "Mark_PhysicalDevices.h"
#include "Utils/VulkanUtils.h"
#include "Utils/Mark_Utils.h"

namespace Mark::RendererVK
{
    VulkanGraphicsPipeline::VulkanGraphicsPipeline(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef) :
        m_vulkanCoreRef(_vulkanCoreRef), m_swapChainRef(_swapChainRef)
    {}

    void VulkanGraphicsPipeline::destroyGraphicsPipeline()
    {
        // Drop cache ref (decrements refcount inside the cache)
        m_cachedRef = {};

        m_pipelineLayout = VK_NULL_HANDLE;

        m_set0Layout = VK_NULL_HANDLE;
        m_set0LayoutHash = 0;
    }

    void VulkanGraphicsPipeline::setResourceLayout(VkDescriptorSetLayout _set0Layout, uint64_t _set0LayoutHash)
    {
        m_set0Layout = _set0Layout;
        m_set0LayoutHash = _set0LayoutHash;
    }

    void VulkanGraphicsPipeline::createGraphicsPipeline()
    {
        auto VkCore = m_vulkanCoreRef.lock();
        VkDevice device = VkCore->device();

        if (m_set0Layout == VK_NULL_HANDLE || m_set0LayoutHash == 0)
        {
            MARK_FATAL(Utils::Category::Vulkan,
                "VulkanGraphicsPipeline::createGraphicsPipeline called without set0 resource layout. "
                "Call setResourceLayout(setLayout, hash) first.");
        }

        // ---=== Temporary area for future Engine side handling ==---
        // Get shader modules from the device shader cache
        const auto vsPath = VkCore->assetPath("Shaders/TriangleTest.vert");
        const auto fsPath = VkCore->assetPath("Shaders/TriangleTest.frag");
        VkShaderModule vs = VkCore->shaderCache().getOrCreateFromGLSL(vsPath.string().c_str());
        VkShaderModule fs = VkCore->shaderCache().getOrCreateFromGLSL(fsPath.string().c_str());
        if (!vs || !fs) {
            MARK_ERROR(Utils::Category::Vulkan, "Failed to load shaders");
            return;
        }

        // Cache key for this render-target + program + baked state
        const VkFormat colourFormat = m_swapChainRef.surfaceFormat().format;
        const VkFormat depthFormat = VkCore->physicalDevices().selected().m_depthFormat;

        const auto samples = VK_SAMPLE_COUNT_1_BIT;
        const uint32_t dynMask = (1u << 0) | (1u << 1); // viewport|scissor

        auto key = VulkanGraphicsPipelineKey::Make(
            colourFormat,
            depthFormat,
            vs, fs,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 
            samples,
            dynMask,
            m_set0LayoutHash
        );

        // Acquire from the device graphics-pipeline cache with a creation lambda
        m_cachedRef = VkCore->graphicsPipelineCache().acquire(
            key,
            [&](const VulkanGraphicsPipelineKey& _key)->GraphicsPipelineCreateResult
            {
                VkPipelineLayout layout = VK_NULL_HANDLE;
                VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                    .setLayoutCount = 1u ,
                    .pSetLayouts = &m_set0Layout
                };

                VkResult res = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout);
                CHECK_VK_RESULT(res, "Failed to create pipeline layout");
                MARK_VK_NAME(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, layout, "VulkPipeline.PipeLayout");

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
                    .cullMode = VK_CULL_MODE_NONE,
                    .frontFace = VK_FRONT_FACE_CLOCKWISE,
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

                VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                    .depthTestEnable = VK_TRUE,
                    .depthWriteEnable = VK_TRUE,
                    .depthCompareOp = VK_COMPARE_OP_LESS,
                    .depthBoundsTestEnable = VK_FALSE,
                    .stencilTestEnable = VK_FALSE,
                    .front = {},
                    .back = {},
                    .minDepthBounds = 0.0f,
                    .maxDepthBounds = 1.0f
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

                VkPipelineRenderingCreateInfo renderingInfo{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    .pNext = nullptr,
                    .viewMask = 0,
                    .colorAttachmentCount = 1,
                    .pColorAttachmentFormats = &_key.m_colourFormat,
                    .depthAttachmentFormat = _key.m_depthFormat,
                    .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
                };

                VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = &renderingInfo,
                    .stageCount = ARRAY_COUNT(shaderStageCreateInfo),
                    .pStages = &shaderStageCreateInfo[0],
                    .pVertexInputState = &vertexInputCreateInfo,
                    .pInputAssemblyState = &inputAssemblyCreateInfo,
                    .pTessellationState = nullptr,
                    .pViewportState = &viewportCreateInfo,
                    .pRasterizationState = &rasterizeCreateInfo,
                    .pMultisampleState = &multisampleInfo,
                    .pDepthStencilState = &depthStencilCreateInfo,
                    .pColorBlendState = &colorBlendCreateInfo,
                    .pDynamicState = &dynamicStateCreateInfo,
                    .layout = layout,
                    .renderPass = VK_NULL_HANDLE,
                    .subpass = 0,
                    .basePipelineHandle = VK_NULL_HANDLE,
                    .basePipelineIndex = -1
                };

                GraphicsPipelineCreateResult out{};
                res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &out.m_pipeline);
                CHECK_VK_RESULT(res, "Create Graphics Pipeline");
                MARK_VK_NAME(device, VK_OBJECT_TYPE_PIPELINE, out.m_pipeline, "VulkPipeline.GraphicsPipe");

                out.m_layout = layout;
                return out;
            });

        if (!m_cachedRef) {
            MARK_FATAL(Utils::Category::Vulkan, "Failed to create/acquire graphics pipeline");
        }

        // Keep convenience handle for existing callers
        m_pipelineLayout = m_cachedRef.layout();
        MARK_INFO(Utils::Category::Vulkan, "Vulkan Graphics Pipeline Created");
    }

    void VulkanGraphicsPipeline::bindPipeline(VkCommandBuffer _cmdBuffer)
    {
        vkCmdBindPipeline(_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cachedRef.get());
    }
} // namespace Mark::RendererVK