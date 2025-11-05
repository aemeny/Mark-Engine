#include "MarkVulkanGraphicsPipelineCache.h"
#include "Utils/MarkUtils.h"

namespace Mark::RendererVK
{
    // ---------- VulkanGraphicsPipelineRef ----------
    VulkanGraphicsPipelineRef& VulkanGraphicsPipelineRef::operator=(VulkanGraphicsPipelineRef&& _rhs) noexcept
    {
        if (this == &_rhs) return *this;
        // release current
        if (m_cache && m_pipeline)
            m_cache->release(m_key);

        m_cache = _rhs.m_cache;   
        m_key = _rhs.m_key;
        m_pipeline = _rhs.m_pipeline; 
        m_layout = _rhs.m_layout;  

        _rhs.m_cache = nullptr;
        _rhs.m_pipeline = VK_NULL_HANDLE;
        _rhs.m_layout = VK_NULL_HANDLE;
        return *this;
    }

    VulkanGraphicsPipelineRef::~VulkanGraphicsPipelineRef()
    {
        if (m_cache && m_pipeline)
            m_cache->release(m_key);
        m_cache = nullptr;
        m_pipeline = VK_NULL_HANDLE;
        m_layout = VK_NULL_HANDLE;
    }

    // ---------- VulkanGraphicsPipelineCache ----------
    VulkanGraphicsPipelineRef VulkanGraphicsPipelineCache::acquire(const VulkanGraphicsPipelineKey& _key, const CreateFn& _creator)
    {
        std::lock_guard<std::mutex> lk(m_mutex);

        auto it = m_map.find(_key);
        if (it != m_map.end())
        {
            it->second.m_refCount++;
            return VulkanGraphicsPipelineRef(this, _key, it->second.m_pipeline, it->second.m_layout);
        }

        GraphicsPipelineCreateResult cr = _creator(_key);
        if (!cr.m_pipeline || !cr.m_layout)
        {
            MARK_LOG_ERROR_C(Utils::Category::Vulkan, "Pipeline creator returned null handles");
            return VulkanGraphicsPipelineRef{};
        }

        Entry e;
        e.m_pipeline = cr.m_pipeline;
        e.m_layout = cr.m_layout;
        e.m_refCount = 1;
        auto [insIt, _] = m_map.emplace(_key, e);

        MARK_INFO_C(Utils::Category::Vulkan, "Graphics Pipeline Cached (entries: %zu)", m_map.size());
        return VulkanGraphicsPipelineRef(this, _key, insIt->second.m_pipeline, insIt->second.m_layout);
    }

    void VulkanGraphicsPipelineCache::release(const VulkanGraphicsPipelineKey& _key)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_map.find(_key);
        if (it == m_map.end()) return;

        if (it->second.m_refCount == 0)
        {
            MARK_WARN_C(Utils::Category::Vulkan, "Graphics pipeline cache: release on zero refCount");
            return;
        }

        it->second.m_refCount--;
    }

    void VulkanGraphicsPipelineCache::destroyAll()
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        for (auto& [k, e] : m_map)
        {
            if (e.m_pipeline)
                vkDestroyPipeline(m_device, e.m_pipeline, nullptr);
            if (e.m_layout)
                vkDestroyPipelineLayout(m_device, e.m_layout, nullptr);
        }
        m_map.clear();
    }

    void VulkanGraphicsPipelineCache::purgeUnused()
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        for (auto it = m_map.begin(); it != m_map.end(); )
        {
            if (it->second.m_refCount == 0)
            {
                if (it->second.m_pipeline)
                    vkDestroyPipeline(m_device, it->second.m_pipeline, nullptr);
                if (it->second.m_layout)
                    vkDestroyPipelineLayout(m_device, it->second.m_layout, nullptr);
                it = m_map.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
} // namespace Mark::RendererVK