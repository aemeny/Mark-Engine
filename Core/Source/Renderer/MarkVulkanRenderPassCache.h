#pragma once
#include "Utils/MarkUtils.h"

#include <volk.h>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <utility>
#include <optional>

namespace Mark::RendererVK
{
    // Handles what makes two render passes "compatible" for a single-subpass path
    struct VulkanRenderPassKey
    {
        VkFormat m_colorFormat{ VK_FORMAT_UNDEFINED };
        VkFormat m_depthFormat{ VK_FORMAT_UNDEFINED }; // VK_FORMAT_UNDEFINED = no depth
        VkSampleCountFlagBits m_samples{ VK_SAMPLE_COUNT_1_BIT };

        VkAttachmentLoadOp m_colorLoad{ VK_ATTACHMENT_LOAD_OP_CLEAR };
        VkAttachmentStoreOp m_colorStore{ VK_ATTACHMENT_STORE_OP_STORE };
        VkAttachmentLoadOp m_depthLoad{ VK_ATTACHMENT_LOAD_OP_CLEAR };
        VkAttachmentStoreOp m_depthStore{ VK_ATTACHMENT_STORE_OP_DONT_CARE };

        bool operator==(const VulkanRenderPassKey& _o) const noexcept
        {
            return m_colorFormat == _o.m_colorFormat
                && m_depthFormat == _o.m_depthFormat
                && m_samples == _o.m_samples
                && m_colorLoad == _o.m_colorLoad
                && m_colorStore == _o.m_colorStore
                && m_depthLoad == _o.m_depthLoad
                && m_depthStore == _o.m_depthStore;
        }

        static VulkanRenderPassKey Make(VkFormat _colorFmt,
            VkFormat _depthFmt,
            VkSampleCountFlagBits _smp = VK_SAMPLE_COUNT_1_BIT,
            VkAttachmentLoadOp _cLoad = VK_ATTACHMENT_LOAD_OP_CLEAR,
            VkAttachmentStoreOp _cStore = VK_ATTACHMENT_STORE_OP_STORE,
            VkAttachmentLoadOp _dLoad = VK_ATTACHMENT_LOAD_OP_CLEAR,
            VkAttachmentStoreOp _dStore = VK_ATTACHMENT_STORE_OP_DONT_CARE)
        {
            VulkanRenderPassKey k;
            k.m_colorFormat = _colorFmt;
            k.m_depthFormat = _depthFmt;
            k.m_samples = _smp;
            k.m_colorLoad = _cLoad;
            k.m_colorStore = _cStore;
            k.m_depthLoad = _dLoad;
            k.m_depthStore = _dStore;
            return k;
        }
    };

    struct VulkanRenderPassKeyHash
    {
        size_t operator()(const VulkanRenderPassKey& _key) const noexcept
        {
            // Simple FNV-1a style mixer
            size_t h = 1469598103934665603ull;
            auto mix = [&](uint64_t _v) { h ^= _v; h *= 1099511628211ull; };
            mix((uint64_t)_key.m_colorFormat);
            mix((uint64_t)_key.m_depthFormat);
            mix((uint64_t)_key.m_samples);
            mix((uint64_t)_key.m_colorLoad);
            mix((uint64_t)_key.m_colorStore);
            mix((uint64_t)_key.m_depthLoad);
            mix((uint64_t)_key.m_depthStore);
            return h;
        }
    };

    struct VulkanRenderPassCache;

    // RAII reference that automatically decrements the cache’s refcount
    struct VulkanRenderPassRef
    {
        VulkanRenderPassRef() = default;
        VulkanRenderPassRef(VulkanRenderPassCache* _cache, const VulkanRenderPassKey& _key, VkRenderPass _rp) noexcept : 
            m_cache(_cache), m_key(_key), m_renderPass(_rp)
        {}

        VulkanRenderPassRef(const VulkanRenderPassRef&) = delete;
        VulkanRenderPassRef& operator=(const VulkanRenderPassRef&) = delete;
        VulkanRenderPassRef(VulkanRenderPassRef&& rhs) noexcept { *this = std::move(rhs); }
        VulkanRenderPassRef& operator=(VulkanRenderPassRef&& _rhs) noexcept;

        ~VulkanRenderPassRef();

        VkRenderPass get() const noexcept { return m_renderPass; }
        explicit operator bool() const noexcept { return m_renderPass != VK_NULL_HANDLE; }

    private:
        VulkanRenderPassCache* m_cache{ nullptr };
        VulkanRenderPassKey m_key{};
        VkRenderPass m_renderPass{ VK_NULL_HANDLE };
    };

    struct VulkanRenderPassCache
    {
        using CreateFn = std::function<VkRenderPass(const VulkanRenderPassKey&)>;

        explicit VulkanRenderPassCache(VkDevice _device) : m_device(_device) {}
        ~VulkanRenderPassCache() { destroyAll(); }

        VulkanRenderPassCache(const VulkanRenderPassCache&) = delete;
        VulkanRenderPassCache& operator=(const VulkanRenderPassCache&) = delete;

        // Acquire or create. Increments refcount, returns RAII wrapper
        VulkanRenderPassRef acquire(const VulkanRenderPassKey& _key, const CreateFn& _creator);

        // Manual release
        void release(const VulkanRenderPassKey& _key);

        // Destroy every render pass held by the cache
        void destroyAll();

        // Destroy only entries that aren’t referenced
        void purgeUnused();

        // Debug aid
        size_t size() const { std::lock_guard<std::mutex> lock(m_mutex); return m_map.size(); }

    private:
        struct Entry 
        {
            VkRenderPass m_renderPass{ VK_NULL_HANDLE };
            uint32_t m_refCount{ 0 };
        };

        VkDevice m_device{ VK_NULL_HANDLE };
        std::unordered_map<VulkanRenderPassKey, Entry, VulkanRenderPassKeyHash> m_map;
        mutable std::mutex m_mutex;

        friend class VulkanRenderPassRef;
    };
} // namespace Mark::RendererVK