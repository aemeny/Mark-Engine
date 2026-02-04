#include "Mark_VulkanGraphicsPipeline.h"
#include "Mark_VulkanCore.h"
#include "Mark_VulkanShader.h"
#include "Mark_VulkanPhysicalDevices.h"
#include "Utils/VulkanUtils.h"
#include "Utils/Mark_Utils.h"

namespace Mark::RendererVK
{
    // Hash helper for descriptor bindings (order-independent by binding index)
    static uint64_t HashBindings(const std::vector<VkDescriptorSetLayoutBinding>& _bindings)
    {
        // FNV-1a 64-bit
        uint64_t h = 1469598103934665603ull;
        auto mix = [&h](uint64_t _v)
        {
            h ^= _v;
            h *= 1099511628211ull;
        };

        // Deterministic order
        std::vector<VkDescriptorSetLayoutBinding> temp = _bindings;
        std::sort(temp.begin(), temp.end(), [](auto& _a, auto& _b) 
            { return _a.binding < _b.binding; }
        );

        for (const auto& b : temp)
        {
            mix(b.binding);
            mix(static_cast<uint64_t>(b.descriptorType));
            mix(b.descriptorCount);
            mix(static_cast<uint64_t>(b.stageFlags));
        }
        return h;
    }

    VulkanGraphicsPipeline::VulkanGraphicsPipeline(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef, VulkanUniformBuffer& _uniformBufferRef, const std::vector<std::shared_ptr<MeshHandler>>* _meshesToDraw) :
        m_vulkanCoreRef(_vulkanCoreRef), m_swapChainRef(_swapChainRef), m_uniformBufferRef(_uniformBufferRef), m_meshesToDraw(_meshesToDraw)
    {}

    void VulkanGraphicsPipeline::destroyGraphicsPipeline()
    {
        // Drop cache ref (decrements refcount inside the cache)
        m_cachedRef = {};

        if (m_vulkanCoreRef.expired())
        {
            MARK_LOG_ERROR_C(Utils::Category::Vulkan, "VulkanCore reference expired, cannot destroy graphics pipeline");
        }
        auto device = m_vulkanCoreRef.lock()->device();

        if (m_descriptorSetLayout) {
            vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
            m_descriptorSetLayout = VK_NULL_HANDLE;
        }
        if (m_descriptorPool) {
            vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }
        m_descriptorSets.clear();

        m_pipelineLayout = VK_NULL_HANDLE;

        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Graphics Pipeline Destroyed");
    }

    void VulkanGraphicsPipeline::createGraphicsPipeline()
    {
        auto VkCore = m_vulkanCoreRef.lock();
        VkDevice device = VkCore->device();

        // ---=== Temporary area for future Engine side handling ==---
        // Get shader modules from the device shader cache
        const auto vsPath = VkCore->assetPath("Shaders/TriangleTest.vert");
        const auto fsPath = VkCore->assetPath("Shaders/TriangleTest.frag");
        VkShaderModule vs = VkCore->shaderCache().getOrCreateFromGLSL(vsPath.string().c_str());
        VkShaderModule fs = VkCore->shaderCache().getOrCreateFromGLSL(fsPath.string().c_str());
        if (!vs || !fs)
        {
            MARK_LOG_ERROR_C(Utils::Category::Vulkan, "Failed to load shaders");
            return;
        }

        // Create descriptor sets for each mesh we have to draw if preloading meshes
        if (m_meshesToDraw && !m_meshesToDraw->empty()) {
            MARK_INFO_C(Utils::Category::Vulkan, "Preloading meshes for graphics pipeline");
            m_meshCount = static_cast<uint32_t>(m_meshesToDraw->size());
            createDescriptorSets(device);
        }
        else {
            m_meshCount = 0;
        }

        // Ensure descriptor set layout exists for hashing
        if (m_descriptorSetLayout == VK_NULL_HANDLE)
            createDescriptorSetLayout(device);

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
            m_descSetLayoutHash
        );

        // Acquire from the device graphics-pipeline cache with a creation lambda
        m_cachedRef = VkCore->graphicsPipelineCache().acquire(
            key,
            [&](const VulkanGraphicsPipelineKey& _key)->GraphicsPipelineCreateResult
            {
                VkPipelineLayout layout = VK_NULL_HANDLE;
                VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                    .setLayoutCount = m_descriptorSetLayout ? 1u : 0u,
                    .pSetLayouts = m_descriptorSetLayout ? &m_descriptorSetLayout : nullptr
                };

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

                out.m_layout = layout;
                return out;
            });

        if (!m_cachedRef) 
        {
            MARK_ERROR("Failed to create/acquire graphics pipeline");
        }

        // Keep convenience handle for existing callers
        m_pipelineLayout = m_cachedRef.layout();
        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Graphics Pipeline Created");
    }

    void VulkanGraphicsPipeline::bindPipeline(VkCommandBuffer _cmdBuffer, uint32_t _imageIndex, uint32_t _meshIndex)
    {
        vkCmdBindPipeline(_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cachedRef.get());

        if (!m_descriptorSets.empty() && m_meshCount) 
        {
            const uint32_t index = _imageIndex * m_meshCount + _meshIndex;
            if (index < m_descriptorSets.size()) 
            {
                vkCmdBindDescriptorSets(_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                    0, // firstSet
                    1, // descriptorSetCount
                    &m_descriptorSets[index],
                    0, // dynamicOffsetCount
                    nullptr // pDynamicOffsets
                );
            }
        }
    }

    void VulkanGraphicsPipeline::rebuildDescriptors()
    {
        auto VkCore = m_vulkanCoreRef.lock();
        VkDevice device = VkCore->device();

        // Destroy old descriptor objects
        if (m_descriptorSetLayout) { 
            vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr); 
            m_descriptorSetLayout = VK_NULL_HANDLE; 
        }
        if (m_descriptorPool) { 
            vkDestroyDescriptorPool(device, m_descriptorPool, nullptr); 
            m_descriptorPool = VK_NULL_HANDLE; 
        }
        m_descriptorSets.clear();

        m_meshCount = m_meshesToDraw ? static_cast<uint32_t>(m_meshesToDraw->size()) : 0;
        if (m_meshCount) {
            createDescriptorSets(device);
        }
    }

    void VulkanGraphicsPipeline::createDescriptorSets(VkDevice _device)
    {
        const uint32_t numImages = static_cast<uint32_t>(m_swapChainRef.numImages());
        const uint32_t totalSets = numImages * m_meshCount;

        createDescriptorPool(totalSets, _device);
        createDescriptorSetLayout(_device);
        allocateDescriptorSets(totalSets, _device);
        updateDescriptorSets(numImages, totalSets, _device);
    }

    void VulkanGraphicsPipeline::createDescriptorPool(uint32_t _numSets, VkDevice _device)
    {
        std::vector<VkDescriptorPoolSize> sizes;
        sizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _numSets * 2 }); // vertex + index SSBO per set (0)
        if (m_uniformBufferRef.bufferCount() > 0) {
            sizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _numSets }); // binding UBO per set (2)
        }
        sizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _numSets }); // Binding texture (3)

        VkDescriptorPoolCreateInfo poolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = 0,
            .maxSets = _numSets,
            .poolSizeCount = static_cast<uint32_t>(sizes.size()),
            .pPoolSizes = sizes.data()
        };

        VkResult res = vkCreateDescriptorPool(_device, &poolCreateInfo, nullptr, &m_descriptorPool);
        CHECK_VK_RESULT(res, "Create Descriptor Pool");
        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Descriptor Pool Created");
    }

    void VulkanGraphicsPipeline::createDescriptorSetLayout(VkDevice _device)
    {
        if (m_descriptorSetLayout != VK_NULL_HANDLE)
            return;
        m_bindings.clear();

        VkDescriptorSetLayoutBinding vertexShaderLayoutBinding_VB = {
            .binding = Binding::verticesSSBO,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        };
        m_bindings.push_back(vertexShaderLayoutBinding_VB);

        VkDescriptorSetLayoutBinding indexBufferBinding = {
            .binding = Binding::indicesSSBO,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        };
        m_bindings.push_back(indexBufferBinding);

        VkDescriptorSetLayoutBinding vertexShaderLayoutBinding_UBO = {
            .binding = Binding::UBO,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        };
        if (m_uniformBufferRef.bufferCount() > 0) {
            m_bindings.push_back(vertexShaderLayoutBinding_UBO);
        }

        VkDescriptorSetLayoutBinding fragmentShaderLayoutBinding = {
            .binding = Binding::texture,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        };
        m_bindings.push_back(fragmentShaderLayoutBinding);

        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0, // Reserved - so must be 0
            .bindingCount = static_cast<uint32_t>(m_bindings.size()),
            .pBindings = m_bindings.data()
        };

        VkResult res = vkCreateDescriptorSetLayout(_device, &layoutCreateInfo, nullptr, &m_descriptorSetLayout);
        CHECK_VK_RESULT(res, "Create Descriptor Set Layout");

        // Hash the bindings for future reference
        m_descSetLayoutHash = HashBindings(m_bindings);
    }

    void VulkanGraphicsPipeline::allocateDescriptorSets(uint32_t _numSets, VkDevice _device)
    {
        std::vector<VkDescriptorSetLayout> layouts(_numSets, m_descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = m_descriptorPool,
            .descriptorSetCount = _numSets,
            .pSetLayouts = layouts.data()
        };

        m_descriptorSets.resize(_numSets);

        VkResult res = vkAllocateDescriptorSets(_device, &allocInfo, m_descriptorSets.data());
        CHECK_VK_RESULT(res, "Allocate Descriptor Sets");
    }

    void VulkanGraphicsPipeline::updateDescriptorSets(uint32_t _numImages, uint32_t _numSets, VkDevice _device)
    {
        // One SSBO info per set; one UBO info per image
        std::vector<VkDescriptorBufferInfo> ssboInfos(_numSets);
        std::vector<VkDescriptorBufferInfo> indexInfos(_numSets);
        std::vector<VkDescriptorBufferInfo> uboInfos;
        std::vector<VkWriteDescriptorSet> writes;

        const bool hasUBO = (m_uniformBufferRef.bufferCount() > 0);
        if (hasUBO) 
        {
            uboInfos.resize(_numImages);
            for (uint32_t img = 0; img < _numImages; img++) {
                uboInfos[img] = m_uniformBufferRef.descriptorInfo(img);
            }
        }
        writes.reserve(_numSets * (hasUBO ? 2u : 1u));

        for (uint32_t img = 0; img < _numImages; img++)
        {
            for (uint32_t meshIndex = 0; meshIndex < m_meshCount; meshIndex++)
            {
                const uint32_t index = img * m_meshCount + meshIndex;
                const auto& mesh = *m_meshesToDraw->at(meshIndex);
                const auto texture = mesh.texture();

                if (!mesh.hasVertexBuffer()) {
                    MARK_ERROR("Mesh has no vertex GPU buffer to bind to descriptor set");
                }
                ssboInfos[index] = {
                    .buffer = mesh.vertexBuffer(),
                    .offset = 0,
                    .range = VK_WHOLE_SIZE
                };
                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = m_descriptorSets[index],
                    .dstBinding = Binding::verticesSSBO,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &ssboInfos[index],
                    .pTexelBufferView = nullptr
                });

                if (!mesh.hasIndexBuffer()) {
                    MARK_ERROR("Mesh has no index GPU buffer to bind to descriptor set");
                }
                indexInfos[index] = {
                    .buffer = mesh.indexBuffer(),
                    .offset = 0,
                    .range = VK_WHOLE_SIZE
                };
                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = m_descriptorSets[index],
                    .dstBinding = Binding::indicesSSBO,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &indexInfos[index],
                    .pTexelBufferView = nullptr
                });

                if (hasUBO) 
                {
                    writes.push_back(VkWriteDescriptorSet{
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = m_descriptorSets[index],
                        .dstBinding = Binding::UBO,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .pImageInfo = nullptr,
                        .pBufferInfo = &uboInfos[img],
                        .pTexelBufferView = nullptr
                    });
                }

                if (texture) 
                {
                    VkDescriptorImageInfo imageInfo = {
                        .sampler = texture->sampler(),
                        .imageView = texture->imageView(),
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    };

                    writes.push_back(VkWriteDescriptorSet{
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = m_descriptorSets[index],
                        .dstBinding = Binding::texture,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .pImageInfo = &imageInfo,
                        .pBufferInfo = nullptr,
                        .pTexelBufferView = nullptr
                    });
                }
            }
        }
        vkUpdateDescriptorSets(_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

} // namespace Mark::RendererVK