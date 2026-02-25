#pragma once
#include <Volk/volk.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Mark::RendererVK
{
    // Generic helper that owns:
    //  - VkDescriptorSetLayout
    //  - VkDescriptorPool
    //  - N VkDescriptorSet
    //
    // It does Nnot know anything about resources such as MeshHandler/Textures/UBOs
    // ResourceSet implementations should call vkUpdateDescriptorSets themselves
    struct VulkanDescriptorSetBundle
    {
        VulkanDescriptorSetBundle() = default;
        ~VulkanDescriptorSetBundle() = default;
        VulkanDescriptorSetBundle(const VulkanDescriptorSetBundle&) = delete;
        VulkanDescriptorSetBundle& operator=(const VulkanDescriptorSetBundle&) = delete;

        // If _bindingFlags is empty, no VkDescriptorSetLayoutBindingFlagsCreateInfo is chained
        void createLayout(VkDevice _device,
            const std::vector<VkDescriptorSetLayoutBinding>& _bindings,
            const std::vector<VkDescriptorBindingFlags>& _bindingFlags,
            VkDescriptorSetLayoutCreateFlags _layoutFlags, const char* _debugName
        );

        // Create descriptor pool - does not allocate any sets
        void createPool(VkDevice _device, const std::vector<VkDescriptorPoolSize>& _poolSizes,
            uint32_t _maxSets, VkDescriptorPoolCreateFlags _poolFlags, const char* _debugName
        );

        // Allocate N sets from the current pool using the current layout
        // If _variableDescriptorCounts is non-empty, variable descriptor count allocation is used
        // The vector size must equal _setCount
        void allocateSets(VkDevice _device, uint32_t _setCount,
            const std::vector<uint32_t>& _variableDescriptorCounts, const char* _debugSetPrefix
        );

        // Convenience overload: allocate N sets with the same variable descriptor count
        void allocateSetsVariableCount(VkDevice _device, uint32_t _setCount, uint32_t _variableCount, const char* _debugSetPrefix);
        // Convenience overload: allocate N sets without variable descriptor count
        void allocateSets(VkDevice _device, uint32_t _setCount, const char* _debugSetPrefix);

        // Destroys pool and sets, but keeps the layout
        void destroyPoolAndSets(VkDevice _device);
        // Destroys layout (and clears cached binding metadata)
        void destroyLayout(VkDevice _device);
        // Destroys everything
        void destroy(VkDevice _device);

        VkDescriptorSetLayout layout() const noexcept { return m_layout; }
        VkDescriptorPool pool() const noexcept { return m_pool; }

        VkDescriptorSet set(uint32_t _index) const noexcept { return (_index < m_sets.size()) ? m_sets[_index] : VK_NULL_HANDLE; }
        uint32_t setCount() const noexcept { return static_cast<uint32_t>(m_sets.size()); }

        uint64_t layoutHash() const noexcept { return m_layoutHash; }

        bool hasLayout() const noexcept { return m_layout != VK_NULL_HANDLE; }
        bool hasPool() const noexcept { return m_pool != VK_NULL_HANDLE; }
        bool hasSets() const noexcept { return !m_sets.empty(); }

        const std::vector<VkDescriptorSetLayoutBinding>& bindings() const noexcept { return m_bindings; }
        const std::vector<VkDescriptorBindingFlags>& bindingFlags() const noexcept { return m_bindingFlags; }
        VkDescriptorSetLayoutCreateFlags layoutFlags() const noexcept { return m_layoutFlags; }

    private:
        VkDescriptorSetLayout m_layout{ VK_NULL_HANDLE };
        VkDescriptorPool m_pool{ VK_NULL_HANDLE };
        std::vector<VkDescriptorSet> m_sets;

        std::vector<VkDescriptorSetLayoutBinding> m_bindings;
        std::vector<VkDescriptorBindingFlags> m_bindingFlags;
        VkDescriptorSetLayoutCreateFlags m_layoutFlags{ 0 };

        std::string m_debugName;
        uint64_t m_layoutHash{ 0 };

        static uint64_t computeLayoutHash(const std::vector<VkDescriptorSetLayoutBinding>& _bindings,
            const std::vector<VkDescriptorBindingFlags>& _flags, VkDescriptorSetLayoutCreateFlags _layoutFlags);
    };
} // namespace Mark::RendererVK