#pragma once
#include "Mark_PipelineDescription.h"

#include <volk.h>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <bit>

namespace Mark::RendererVK
{
    struct PipelineStateHash128
    {
        uint64_t a{ 0 };
        uint64_t b{ 0 };
        bool operator==(const PipelineStateHash128& o) const noexcept {return a == o.a && b == o.b; }
    };

    // FNV-1a mixing into 64-bit state
    static inline void HashMix64(uint64_t& h, uint64_t v) noexcept
    {
        constexpr uint64_t kPrime = 1099511628211ull; // FNV-1a constant
        h ^= v;
        h *= kPrime;
    }

    static inline uint64_t HashFloat(float f) noexcept
    {
        return static_cast<uint64_t>(std::bit_cast<uint32_t>(f)); // Hash bit-pattern
    }

    static inline void HashStencilOpState(uint64_t& h, const VkStencilOpState& s) noexcept
    {
        HashMix64(h, static_cast<uint64_t>(s.failOp));
        HashMix64(h, static_cast<uint64_t>(s.passOp));
        HashMix64(h, static_cast<uint64_t>(s.depthFailOp));
        HashMix64(h, static_cast<uint64_t>(s.compareOp));
        HashMix64(h, static_cast<uint64_t>(s.compareMask));
        HashMix64(h, static_cast<uint64_t>(s.writeMask));
        HashMix64(h, static_cast<uint64_t>(s.reference));
    }

    // Builds a normalized 128-bit hash from PipelineDesc contents
    static inline PipelineStateHash128 HashPipelineDescState(const PipelineDesc& d)
    {
        // Two different seeds to produce two independent 64-bit hashes.
        uint64_t ha = 1469598103934665603ull;
        uint64_t hb = 1099511628211ull ^ 0x9E3779B97F4A7C15ull;

        auto mixA = [&](uint64_t v) { HashMix64(ha, v); };
        auto mixB = [&](uint64_t v) { HashMix64(hb, v ^ 0xD6E8FEB86659FD93ull); };
        auto mix = [&](uint64_t v) { mixA(v); mixB(v); };

        // Shaders
        mix(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(d.vertexShader)));
        mix(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(d.fragmentShader)));

        // Render target formats
        mix(static_cast<uint64_t>(d.renderTargetsDesc.viewMask));
        mix(static_cast<uint64_t>(d.renderTargetsDesc.depthFormat));
        mix(static_cast<uint64_t>(d.renderTargetsDesc.stencilFormat));

        mix(static_cast<uint64_t>(d.renderTargetsDesc.colourFormats.size()));
        for (VkFormat f : d.renderTargetsDesc.colourFormats) {
            mix(static_cast<uint64_t>(f));
        }

        const uint32_t colourCount = static_cast<uint32_t>(d.renderTargetsDesc.colourFormats.size());

        // Input assembly + tess
        mix(static_cast<uint64_t>(d.inputAssemblyDesc.topology));
        mix(static_cast<uint64_t>(d.inputAssemblyDesc.primitiveRestartEnable ? 1u : 0u));
        mix(static_cast<uint64_t>(d.inputAssemblyDesc.patchControlPoints));

        // Raster
        mix(static_cast<uint64_t>(d.rasterDesc.polygonMode));
        mix(static_cast<uint64_t>(d.rasterDesc.cullMode));
        mix(static_cast<uint64_t>(d.rasterDesc.frontFace));
        mix(static_cast<uint64_t>(d.rasterDesc.depthClampEnable ? 1u : 0u));
        mix(static_cast<uint64_t>(d.rasterDesc.rasterizerDiscardEnable ? 1u : 0u));

        mix(static_cast<uint64_t>(d.rasterDesc.depthBiasEnable ? 1u : 0u));
        mix(HashFloat(d.rasterDesc.depthBiasConstantFactor));
        mix(HashFloat(d.rasterDesc.depthBiasClamp));
        mix(HashFloat(d.rasterDesc.depthBiasSlopeFactor));
        mix(HashFloat(d.rasterDesc.lineWidth));

        // Multisample
        mix(static_cast<uint64_t>(d.multisampleDesc.rasterizationSamples));
        mix(static_cast<uint64_t>(d.multisampleDesc.sampleShadingEnable ? 1u : 0u));
        mix(HashFloat(d.multisampleDesc.minSampleShading));
        mix(static_cast<uint64_t>(d.multisampleDesc.alphaToCoverageEnable ? 1u : 0u));
        mix(static_cast<uint64_t>(d.multisampleDesc.alphaToOneEnable ? 1u : 0u));

        // Sample mask array contents
        mix(static_cast<uint64_t>(d.multisampleDesc.sampleMask.size()));
        for (auto m : d.multisampleDesc.sampleMask) {
            mix(static_cast<uint64_t>(m));
        }

        // Depth/stencil
        mix(static_cast<uint64_t>(d.depthStencilDesc.depthTestEnable ? 1u : 0u));
        mix(static_cast<uint64_t>(d.depthStencilDesc.depthWriteEnable ? 1u : 0u));
        mix(static_cast<uint64_t>(d.depthStencilDesc.depthCompareOp));
        mix(static_cast<uint64_t>(d.depthStencilDesc.depthBoundsTestEnable ? 1u : 0u));
        mix(HashFloat(d.depthStencilDesc.minDepthBounds));
        mix(HashFloat(d.depthStencilDesc.maxDepthBounds));
        mix(static_cast<uint64_t>(d.depthStencilDesc.stencilTestEnable ? 1u : 0u));
        HashStencilOpState(ha, d.depthStencilDesc.front);
        HashStencilOpState(hb, d.depthStencilDesc.front);
        HashStencilOpState(ha, d.depthStencilDesc.back);
        HashStencilOpState(hb, d.depthStencilDesc.back);

        // Blending
        mix(static_cast<uint64_t>(d.blendDesc.logicOpEnable ? 1u : 0u));
        mix(static_cast<uint64_t>(d.blendDesc.logicOp));
        mix(HashFloat(d.blendDesc.blendConstants[0]));
        mix(HashFloat(d.blendDesc.blendConstants[1]));
        mix(HashFloat(d.blendDesc.blendConstants[2]));
        mix(HashFloat(d.blendDesc.blendConstants[3]));

        std::vector<PipelineBlendAttachmentDesc> blendAtt = d.blendDesc.attachments;
        if (blendAtt.empty()) {
            blendAtt.resize(colourCount);
        }
        else if (blendAtt.size() == 1 && colourCount > 1) {
            blendAtt.resize(colourCount, blendAtt[0]);
        }
        else if (colourCount != 0 && blendAtt.size() != colourCount) {
            mix(static_cast<uint64_t>(0xBADB1EADu));
        }

        mix(static_cast<uint64_t>(blendAtt.size()));
        for (const auto& a : blendAtt)
        {
            mix(static_cast<uint64_t>(a.enable ? 1u : 0u));
            mix(static_cast<uint64_t>(a.srcColour));
            mix(static_cast<uint64_t>(a.dstColour));
            mix(static_cast<uint64_t>(a.colourOp));
            mix(static_cast<uint64_t>(a.srcAlpha));
            mix(static_cast<uint64_t>(a.dstAlpha));
            mix(static_cast<uint64_t>(a.alphaOp));
            mix(static_cast<uint64_t>(a.colorWriteMask));
        }

        // Dynamic states
        std::vector<VkDynamicState> dyn = d.dynamicDesc.states;
        std::sort(dyn.begin(), dyn.end());
        mix(static_cast<uint64_t>(dyn.size()));
        for (auto s : dyn) {
            mix(static_cast<uint64_t>(s));
        }

        return PipelineStateHash128{ ha, hb };
    }

    

    struct VulkanGraphicsPipelineKey
    {
        VkShaderModule m_vertShader{ VK_NULL_HANDLE };
        VkShaderModule m_fragShader{ VK_NULL_HANDLE };
        uint64_t m_setLayoutHash{ 0 };
        PipelineStateHash128 m_stateHash{};

        bool operator==(const VulkanGraphicsPipelineKey& _o) const noexcept
        {
            return m_vertShader == _o.m_vertShader
                && m_fragShader == _o.m_fragShader
                && m_setLayoutHash == _o.m_setLayoutHash
                && m_stateHash == _o.m_stateHash;
        }

        static VulkanGraphicsPipelineKey Make(const PipelineDesc& _desc, uint64_t _setLayoutHash)
        {
            VulkanGraphicsPipelineKey k;
            k.m_vertShader = _desc.vertexShader;
            k.m_fragShader = _desc.fragmentShader;
            k.m_setLayoutHash = _setLayoutHash;
            // Hashes all states from desc
            k.m_stateHash = HashPipelineDescState(_desc);
            return k;
        }
    };

    struct VulkanGraphicsPipelineKeyHash
    {
        size_t operator()(const VulkanGraphicsPipelineKey& _key) const noexcept
        {
            // Combines the two 64-bit hashes and key identity bits
            uint64_t h = 1469598103934665603ull;
            HashMix64(h, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(_key.m_vertShader)));
            HashMix64(h, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(_key.m_fragShader)));
            HashMix64(h, _key.m_setLayoutHash);
            HashMix64(h, _key.m_stateHash.a);
            HashMix64(h, _key.m_stateHash.b);
            return static_cast<size_t>(h);
        }
    };

    struct VulkanGraphicsPipelineCache;

    struct GraphicsPipelineCreateResult
    {
        VkPipeline m_pipeline{ VK_NULL_HANDLE };
        VkPipelineLayout m_layout{ VK_NULL_HANDLE };
    };

    struct VulkanGraphicsPipelineRef
    {
        VulkanGraphicsPipelineRef() = default;
        VulkanGraphicsPipelineRef(VulkanGraphicsPipelineCache* _cache, const VulkanGraphicsPipelineKey& _key, VkPipeline _pipeline, VkPipelineLayout _layout) noexcept : 
            m_cache(_cache), m_key(_key), m_pipeline(_pipeline), m_layout(_layout) 
        {}

        VulkanGraphicsPipelineRef(const VulkanGraphicsPipelineRef&) = delete;
        VulkanGraphicsPipelineRef& operator=(const VulkanGraphicsPipelineRef&) = delete;
        VulkanGraphicsPipelineRef(VulkanGraphicsPipelineRef&& rhs) noexcept { *this = std::move(rhs); }
        VulkanGraphicsPipelineRef& operator=(VulkanGraphicsPipelineRef&& rhs) noexcept;

        ~VulkanGraphicsPipelineRef();

        VkPipeline get() const noexcept { return m_pipeline; }
        VkPipelineLayout layout() const noexcept { return m_layout; }
        explicit operator bool() const noexcept { return m_pipeline != VK_NULL_HANDLE; }

    private:
        VulkanGraphicsPipelineCache* m_cache{ nullptr };
        VulkanGraphicsPipelineKey m_key{};
        VkPipeline m_pipeline{ VK_NULL_HANDLE };
        VkPipelineLayout m_layout{ VK_NULL_HANDLE };
    };

    struct VulkanGraphicsPipelineCache
    {
        using CreateFn = std::function<GraphicsPipelineCreateResult(const VulkanGraphicsPipelineKey&)>;

        explicit VulkanGraphicsPipelineCache(VkDevice device) : m_device(device) {}
        ~VulkanGraphicsPipelineCache() { destroyAll(); }

        VulkanGraphicsPipelineCache(const VulkanGraphicsPipelineCache&) = delete;
        VulkanGraphicsPipelineCache& operator=(const VulkanGraphicsPipelineCache&) = delete;

        // Acquire or create. Increments refcount and returns RAII wrapper.
        VulkanGraphicsPipelineRef acquire(const VulkanGraphicsPipelineKey& key, const CreateFn& creator);

        // Manual release
        void release(const VulkanGraphicsPipelineKey& key);

        // Destroy everything
        void destroyAll();

        // Destroy only entries that aren't referenced
        void purgeUnused();

        size_t size() const { std::lock_guard<std::mutex> lk(m_mutex); return m_map.size(); }

    private:
        struct Entry
        {
            VkPipeline m_pipeline{ VK_NULL_HANDLE };
            VkPipelineLayout m_layout{ VK_NULL_HANDLE };
            uint32_t m_refCount{ 0 };
        };

        VkDevice m_device{ VK_NULL_HANDLE };
        std::unordered_map<VulkanGraphicsPipelineKey, Entry, VulkanGraphicsPipelineKeyHash> m_map;
        mutable std::mutex m_mutex;

        friend struct VulkanGraphicsPipelineRef;
    };
} // namespace Mark::RendererVK