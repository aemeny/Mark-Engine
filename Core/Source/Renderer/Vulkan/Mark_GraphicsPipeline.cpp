#include "Mark_GraphicsPipeline.h"
#include "Mark_VulkanCore.h"
#include "Mark_Shader.h"
#include "Mark_PhysicalDevices.h"
#include "Utils/VulkanUtils.h"
#include "Utils/Mark_Utils.h"

namespace Mark::RendererVK
{
    static void validatePipelineDesc(const PipelineDesc& _desc)
    {
        if (_desc.device == VK_NULL_HANDLE) {
            MARK_FATAL(Utils::Category::Vulkan, "PipelineDesc.device is VK_NULL_HANDLE");
        }
        if (_desc.vertexShader == VK_NULL_HANDLE || _desc.fragmentShader == VK_NULL_HANDLE) {
            MARK_FATAL(Utils::Category::Vulkan, "PipelineDesc shaders not set (Shader module is VK_NULL_HANDLE)");
        }
        if (_desc.renderTargetsDesc.colourFormats.empty()) {
            MARK_FATAL(Utils::Category::Vulkan, "PipelineDesc.renderTargetsDesc.colourFormats is empty. "
                "Dynamic rendering requires at least one colour attachment format");
        }

        const bool wantsDepth = _desc.depthStencilDesc.depthTestEnable  ||
                                _desc.depthStencilDesc.depthWriteEnable ||
                                _desc.depthStencilDesc.stencilTestEnable;

        if (wantsDepth && _desc.renderTargetsDesc.depthFormat == VK_FORMAT_UNDEFINED) {
            MARK_FATAL(Utils::Category::Vulkan, "PipelineDesc uses depth/stencil state but renderTargetsDesc.depthFormat is VK_FORMAT_UNDEFINED");
        }

        if (_desc.inputAssemblyDesc.topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST &&
            _desc.inputAssemblyDesc.patchControlPoints == 0) {
            MARK_FATAL(Utils::Category::Vulkan, "PipelineDesc topology is PATCH_LIST but patchControlPoints == 0. "
                "Set inputAssemblyDesc.patchControlPoints to > 0 when using tessellation");
        }
    }
    

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

    void VulkanGraphicsPipeline::bindPipeline(VkCommandBuffer _cmdBuffer)
    {
        vkCmdBindPipeline(_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cachedRef.get());
    }

    void VulkanGraphicsPipeline::createGraphicsPipeline(const PipelineDesc& _pipelineDesc)
    {
        if (m_set0Layout == VK_NULL_HANDLE || m_set0LayoutHash == 0) {
            MARK_FATAL(Utils::Category::Vulkan,
                "VulkanGraphicsPipeline::createGraphicsPipeline called without set0 resource layout. "
                "Call setResourceLayout(setLayout, hash) first.");
        }

        validatePipelineDesc(_pipelineDesc);

        const auto samples = _pipelineDesc.multisampleDesc.rasterizationSamples;

        VulkanGraphicsPipelineKey key = VulkanGraphicsPipelineKey::Make(_pipelineDesc, m_set0LayoutHash);

        // Acquire from the device graphics-pipeline cache with a creation lambda
        m_cachedRef = _pipelineDesc.cache.acquire(
            key,
            [&](const VulkanGraphicsPipelineKey& _key)->GraphicsPipelineCreateResult
            {
                VkPipelineLayout layout = VK_NULL_HANDLE;
                VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                    .setLayoutCount = 1u ,
                    .pSetLayouts = &m_set0Layout
                };

                VkResult res = vkCreatePipelineLayout(_pipelineDesc.device, &pipelineLayoutInfo, nullptr, &layout);
                CHECK_VK_RESULT(res, "Failed to create pipeline layout");
                MARK_VK_NAME(_pipelineDesc.device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, layout, ("VulkanPipeline." + _pipelineDesc.debugName + ".PipeLayout").c_str());

                // Shader stages
                VkPipelineShaderStageCreateInfo shaderStageCreateInfo[2] = {
                    {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage = VK_SHADER_STAGE_VERTEX_BIT,
                        .module = _pipelineDesc.vertexShader,
                        .pName = "main"
                    },
                    {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .module = _pipelineDesc.fragmentShader,
                        .pName = "main"
                    }
                };

                const auto& dynStatesVec = _pipelineDesc.dynamicDesc.states;
                VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = { 
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = static_cast<uint32_t>(dynStatesVec.size()),
                    .pDynamicStates = dynStatesVec.empty() ? nullptr : dynStatesVec.data()
                };

                VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = { 
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO 
                };

                VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = { 
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                    .topology = _pipelineDesc.inputAssemblyDesc.topology,
                    .primitiveRestartEnable = _pipelineDesc.inputAssemblyDesc.primitiveRestartEnable ? VK_TRUE : VK_FALSE
                };
                
                VkPipelineRasterizationStateCreateInfo rasterizeCreateInfo = { 
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                    .depthClampEnable = _pipelineDesc.rasterDesc.depthClampEnable ? VK_TRUE : VK_FALSE,
                    .rasterizerDiscardEnable = _pipelineDesc.rasterDesc.rasterizerDiscardEnable ? VK_TRUE : VK_FALSE,
                    .polygonMode = _pipelineDesc.rasterDesc.polygonMode,
                    .cullMode = _pipelineDesc.rasterDesc.cullMode,
                    .frontFace = _pipelineDesc.rasterDesc.frontFace,
                    .depthBiasEnable = _pipelineDesc.rasterDesc.depthBiasEnable ? VK_TRUE : VK_FALSE,
                    .depthBiasConstantFactor = _pipelineDesc.rasterDesc.depthBiasConstantFactor,
                    .depthBiasClamp = _pipelineDesc.rasterDesc.depthBiasClamp,
                    .depthBiasSlopeFactor = _pipelineDesc.rasterDesc.depthBiasSlopeFactor,
                    .lineWidth = _pipelineDesc.rasterDesc.lineWidth
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

                const uint32_t sampleCount = static_cast<uint32_t>(_pipelineDesc.multisampleDesc.rasterizationSamples);
                const uint32_t neededMaskCount = (sampleCount + 31u) / 32u;
                if (neededMaskCount > static_cast<uint32_t>(_pipelineDesc.multisampleDesc.sampleMask.size())) {
                    MARK_FATAL(Utils::Category::Vulkan, "multisampleDesc.sampleMask only supports up to 64 samples");
                }
                VkPipelineMultisampleStateCreateInfo multisampleInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    .rasterizationSamples = _pipelineDesc.multisampleDesc.rasterizationSamples,
                    .sampleShadingEnable = _pipelineDesc.multisampleDesc.sampleShadingEnable ? VK_TRUE : VK_FALSE,
                    .minSampleShading = _pipelineDesc.multisampleDesc.minSampleShading,
                    .pSampleMask = _pipelineDesc.multisampleDesc.sampleMask.data(),
                    .alphaToCoverageEnable = _pipelineDesc.multisampleDesc.alphaToCoverageEnable ? VK_TRUE : VK_FALSE,
                    .alphaToOneEnable = _pipelineDesc.multisampleDesc.alphaToOneEnable ? VK_TRUE : VK_FALSE
                };

                VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                    .depthTestEnable = _pipelineDesc.depthStencilDesc.depthTestEnable ? VK_TRUE : VK_FALSE,
                    .depthWriteEnable = _pipelineDesc.depthStencilDesc.depthWriteEnable ? VK_TRUE : VK_FALSE,
                    .depthCompareOp = _pipelineDesc.depthStencilDesc.depthCompareOp,
                    .depthBoundsTestEnable = _pipelineDesc.depthStencilDesc.depthBoundsTestEnable ? VK_TRUE : VK_FALSE,
                    .stencilTestEnable = _pipelineDesc.depthStencilDesc.stencilTestEnable ? VK_TRUE : VK_FALSE,
                    .front = _pipelineDesc.depthStencilDesc.front,
                    .back = _pipelineDesc.depthStencilDesc.back,
                    .minDepthBounds = _pipelineDesc.depthStencilDesc.minDepthBounds,
                    .maxDepthBounds = _pipelineDesc.depthStencilDesc.maxDepthBounds
                };

                // Blend attachments must match colour attachment count
                const uint32_t colourCount = static_cast<uint32_t>(_pipelineDesc.renderTargetsDesc.colourFormats.size());
                std::vector<PipelineBlendAttachmentDesc> blendIn = _pipelineDesc.blendDesc.attachments;
                if (blendIn.empty()) {
                    blendIn.resize(colourCount); // Defaults are opaque as enable=false by default
                }
                else if (blendIn.size() == 1 && colourCount > 1) {
                    blendIn.resize(colourCount, blendIn[0]);
                }
                else if (blendIn.size() != colourCount) {
                    MARK_FATAL(Utils::Category::Vulkan, "PipelineDesc.blendDesc.attachments size must be 0, 1, or equal to renderTargetsDesc.colourFormats size.");
                }
                
                std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
                blendAttachments.reserve(blendIn.size());
                for (const auto& attachment : blendIn)
                {
                    VkPipelineColorBlendAttachmentState out = {
                        .blendEnable = attachment.enable ? VK_TRUE : VK_FALSE,
                        .srcColorBlendFactor = attachment.srcColour,
                        .dstColorBlendFactor = attachment.dstColour,
                        .colorBlendOp = attachment.colourOp,
                        .srcAlphaBlendFactor = attachment.srcAlpha,
                        .dstAlphaBlendFactor = attachment.dstAlpha,
                        .alphaBlendOp = attachment.alphaOp,
                        .colorWriteMask = attachment.colorWriteMask
                    };

                    blendAttachments.push_back(out);
                }

                VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                    .logicOpEnable = _pipelineDesc.blendDesc.logicOpEnable ? VK_TRUE : VK_FALSE,
                    .logicOp = _pipelineDesc.blendDesc.logicOp,
                    .attachmentCount = static_cast<uint32_t>(blendAttachments.size()),
                    .pAttachments = blendAttachments.data()
                };
                colorBlendCreateInfo.blendConstants[0] = _pipelineDesc.blendDesc.blendConstants[0];
                colorBlendCreateInfo.blendConstants[1] = _pipelineDesc.blendDesc.blendConstants[1];
                colorBlendCreateInfo.blendConstants[2] = _pipelineDesc.blendDesc.blendConstants[2];
                colorBlendCreateInfo.blendConstants[3] = _pipelineDesc.blendDesc.blendConstants[3];


                VkPipelineRenderingCreateInfo renderingInfo{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    .pNext = nullptr,
                    .viewMask = _pipelineDesc.renderTargetsDesc.viewMask,
                    .colorAttachmentCount = static_cast<uint32_t>(_pipelineDesc.renderTargetsDesc.colourFormats.size()),
                    .pColorAttachmentFormats = _pipelineDesc.renderTargetsDesc.colourFormats.data(),
                    .depthAttachmentFormat = _pipelineDesc.renderTargetsDesc.depthFormat,
                    .stencilAttachmentFormat = _pipelineDesc.renderTargetsDesc.stencilFormat
                };

                VkPipelineTessellationStateCreateInfo tessInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
                    .patchControlPoints = _pipelineDesc.inputAssemblyDesc.patchControlPoints
                };
                const bool useTess = (_pipelineDesc.inputAssemblyDesc.topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST) &&
                                     (_pipelineDesc.inputAssemblyDesc.patchControlPoints > 0);

                VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = &renderingInfo,
                    .stageCount = ARRAY_COUNT(shaderStageCreateInfo),
                    .pStages = &shaderStageCreateInfo[0],
                    .pVertexInputState = &vertexInputCreateInfo,
                    .pInputAssemblyState = &inputAssemblyCreateInfo,
                    .pTessellationState = useTess ? &tessInfo : nullptr,
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
                res = vkCreateGraphicsPipelines(_pipelineDesc.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &out.m_pipeline);
                CHECK_VK_RESULT(res, "Create Graphics Pipeline");
                MARK_VK_NAME(_pipelineDesc.device, VK_OBJECT_TYPE_PIPELINE, out.m_pipeline, ("VulkanPipeline." + _pipelineDesc.debugName + ".GraphicsPipe").c_str());

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
} // namespace Mark::RendererVK