#include "Mark_DescriptorSetBundle.h"

#include "Utils/VulkanUtils.h"
#include "Utils/Mark_Utils.h"

#include <algorithm>

namespace Mark::RendererVK
{
    uint64_t VulkanDescriptorSetBundle::computeLayoutHash(const std::vector<VkDescriptorSetLayoutBinding>& _bindings,
        const std::vector<VkDescriptorBindingFlags>& _flags, VkDescriptorSetLayoutCreateFlags _layoutFlags)
    {
        // FNV-1a 64-bit
        uint64_t h = 1469598103934665603ull;
        auto mix = [&h](uint64_t v)
        {
            h ^= v;
            h *= 1099511628211ull;
        };

        mix(static_cast<uint64_t>(_layoutFlags));

        struct Entry { VkDescriptorSetLayoutBinding b; VkDescriptorBindingFlags f; };
        std::vector<Entry> temp;
        temp.reserve(_bindings.size());
        for (size_t i = 0; i < _bindings.size(); ++i)
        {
            VkDescriptorBindingFlags f = (i < _flags.size()) ? _flags[i] : 0;
            temp.push_back({ _bindings[i], f });
        }

        std::sort(temp.begin(), temp.end(),[](const Entry& a, const Entry& b) { 
            return a.b.binding < b.b.binding; 
        });

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

    void VulkanDescriptorSetBundle::createLayout(VkDevice _device,
        const std::vector<VkDescriptorSetLayoutBinding>& _bindings,
        const std::vector<VkDescriptorBindingFlags>& _bindingFlags,
        VkDescriptorSetLayoutCreateFlags _layoutFlags, const char* _debugName)
    {
        if (_device == VK_NULL_HANDLE) {
            MARK_FATAL(Utils::Category::Vulkan, "VulkanDescriptorSetBundle::createLayout called with null device");
        }

        // If already have a layout, destroy it first
        if (m_layout != VK_NULL_HANDLE) {
            destroyLayout(_device);
        }

        m_bindings = _bindings;
        m_bindingFlags = _bindingFlags;
        m_layoutFlags = _layoutFlags;
        m_debugName = (_debugName && _debugName[0]) ? _debugName : "UnnamedDescriptorSetBundle";

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = static_cast<uint32_t>(m_bindingFlags.size()),
            .pBindingFlags = m_bindingFlags.empty() ? nullptr : m_bindingFlags.data()
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = m_layoutFlags,
            .bindingCount = static_cast<uint32_t>(m_bindings.size()),
            .pBindings = m_bindings.data()
        };

        if (!m_bindingFlags.empty()) {
            layoutInfo.pNext = &flagsInfo;
        }
        else {
            layoutInfo.pNext = nullptr;
        }

        VkResult res = vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &m_layout);
        CHECK_VK_RESULT(res, "Create Descriptor Set Layout");

        m_layoutHash = computeLayoutHash(m_bindings, m_bindingFlags, m_layoutFlags);

        MARK_VK_NAME(_device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, m_layout, ("DescBundle." + m_debugName + ".SetLayout").c_str());
    }

    void VulkanDescriptorSetBundle::createPool(VkDevice _device, const std::vector<VkDescriptorPoolSize>& _poolSizes,
        uint32_t _maxSets, VkDescriptorPoolCreateFlags _poolFlags, const char* _debugName)
    {
        if (_device == VK_NULL_HANDLE) {
            MARK_FATAL(Utils::Category::Vulkan, "VulkanDescriptorSetBundle::createPool called with null device");
        }
        if (_maxSets == 0) {
            MARK_FATAL(Utils::Category::Vulkan, "VulkanDescriptorSetBundle::createPool maxSets==0");
        }

        // Recreate pool if needed
        if (m_pool != VK_NULL_HANDLE) {
            destroyPoolAndSets(_device);
        }
        // Pool name can differ from layout name if desired
        if (_debugName && _debugName[0]) {
            m_debugName = _debugName;
        }

        VkDescriptorPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = _poolFlags,
            .maxSets = _maxSets,
            .poolSizeCount = static_cast<uint32_t>(_poolSizes.size()),
            .pPoolSizes = _poolSizes.data()
        };

        VkResult res = vkCreateDescriptorPool(_device, &poolInfo, nullptr, &m_pool);
        CHECK_VK_RESULT(res, "Create Descriptor Pool");

        MARK_VK_NAME(_device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, m_pool, ("DescBundle." + m_debugName + ".Pool").c_str());
    }

    void VulkanDescriptorSetBundle::allocateSets(VkDevice _device, uint32_t _setCount,
        const std::vector<uint32_t>& _variableDescriptorCounts, const char* _debugSetPrefix)
    {
        if (_device == VK_NULL_HANDLE) {
            MARK_FATAL(Utils::Category::Vulkan, "VulkanDescriptorSetBundle::allocateSets called with null device");
        }
        if (m_layout == VK_NULL_HANDLE) {
            MARK_FATAL(Utils::Category::Vulkan, "VulkanDescriptorSetBundle::allocateSets called before createLayout");
        }
        if (m_pool == VK_NULL_HANDLE) {
            MARK_FATAL(Utils::Category::Vulkan, "VulkanDescriptorSetBundle::allocateSets called before createPool");
        }
        if (_setCount == 0) {
            MARK_FATAL(Utils::Category::Vulkan, "VulkanDescriptorSetBundle::allocateSets setCount==0");
        }
        if (!_variableDescriptorCounts.empty() && _variableDescriptorCounts.size() != _setCount) {
            MARK_FATAL(Utils::Category::Vulkan, "VulkanDescriptorSetBundle::allocateSets variableDescriptorCounts size mismatch");
        }

        m_sets.clear();
        m_sets.resize(_setCount);

        std::vector<VkDescriptorSetLayout> layouts(_setCount, m_layout);

        VkDescriptorSetVariableDescriptorCountAllocateInfo varInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .descriptorSetCount = _setCount,
            .pDescriptorCounts = _variableDescriptorCounts.empty() ? nullptr : _variableDescriptorCounts.data()
        };

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = _variableDescriptorCounts.empty() ? nullptr : &varInfo,
            .descriptorPool = m_pool,
            .descriptorSetCount = _setCount,
            .pSetLayouts = layouts.data()
        };

        VkResult res = vkAllocateDescriptorSets(_device, &allocInfo, m_sets.data());
        CHECK_VK_RESULT(res, "Allocate Descriptor Sets");

        const char* prefix = (_debugSetPrefix && _debugSetPrefix[0]) ? _debugSetPrefix : m_debugName.c_str();
        for (uint32_t i = 0; i < _setCount; ++i) {
            MARK_VK_NAME_F(_device, VK_OBJECT_TYPE_DESCRIPTOR_SET, m_sets[i], "DescBundle.%s.Set[%u]", prefix, i);
        }
    }

    void VulkanDescriptorSetBundle::allocateSetsVariableCount(VkDevice _device, uint32_t _setCount, uint32_t _variableCount, const char* _debugSetPrefix)
    {
        std::vector<uint32_t> counts(_setCount, _variableCount);
        allocateSets(_device, _setCount, counts, _debugSetPrefix);
    }

    void VulkanDescriptorSetBundle::allocateSets(VkDevice _device, uint32_t _setCount, const char* _debugSetPrefix)
    {
        std::vector<uint32_t> none;
        allocateSets(_device, _setCount, none, _debugSetPrefix);
    }

    void VulkanDescriptorSetBundle::destroyPoolAndSets(VkDevice _device)
    {
        if (m_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(_device, m_pool, nullptr);
            m_pool = VK_NULL_HANDLE;
        }
        m_sets.clear();
    }

    void VulkanDescriptorSetBundle::destroyLayout(VkDevice _device)
    {
        // Layout should not be destroyed while pipelines/layouts still reference it
        if (m_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(_device, m_layout, nullptr);
            m_layout = VK_NULL_HANDLE;
        }

        m_bindings.clear();
        m_bindingFlags.clear();
        m_layoutFlags = 0;
        m_layoutHash = 0;
    }

    void VulkanDescriptorSetBundle::destroy(VkDevice _device)
    {
        destroyPoolAndSets(_device);
        destroyLayout(_device);
        m_debugName.clear();
    }
} // namespace Mark::RendererVK