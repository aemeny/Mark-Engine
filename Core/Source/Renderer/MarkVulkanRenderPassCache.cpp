#include "MarkVulkanRenderPassCache.h"

namespace Mark::RendererVK
{
    // -------- RenderPassRef --------
    VulkanRenderPassRef& VulkanRenderPassRef::operator=(VulkanRenderPassRef&& _rhs) noexcept
    {
        if (this == &_rhs) return *this;
        // release current
        if (m_cache && m_renderPass != VK_NULL_HANDLE)
            m_cache->release(m_key);

        m_cache = _rhs.m_cache;
        m_key = _rhs.m_key;
        m_renderPass = _rhs.m_renderPass;

        _rhs.m_cache = nullptr;
        _rhs.m_renderPass = VK_NULL_HANDLE;
        return *this;
    }

    VulkanRenderPassRef::~VulkanRenderPassRef()
    {
        if (m_cache && m_renderPass != VK_NULL_HANDLE) 
            m_cache->release(m_key);
    }

    // -------- RenderPassCache --------
    VulkanRenderPassRef VulkanRenderPassCache::acquire(const VulkanRenderPassKey& _key, const CreateFn& _creator)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_map.find(_key);
        if (it != m_map.end()) 
        {
            it->second.refCount++;
            MARK_DEBUG("RenderPass-Cache reuse: refs=%u (entries=%zu)", it->second.refCount, m_map.size());
            return VulkanRenderPassRef(this, _key, it->second.renderPass);
        }

        // Create new via user-provided function
        VkRenderPass rp = _creator ? _creator(_key) : VK_NULL_HANDLE;
        if (rp == VK_NULL_HANDLE) 
        {
            MARK_LOG_ERROR("RenderPass-Cache: creator returned null render pass");
            return VulkanRenderPassRef{};
        }

        Entry e;
        e.renderPass = rp;
        e.refCount = 1;
        m_map.emplace(_key, e);

        MARK_INFO("RenderPass-Cache create: entries=%zu", m_map.size());
        return VulkanRenderPassRef(this, _key, rp);
    }

    void VulkanRenderPassCache::release(const VulkanRenderPassKey& _key)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_map.find(_key);
        if (it == m_map.end()) return;

        Entry& e = it->second;
        if (e.refCount > 0) 
            e.refCount--;
    }

    void VulkanRenderPassCache::destroyAll()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [k, e] : m_map) 
        {
            if (e.renderPass) 
            {
                vkDestroyRenderPass(m_device, e.renderPass, nullptr);
                e.renderPass = VK_NULL_HANDLE;
                e.refCount = 0;
            }
        }
        m_map.clear();
    }

    void VulkanRenderPassCache::purgeUnused()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_map.begin(); it != m_map.end(); ) 
        {
            if (it->second.refCount == 0) 
            {
                vkDestroyRenderPass(m_device, it->second.renderPass, nullptr);
                it = m_map.erase(it);
            }
            else 
            {
                ++it;
            }
        }
    }
}// namespace Mark::RendererVK