#pragma once
#include <Volk/volk.h>
#include <vector>
#include <string>
#include <array>

namespace Mark::RendererVK
{
    // ------------------------------------------------------------
    //  Pipeline description
    //  - Exposes pipeline settings up-front
    //  - Defaults chosen to work for most default opaque rendering
    // ------------------------------------------------------------

    struct PipelineRasterDesc
    {
        VkPolygonMode polygonMode{ VK_POLYGON_MODE_FILL };
        VkCullModeFlags cullMode{ VK_CULL_MODE_BACK_BIT };
        VkFrontFace frontFace{ VK_FRONT_FACE_CLOCKWISE };

        bool depthClampEnable{ false };                // OPTIONAL: Useful for shadow maps / clip control
        bool rasterizerDiscardEnable{ false };         // OPTIONAL: Transform feedback / debuging

        // Depth bias
        bool depthBiasEnable{ false };                 // OPTIONAL: Commonly used for shadow mapping / decals
        float depthBiasConstantFactor{ 0.0f };         // OPTIONAL: Commonly used for shadow mapping / decals
        float depthBiasClamp{ 0.0f };                  // OPTIONAL: Commonly used for shadow mapping / decals
        float depthBiasSlopeFactor{ 0.0f };            // OPTIONAL: Commonly used for shadow mapping / decals

        float lineWidth{ 1.0f };                       // NOTE: >1 requires wideLines feature on many GPUs
    };

    struct PipelineMultisampleDesc
    {
        VkSampleCountFlagBits rasterizationSamples{ VK_SAMPLE_COUNT_1_BIT };

        bool sampleShadingEnable{ false };             // OPTIONAL
        float minSampleShading{ 1.0f };                // OPTIONAL

        std::array<VkSampleMask, 2> sampleMask{        // OPTIONAL: Supports up to 64x MSAA. Length = ceil(samples/32)
            0xFFFFFFFFu,  
            0xFFFFFFFFu 
        }; 
        bool alphaToCoverageEnable{ false };           // OPTIONAL: Useful in alpha-tested foliage
        bool alphaToOneEnable{ false };                // OPTIONAL
    };

    struct PipelineDepthStencilDesc
    {
        // Depth
        bool depthTestEnable{ true };
        bool depthWriteEnable{ true };
        VkCompareOp depthCompareOp{ VK_COMPARE_OP_LESS };

        bool depthBoundsTestEnable{ false };           // OPTIONAL
        float minDepthBounds{ 0.0f };                  // OPTIONAL
        float maxDepthBounds{ 1.0f };                  // OPTIONAL

        // Stencil
        bool stencilTestEnable{ false };               // OPTIONAL
        VkStencilOpState front{};                      // If stencilTestEnable, front must be specified
        VkStencilOpState back{};                       // If stencilTestEnable, back must be specified
    };

    struct PipelineBlendAttachmentDesc
    {
        bool enable{ false };

        VkBlendFactor srcColour{ VK_BLEND_FACTOR_SRC_ALPHA };
        VkBlendFactor dstColour{ VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA };
        VkBlendOp colourOp{ VK_BLEND_OP_ADD };

        VkBlendFactor srcAlpha{ VK_BLEND_FACTOR_ONE };
        VkBlendFactor dstAlpha{ VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA };
        VkBlendOp alphaOp{ VK_BLEND_OP_ADD };

        VkColorComponentFlags colorWriteMask{
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        };
    };

    struct PipelineBlendDesc
    {
        bool logicOpEnable{ false };                   // OPTIONAL
        VkLogicOp logicOp{ VK_LOGIC_OP_COPY };         // OPTIONAL

        float blendConstants[4]{ 0.f, 0.f, 0.f, 0.f }; // OPTIONAL: Used if blend factors reference constants

        // Per-attachment blend states (size should match color attachment count)
        // If empty, engine will assume one default attachment (opaque: enable=false)
        std::vector<PipelineBlendAttachmentDesc> attachments{};
    };

    struct PipelineInputAssemblyDesc
    {
        VkPrimitiveTopology topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
        bool primitiveRestartEnable{ false };          // OPTIONAL: Mostly for strips

        // Tessellation
        uint32_t patchControlPoints{ 0 };              // OPTIONAL: If topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, patchControlPoints must be > 0
    };

    struct PipelineDynamicStateDesc
    {
        // Default: Dynamic viewport + scissor for engine to work for any window size
        std::vector<VkDynamicState> states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
    };

    struct PipelineRenderTargetDesc
    {
        // For dynamic rendering:
        // If colourFormats is empty, engine will fall back to PipelineDesc.m_colourFormat.
        std::vector<VkFormat> colourFormats{};

        VkFormat depthFormat{ VK_FORMAT_UNDEFINED };
        VkFormat stencilFormat{ VK_FORMAT_UNDEFINED }; // OPTIONAL

        uint32_t viewMask{ 0 };                        // OPTIONAL: Multiview
    };

    struct VulkanGraphicsPipelineCache; // Forward declaration
    struct PipelineDesc
    {
        VkDevice device{ VK_NULL_HANDLE }; // Vulkan engine device
        VulkanGraphicsPipelineCache& cache; //Reference to cache for internal setup
        VkShaderModule vertexShader{ VK_NULL_HANDLE };
        VkShaderModule fragmentShader{ VK_NULL_HANDLE };

        std::string debugName{ "Unnamed" };

        PipelineRenderTargetDesc   renderTargetsDesc{};
        PipelineInputAssemblyDesc  inputAssemblyDesc{};
        PipelineRasterDesc         rasterDesc{};
        PipelineMultisampleDesc    multisampleDesc{};
        PipelineDepthStencilDesc   depthStencilDesc{};
        PipelineBlendDesc          blendDesc{};
        PipelineDynamicStateDesc   dynamicDesc{};
    };
}
