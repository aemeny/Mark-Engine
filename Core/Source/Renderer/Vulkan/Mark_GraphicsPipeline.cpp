#include "Mark_GraphicsPipeline.h"
#include "Mark_VulkanCore.h"
#include "Mark_Shader.h"
#include "Mark_PhysicalDevices.h"
#include "Utils/VulkanUtils.h"
#include "Utils/Mark_Utils.h"

#include <algorithm>

namespace Mark::RendererVK
{
    // Hash helper for descriptor bindings (order-independent by binding index)
    static uint64_t HashBindings(const std::vector<VkDescriptorSetLayoutBinding>& _bindings,
        const std::vector<VkDescriptorBindingFlags>& _flags, VkDescriptorSetLayoutCreateFlags _layoutFlags)
    {
        // FNV-1a 64-bit
        uint64_t h = 1469598103934665603ull;
        auto mix = [&h](uint64_t _v)
        {
            h ^= _v;
            h *= 1099511628211ull;
        };

        mix(static_cast<uint64_t>(_layoutFlags));

        // Deterministic order
        struct Entry { VkDescriptorSetLayoutBinding b; VkDescriptorBindingFlags f; };
        std::vector<Entry> temp;
        temp.reserve(_bindings.size());
        for (size_t i = 0; i < _bindings.size(); i++) 
        {
            VkDescriptorBindingFlags f = (i < _flags.size()) ? _flags[i] : 0;
            temp.push_back({ _bindings[i], f });
        }
        std::sort(temp.begin(), temp.end(), [](const Entry& a, const Entry& b) { return a.b.binding < b.b.binding; });

        for (const auto& e : temp)
        {
            mix(e.b.binding);
            mix(static_cast<uint64_t>(e.b.descriptorType));
            mix(e.b.descriptorCount);
            mix(static_cast<uint64_t>(e.b.stageFlags));
            mix(static_cast<uint64_t>(e.f));
        }
        return h;
    }

    static uint32_t GrowPow2Capacity(uint32_t required, uint32_t maxCap)
    {
        required = std::max(1u, required);
        maxCap = std::max(1u, maxCap);

        uint32_t cap = 64u;
        if (cap > maxCap) cap = maxCap;
        while (cap < required && cap < maxCap) cap <<= 1u;
        if (cap < required) cap = required;
        cap = std::min(cap, maxCap);
        return std::max(1u, cap);
    }

    VulkanGraphicsPipeline::VulkanGraphicsPipeline(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef, VulkanUniformBuffer& _uniformBufferRef, const std::vector<std::shared_ptr<MeshHandler>>* _meshesToDraw) :
        m_vulkanCoreRef(_vulkanCoreRef), m_swapChainRef(_swapChainRef), m_uniformBufferRef(_uniformBufferRef), m_meshesToDraw(_meshesToDraw), m_bindlessCaps(_vulkanCoreRef.lock()->bindlessCaps())
    {}

    void VulkanGraphicsPipeline::destroyGraphicsPipeline()
    {
        // Drop cache ref (decrements refcount inside the cache)
        m_cachedRef = {};

        if (m_vulkanCoreRef.expired()) {
            MARK_FATAL(Utils::Category::Vulkan, "VulkanCore reference expired, cannot destroy graphics pipeline");
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

        MARK_INFO(Utils::Category::Vulkan, "Vulkan Graphics Pipeline Destroyed");
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
        if (!vs || !fs) {
            MARK_ERROR(Utils::Category::Vulkan, "Failed to load shaders");
            return;
        }

        m_maxMeshesLayout = m_bindlessCaps.maxMeshes;
        m_maxTexturesLayout = std::min<uint32_t>(4096u, m_bindlessCaps.maxTextureDescriptors);
        if (m_maxTexturesLayout == 0) m_maxTexturesLayout = 1;
        if (m_maxTexturesLayout < m_maxMeshesLayout)
        {
            MARK_WARN(Utils::Category::Vulkan,
                "Bindless: maxTexturesLayout (%u) < maxMeshesLayout (%u). Clamping mesh layout to textures.",
                m_maxTexturesLayout, m_maxMeshesLayout);
            m_maxMeshesLayout = m_maxTexturesLayout / m_bindlessCaps.numAttachableTextures;
        }

        // Create descriptor sets for each mesh we have to draw if preloading meshes
        if (m_meshesToDraw && !m_meshesToDraw->empty()) 
        {
            m_meshCount = static_cast<uint32_t>(m_meshesToDraw->size());
            if (m_meshCount > m_maxMeshesLayout) {
                MARK_FATAL(Utils::Category::Vulkan, "Mesh count (%u) exceeds bindless MAX_MESHES (%u). Clamping.", m_meshCount, m_maxMeshesLayout);
                m_meshCount = m_maxMeshesLayout;
            }
            const uint32_t requiredTextures = std::min(m_meshCount, m_maxTexturesLayout);
            m_textureDescriptorCount = GrowPow2Capacity(requiredTextures, m_maxTexturesLayout);
        }
        else {
            m_meshCount = 0;
            m_textureDescriptorCount = GrowPow2Capacity(0, m_maxTexturesLayout);
        }

        // Ensure descriptor set layout exists for hashing
        createDescriptorSetLayout(device);

        createDescriptorSets(device);

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

    void VulkanGraphicsPipeline::bindPipeline(VkCommandBuffer _cmdBuffer, uint32_t _imageIndex)
    {
        vkCmdBindPipeline(_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cachedRef.get());

        if (!m_descriptorSets.empty()) 
        {
            if (_imageIndex < m_descriptorSets.size())
            {
                vkCmdBindDescriptorSets(_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                    0, // firstSet
                    1, // descriptorSetCount
                    &m_descriptorSets[_imageIndex],
                    0, // dynamicOffsetCount
                    nullptr // pDynamicOffsets
                );
            }
        }
    }

    bool VulkanGraphicsPipeline::tryUpdateDescriptorsWithMesh(const uint32_t _newMeshIndex)
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) return false;
        VkDevice device = VkCore->device();

        // If no sets yet, caller must rebuild
        if (m_descriptorSets.empty() || m_descriptorPool == VK_NULL_HANDLE || m_descriptorSetLayout == VK_NULL_HANDLE)
            return false;
        if (!m_meshesToDraw) return false;
        if (_newMeshIndex >= m_meshesToDraw->size()) return false;

        const uint32_t requiredMeshes = _newMeshIndex + 1u;
        // Can grow until m_maxMeshesLayout
        if (requiredMeshes > m_maxMeshesLayout)
            return false;

        // Textures cannot grow past allocated capacity without reallocating sets.
        if (requiredMeshes > m_textureDescriptorCount)
            return false;

        const auto& mesh = *m_meshesToDraw->at(_newMeshIndex);
        if (!mesh.hasVertexBuffer() || !mesh.hasIndexBuffer()) {
            MARK_FATAL(Utils::Category::Vulkan, "tryAppendMeshAndUpdateDescriptors: mesh %u missing GPU buffers", _newMeshIndex);
            return false;
        }

        VkDescriptorBufferInfo vbInfo{ mesh.vertexBuffer(), 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo ibInfo{ mesh.indexBuffer(), 0, VK_WHOLE_SIZE };

        TextureHandler* tex = mesh.texture();
        VkDescriptorImageInfo imgInfo{};
        if (tex)
        {
            imgInfo = {
                .sampler = tex->sampler(),
                .imageView = tex->imageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
        }

        const uint32_t numImages = static_cast<uint32_t>(m_swapChainRef.numImages());
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(numImages * (2u + (tex ? 1u : 0u)));

        for (uint32_t img = 0; img < numImages; ++img)
        {
            VkDescriptorSet set = m_descriptorSets[img];

            // Vertices SSBO slot
            writes.push_back(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set,
                .dstBinding = Binding::verticesSSBO,
                .dstArrayElement = _newMeshIndex,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pImageInfo = nullptr,
                .pBufferInfo = &vbInfo,
                .pTexelBufferView = nullptr
            });

            // Indices SSBO slot
            writes.push_back(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set,
                .dstBinding = Binding::indicesSSBO,
                .dstArrayElement = _newMeshIndex,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pImageInfo = nullptr,
                .pBufferInfo = &ibInfo,
                .pTexelBufferView = nullptr
            });

            // Texture slot
            if (tex) {
                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set,
                    .dstBinding = Binding::texture,
                    .dstArrayElement = _newMeshIndex,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imgInfo,
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr
                });

            }
        }

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        // Increase mesh count for command buffer recording loops.
        m_meshCount = requiredMeshes;
        return true;
    }

    void VulkanGraphicsPipeline::rebuildDescriptors()
    {
        VkDevice device = m_vulkanCoreRef.lock()->device();

        if (m_descriptorPool) {
            vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }
        m_descriptorSets.clear();

        if (m_maxMeshesLayout == 0)  m_maxMeshesLayout = m_bindlessCaps.maxMeshes;
        if (m_maxTexturesLayout == 0) m_maxTexturesLayout = std::min<uint32_t>(4096u, m_bindlessCaps.maxTextureDescriptors);
        if (m_maxTexturesLayout < m_maxMeshesLayout) 
        {
            MARK_WARN(Utils::Category::Vulkan, "Bindless: maxTexturesLayout (%u) < maxMeshesLayout (%u). Clamping mesh layout to textures for safety.",
                m_maxTexturesLayout, m_maxMeshesLayout);
            m_maxMeshesLayout = m_maxTexturesLayout / m_bindlessCaps.numAttachableTextures;
        }

        m_meshCount = m_meshesToDraw ? static_cast<uint32_t>(m_meshesToDraw->size()) : 0;
        if (m_meshCount > m_maxMeshesLayout) {
            MARK_FATAL(Utils::Category::Vulkan, "Mesh count (%u) exceeds bindless MAX_MESHES (%u). Clamping.", m_meshCount, m_maxMeshesLayout);
            m_meshCount = m_maxMeshesLayout;
        }
        const uint32_t requiredTextures = std::min(m_meshCount, m_maxTexturesLayout);
        m_textureDescriptorCount = GrowPow2Capacity(requiredTextures, m_maxTexturesLayout);

        createDescriptorSets(device);
    }

    void VulkanGraphicsPipeline::createDescriptorSets(VkDevice _device)
    {
        const uint32_t numImages = static_cast<uint32_t>(m_swapChainRef.numImages());

        createDescriptorPool(numImages, _device);
        allocateDescriptorSets(numImages, _device);
        updateDescriptorSets(numImages, _device);
    }

    void VulkanGraphicsPipeline::createDescriptorPool(uint32_t _numImages, VkDevice _device)
    {
        std::vector<VkDescriptorPoolSize> sizes;
        // NOTE: descriptor pool counts are in DESCRIPTORS, not sets.
        // Each set contains:
        //  - binding 0: MAX_MESHES storage buffers
        //  - binding 1: MAX_MESHES storage buffers
        //  - binding 2: 1 UBO
        //  - binding 3: textureDescriptorCount combined image samplers (variable)
        sizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _numImages * m_maxMeshesLayout * 2u });
        sizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _numImages });
        sizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _numImages * m_textureDescriptorCount });

        VkDescriptorPoolCreateInfo poolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = 0,
            .maxSets = _numImages,
            .poolSizeCount = static_cast<uint32_t>(sizes.size()),
            .pPoolSizes = sizes.data()
        };

        VkResult res = vkCreateDescriptorPool(_device, &poolCreateInfo, nullptr, &m_descriptorPool);
        CHECK_VK_RESULT(res, "Create Descriptor Pool");
        MARK_VK_NAME(_device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, m_descriptorPool, "VulkPipeline.DescPool");

        MARK_INFO(Utils::Category::Vulkan, "Vulkan Descriptor Pool Created");
    }

    void VulkanGraphicsPipeline::createDescriptorSetLayout(VkDevice _device)
    {
        if (m_descriptorSetLayout != VK_NULL_HANDLE)
            return;
        m_bindings.clear();
        m_bindingFlags.clear();

        VkDescriptorSetLayoutBinding vertexShaderLayoutBinding_VB = {
            .binding = Binding::verticesSSBO,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = m_maxMeshesLayout,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        };
        m_bindings.push_back(vertexShaderLayoutBinding_VB);
        m_bindingFlags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);

        VkDescriptorSetLayoutBinding indexBufferBinding = {
            .binding = Binding::indicesSSBO,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = m_maxMeshesLayout,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        };
        m_bindings.push_back(indexBufferBinding);
        m_bindingFlags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);

        VkDescriptorSetLayoutBinding vertexShaderLayoutBinding_UBO = {
            .binding = Binding::UBO,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        };
        m_bindings.push_back(vertexShaderLayoutBinding_UBO);
        m_bindingFlags.push_back(0);

        VkDescriptorSetLayoutBinding fragmentShaderLayoutBinding = {
            .binding = Binding::texture,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = m_maxTexturesLayout,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        };
        m_bindings.push_back(fragmentShaderLayoutBinding);
        m_bindingFlags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = static_cast<uint32_t>(m_bindingFlags.size()),
            .pBindingFlags = m_bindingFlags.data()
        };

        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = &bindingFlagsInfo,
            .flags = 0, // no UPDATE_AFTER_BIND
            .bindingCount = static_cast<uint32_t>(m_bindings.size()),
            .pBindings = m_bindings.data()
        };

        VkResult res = vkCreateDescriptorSetLayout(_device, &layoutCreateInfo, nullptr, &m_descriptorSetLayout);
        CHECK_VK_RESULT(res, "Create Descriptor Set Layout");
        MARK_VK_NAME(_device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, m_descriptorSetLayout, "VulkPipeline.DescSetLayout");

        // Hash the bindings for future reference
        m_descSetLayoutHash = HashBindings(m_bindings, m_bindingFlags, layoutCreateInfo.flags);
    }

    void VulkanGraphicsPipeline::allocateDescriptorSets(uint32_t _numImages, VkDevice _device)
    {
        std::vector<VkDescriptorSetLayout> layouts(_numImages, m_descriptorSetLayout);
        std::vector<uint32_t> variableCounts(_numImages, m_textureDescriptorCount);

        VkDescriptorSetVariableDescriptorCountAllocateInfo varInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorSetCount = _numImages,
            .pDescriptorCounts = variableCounts.data()
        };

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = &varInfo,
            .descriptorPool = m_descriptorPool,
            .descriptorSetCount = _numImages,
            .pSetLayouts = layouts.data()
        };

        m_descriptorSets.resize(_numImages);

        VkResult res = vkAllocateDescriptorSets(_device, &allocInfo, m_descriptorSets.data());
        CHECK_VK_RESULT(res, "Allocate Descriptor Sets");
        for (uint32_t i = 0; i < _numImages; i++) {
            MARK_VK_NAME_F(_device, VK_OBJECT_TYPE_DESCRIPTOR_SET, m_descriptorSets[i], "VulkPipeline.DescripSet[%u]", i);
        }
    }

    void VulkanGraphicsPipeline::updateDescriptorSets(uint32_t _numImages, VkDevice _device)
    {
        std::vector<VkDescriptorBufferInfo> ssboInfos(m_meshCount);
        std::vector<VkDescriptorBufferInfo> indexInfos(m_meshCount);
        std::vector<VkDescriptorBufferInfo> uboInfos(_numImages);
        std::vector<VkDescriptorImageInfo> imageInfos(m_textureDescriptorCount);
        std::vector<uint8_t> hasTexture(m_textureDescriptorCount, 0);
        std::vector<VkWriteDescriptorSet> writes;

        for (uint32_t img = 0; img < _numImages; img++) {
            uboInfos[img] = m_uniformBufferRef.descriptorInfo(img);
        }

        // Build mesh buffer infos once
        for (uint32_t meshIndex = 0; meshIndex < m_meshCount; meshIndex++)
        {
            const auto& mesh = *m_meshesToDraw->at(meshIndex);
            if (!mesh.hasVertexBuffer()) MARK_FATAL(Utils::Category::Vulkan, "Mesh has no vertex GPU buffer");
            if (!mesh.hasIndexBuffer())  MARK_FATAL(Utils::Category::Vulkan, "Mesh has no index GPU buffer");

            ssboInfos[meshIndex] = { mesh.vertexBuffer(), 0, VK_WHOLE_SIZE };
            indexInfos[meshIndex] = { mesh.indexBuffer(), 0, VK_WHOLE_SIZE };

            if (meshIndex < m_textureDescriptorCount)
            {
                auto texture = mesh.texture();
                if (!texture) {
                    MARK_WARN(Utils::Category::System ,"Mesh has no texture but shader will sample one.");
                } 
                else 
                {
                    imageInfos[meshIndex] = {
                        .sampler = texture->sampler(),
                        .imageView = texture->imageView(),
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    };
                    hasTexture[meshIndex] = 1;
                }
            }
        }
        writes.reserve(_numImages * (1u + (m_meshCount * 2u) + std::min(m_meshCount, m_textureDescriptorCount)));

        for (uint32_t img = 0; img < _numImages; img++)
        {
            VkDescriptorSet set = m_descriptorSets[img];

            // UBO
            writes.push_back(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set,
                .dstBinding = Binding::UBO,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pImageInfo = nullptr,
                .pBufferInfo = &uboInfos[img],
                .pTexelBufferView = nullptr
            });

            for (uint32_t meshIndex = 0; meshIndex < m_meshCount; meshIndex++)
            {
                // Vertices SSBO
                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set,
                    .dstBinding = Binding::verticesSSBO,
                    .dstArrayElement = meshIndex,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &ssboInfos[meshIndex],
                    .pTexelBufferView = nullptr
                });

                // Indices SSBO
                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set,
                    .dstBinding = Binding::indicesSSBO,
                    .dstArrayElement = meshIndex,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &indexInfos[meshIndex],
                    .pTexelBufferView = nullptr
                });

                // Texture
                if (meshIndex < m_textureDescriptorCount && hasTexture[meshIndex])
                {
                    writes.push_back(VkWriteDescriptorSet{
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = set,
                        .dstBinding = Binding::texture,
                        .dstArrayElement = meshIndex,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .pImageInfo = &imageInfos[meshIndex],
                        .pBufferInfo = nullptr,
                        .pTexelBufferView = nullptr
                    });
                }
            }
        }
        vkUpdateDescriptorSets(_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

} // namespace Mark::RendererVK