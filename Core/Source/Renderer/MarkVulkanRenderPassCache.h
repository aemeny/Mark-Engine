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
    // Handles what makes two render passes "compatible" for Marks single-subpass path
    struct VulkanRenderPassKey
    {
        VkFormat colorFormat{ VK_FORMAT_UNDEFINED };
        VkFormat depthFormat{ VK_FORMAT_UNDEFINED }; // VK_FORMAT_UNDEFINED = no depth
        VkSampleCountFlagBits samples{ VK_SAMPLE_COUNT_1_BIT };

        VkAttachmentLoadOp colorLoad{ VK_ATTACHMENT_LOAD_OP_CLEAR };
        VkAttachmentStoreOp colorStore{ VK_ATTACHMENT_STORE_OP_STORE };
        VkAttachmentLoadOp depthLoad{ VK_ATTACHMENT_LOAD_OP_CLEAR };
        VkAttachmentStoreOp depthStore{ VK_ATTACHMENT_STORE_OP_DONT_CARE };

        bool operator==(const VulkanRenderPassKey& _o) const noexcept
        {
            return colorFormat == _o.colorFormat
                && depthFormat == _o.depthFormat
                && samples == _o.samples
                && colorLoad == _o.colorLoad
                && colorStore == _o.colorStore
                && depthLoad == _o.depthLoad
                && depthStore == _o.depthStore;
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
            k.colorFormat = _colorFmt;
            k.depthFormat = _depthFmt;
            k.samples = _smp;
            k.colorLoad = _cLoad;
            k.colorStore = _cStore;
            k.depthLoad = _dLoad;
            k.depthStore = _dStore;
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
            mix((uint64_t)_key.colorFormat);
            mix((uint64_t)_key.depthFormat);
            mix((uint64_t)_key.samples);
            mix((uint64_t)_key.colorLoad);
            mix((uint64_t)_key.colorStore);
            mix((uint64_t)_key.depthLoad);
            mix((uint64_t)_key.depthStore);
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

    // Cache of VkRenderPass per VkDevice (you own VkDevice lifetime)
    // You provide a creator lambda that builds the VkRenderPass for a given key
    struct VulkanRenderPassCache
    {
        using CreateFn = std::function<VkRenderPass(const VulkanRenderPassKey&)>;

        explicit VulkanRenderPassCache(VkDevice _device) : m_device(_device) {}
        ~VulkanRenderPassCache() { destroyAll(); }

        VulkanRenderPassCache(const VulkanRenderPassCache&) = delete;
        VulkanRenderPassCache& operator=(const VulkanRenderPassCache&) = delete;

        // Acquire or create. Increments refcount, returns RAII wrapper
        VulkanRenderPassRef acquire(const VulkanRenderPassKey& _key, const CreateFn& _creator);

        // Manual release if you didn’t keep the RAII wrapper (not recommended)
        void release(const VulkanRenderPassKey& _key);

        // Destroy every render pass held by the cache (call before destroying device)
        void destroyAll();

        // Destroy only entries that aren’t referenced
        void purgeUnused();

        // Debug aid
        size_t size() const { std::lock_guard<std::mutex> lock(m_mutex); return m_map.size(); }

    private:
        struct Entry 
        {
            VkRenderPass renderPass{ VK_NULL_HANDLE };
            uint32_t refCount{ 0 };
        };

        VkDevice m_device{ VK_NULL_HANDLE };
        std::unordered_map<VulkanRenderPassKey, Entry, VulkanRenderPassKeyHash> m_map;
        mutable std::mutex m_mutex;

        friend class VulkanRenderPassRef;
    };
} // namespace Mark::RendererVK