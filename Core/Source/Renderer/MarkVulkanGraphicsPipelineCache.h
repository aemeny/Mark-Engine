#pragma once
#include <volk.h>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <cstdint>

namespace Mark::RendererVK
{
    // Pipeline identity:
    // - Dynamic rendering
    // - Program: shader modules (VS/FS)  (entry names assumed "main")
    // - Fixed state: topology, samples

    struct VulkanGraphicsPipelineKey
    {
        VkFormat m_colourFormat{ VK_FORMAT_UNDEFINED };
        VkFormat m_depthFormat{ VK_FORMAT_UNDEFINED };

        VkShaderModule m_vertShader{ VK_NULL_HANDLE };
        VkShaderModule m_fragShader{ VK_NULL_HANDLE };
        VkPrimitiveTopology m_topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
        VkSampleCountFlagBits m_samples{ VK_SAMPLE_COUNT_1_BIT };
        uint32_t m_dynamicStateMask{ 0 };
        uint64_t m_descriptorSetLayoutHash{ 0 };

        bool operator==(const VulkanGraphicsPipelineKey& _o) const noexcept
        {
            return m_colourFormat == _o.m_colourFormat
                && m_depthFormat == _o.m_depthFormat
                && m_vertShader == _o.m_vertShader
                && m_fragShader == _o.m_fragShader
                && m_topology == _o.m_topology
                && m_samples == _o.m_samples
                && m_dynamicStateMask == _o.m_dynamicStateMask
                && m_descriptorSetLayoutHash == _o.m_descriptorSetLayoutHash;
        }

        static VulkanGraphicsPipelineKey Make(
            VkFormat _color,
            VkFormat _depth,
            VkShaderModule _vert,
            VkShaderModule _frag,
            VkPrimitiveTopology _topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VkSampleCountFlagBits _smp = VK_SAMPLE_COUNT_1_BIT,
            uint32_t _dynMask = 0,
            uint64_t _descSetHash = 0)
        {
            VulkanGraphicsPipelineKey k;
            k.m_colourFormat = _color;
            k.m_depthFormat = _depth;
            k.m_vertShader = _vert;
            k.m_fragShader = _frag;
            k.m_topology = _topo;
            k.m_samples = _smp;
            k.m_dynamicStateMask = _dynMask;
            k.m_descriptorSetLayoutHash = _descSetHash;
            return k;
        }
    };

    struct VulkanGraphicsPipelineKeyHash
    {
        size_t operator()(const VulkanGraphicsPipelineKey& _key) const noexcept
        {
            // Simple FNV-1a style mixer
            size_t hash = 1469598103934665603ull;
            auto mix = [&hash](size_t _v)
            {
                hash ^= _v;
                hash *= 1099511628211ull;
            };
            mix(static_cast<size_t>(_key.m_colourFormat));
            mix(static_cast<size_t>(_key.m_depthFormat));
            mix(reinterpret_cast<size_t>(_key.m_vertShader));
            mix(reinterpret_cast<size_t>(_key.m_fragShader));
            mix(static_cast<size_t>(_key.m_topology));
            mix(static_cast<size_t>(_key.m_samples));
            mix(static_cast<size_t>(_key.m_dynamicStateMask));
            mix(static_cast<size_t>(_key.m_descriptorSetLayoutHash));
            return hash;
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