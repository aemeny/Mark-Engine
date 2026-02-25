#pragma once
#include "Mark_DescriptorSetBundle.h"

#include <Volk/volk.h>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanSwapChain;
    struct VulkanUniformBuffer;
    struct MeshHandler;
    struct TextureHandler;
    struct BindlessCaps;

    //  binding 0: vertices SSBO array
    //  binding 1: indices SSBO array
    //  binding 2: global/per-image UBO
    //  binding 3: bindless textures (has variable descriptor count)
    namespace BindlessBinding
    {
        constexpr uint32_t verticesSSBO = 0;
        constexpr uint32_t indicesSSBO = 1;
        constexpr uint32_t UBO = 2;
        constexpr uint32_t texture = 3;
    }

    struct VulkanBindlessMeshResourceSet
    {
        VulkanBindlessMeshResourceSet() = default;
        ~VulkanBindlessMeshResourceSet() = default;
        VulkanBindlessMeshResourceSet(const VulkanBindlessMeshResourceSet&) = delete;
        VulkanBindlessMeshResourceSet& operator=(const VulkanBindlessMeshResourceSet&) = delete;

        // Set within engine
        struct Settings
        {
            // Keeps the texture array to a sensible max for the engine
            const uint32_t maxTextures = 8192u;

            // Initial capacity for command buffers to bind (Avoids immediate full rebuild)
            const uint32_t initialTextureCapacity = 64u;

            // Number of possible textures that the engine can handle per mesh
            const uint32_t numAttachableTextures = 1u;
        };

        void initialize(std::weak_ptr<VulkanCore> _core, const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo,
            const std::vector<std::shared_ptr<MeshHandler>>* _meshes, // Optional (can be null)
            const char* _debugName
        );

        void destroy(VkDevice _device);

        // Recreate pool + sets when swapchain image count changes
        void recreateForSwapchain(const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo,
            const std::vector<std::shared_ptr<MeshHandler>>* _meshes // Optional (can be null)
        );

        // Full rewrite of all descriptors (UBO + mesh slots + textures)
        void rebuildAll(const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo,
            const std::vector<std::shared_ptr<MeshHandler>>& _meshes
        );

        // Updates a single mesh slot across all swapchain-image sets
        // Returns false if capacity exceeded and a full recreate is required
        bool tryWriteMeshSlot(const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo, uint32_t _meshIndex, const MeshHandler& _mesh);

        // Bind set 0 for the given swapchain image index.
        void bind(VkCommandBuffer _cmd, VkPipelineLayout _layout, uint32_t _imageIndex) const;

        VkDescriptorSetLayout layout() const noexcept { return m_set.layout(); }
        uint64_t layoutHash() const noexcept { return m_set.layoutHash(); }

        // Info
        uint32_t maxMeshesLayout() const noexcept { return m_maxMeshesLayout; }
        uint32_t maxTexturesLayout() const noexcept { return m_maxTexturesLayout; }
        uint32_t textureCapacity()  const noexcept { return m_textureDescriptorCount; }
        uint32_t meshCountUsed()    const noexcept { return m_meshCountUsed; }

        bool valid() const noexcept { return m_set.hasLayout() && m_set.hasSets(); }

    private:
        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VkDevice m_device{ VK_NULL_HANDLE };
        std::string m_debugName;
        const Settings m_settings;

        VulkanDescriptorSetBundle m_set;

        // Layout config / capacity
        uint32_t m_maxMeshesLayout{ 0 };        // DescriptorCount for bindings 0/1
        uint32_t m_maxTexturesLayout{ 0 };      // Layout maximum for binding 3
        uint32_t m_textureDescriptorCount{ 1 }; // Allocated variable descriptor count capacity for binding 3
        uint32_t m_meshCountUsed{ 0 };          // Used mesh count (clamped to maxMeshesLayout)

        void configureFromCaps(const BindlessCaps& _caps, uint32_t _meshCountHint);
        void ensureLayoutCreated();
        void recreatePoolAndSets(uint32_t _numImages);

        void updateAllDescriptors(uint32_t _numImages,
            const VulkanUniformBuffer& _ubo,
            const std::vector<std::shared_ptr<MeshHandler>>* _meshes);

        void updateMeshSlotDescriptors(uint32_t _numImages,uint32_t _meshIndex, const MeshHandler& _mesh, const VulkanUniformBuffer& _ubo);

        static uint32_t growPow2Capacity(uint32_t _required, uint32_t _maxCap, uint32_t _initialCap);
    };
} // namespace Mark::RendererVK