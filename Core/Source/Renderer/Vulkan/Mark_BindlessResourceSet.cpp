#include "Mark_BindlessResourceSet.h"

#include "Mark_VulkanCore.h"
#include "Mark_SwapChain.h"
#include "Mark_UniformBuffer.h"
#include "Mark_ModelHandler.h" // MeshHandler + TextureHandler

#include "Utils/VulkanUtils.h"
#include "Utils/Mark_Utils.h"

#include <algorithm>
#include <cstring>

namespace Mark::RendererVK
{
    static inline uint32_t safeMin(uint32_t _a, uint32_t _b) { return (_a < _b) ? _a : _b; }
    static inline uint32_t safeMax(uint32_t _a, uint32_t _b) { return (_a > _b) ? _a : _b; }

    uint32_t VulkanBindlessResourceSet::growPow2Capacity(uint32_t _required, uint32_t _maxCap, uint32_t _initialCap)
    {
        _required = safeMax(1u, _required);
        _maxCap = safeMax(1u, _maxCap);

        uint32_t cap = safeMax(1u, _initialCap);
        cap = safeMin(cap, _maxCap);

        while (cap < _required && cap < _maxCap)
            cap <<= 1u;

        if (cap < _required)
            cap = _required;

        cap = safeMin(cap, _maxCap);
        return safeMax(1u, cap);
    }

    uint64_t VulkanBindlessResourceSet::hashBindings(const std::vector<VkDescriptorSetLayoutBinding>& _bindings,
        const std::vector<VkDescriptorBindingFlags>& _flags,
        VkDescriptorSetLayoutCreateFlags _layoutFlags)
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

        std::sort(temp.begin(), temp.end(),
            [](const Entry& a, const Entry& b) { return a.b.binding < b.b.binding; });

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

    void VulkanBindlessResourceSet::initialize(std::weak_ptr<VulkanCore> _core, const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo,
        const std::vector<std::shared_ptr<MeshHandler>>* _meshes,
        const char* _debugName)
    {
        m_vulkanCoreRef = _core;
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) {
            MARK_FATAL(Utils::Category::Vulkan, "VulkanBindlessResourceSet::initialize - VulkanCore expired");
        }

        m_device = VkCore->device();
        m_debugName = (_debugName && _debugName[0]) ? _debugName : "BindlessSet.Unnamed";

        const uint32_t numImages = static_cast<uint32_t>(_swapchain.numImages());

        // Determine mesh count from provided meshes
        const uint32_t meshCount = (_meshes) ? static_cast<uint32_t>(_meshes->size()) : 0u;
        configureFromCaps(VkCore->bindlessCaps(), meshCount);

        // Layout is created once and reused across pool recreations
        createDescriptorSetLayout();

        createDescriptorPool(numImages);
        allocateDescriptorSets(numImages);

        // Write UBO + any mesh slots provided 
        updateAllDescriptors(numImages, _ubo, _meshes);

        MARK_INFO(Utils::Category::Vulkan, "BindlessResourceSet initialized (%s). Sets=%u, maxMeshes=%u, texCap=%u, texLayoutMax=%u",
            m_debugName.c_str(), numImages, m_maxMeshesLayout, m_textureDescriptorCount, m_maxTexturesLayout);
    }

    void VulkanBindlessResourceSet::destroy(VkDevice _device)
    {
        if (m_descriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(_device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }

        m_descriptorSets.clear();

        if (m_descriptorSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(_device, m_descriptorSetLayout, nullptr);
            m_descriptorSetLayout = VK_NULL_HANDLE;
        }

        m_bindings.clear();
        m_bindingFlags.clear();
        m_descSetLayoutHash = 0;
        m_layoutCreateFlags = 0;
    }

    void VulkanBindlessResourceSet::recreateForSwapchain(const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo,
        const std::vector<std::shared_ptr<MeshHandler>>* _meshes)
    {
        if (m_device == VK_NULL_HANDLE) {
            MARK_FATAL(Utils::Category::Vulkan, "BindlessResourceSet recreateForSwapchain with null device");
        }

        const uint32_t numImages = static_cast<uint32_t>(_swapchain.numImages());

        if (m_descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }
        m_descriptorSets.clear();

        // Recompute counts / capacity based on current mesh count
        const uint32_t meshCount = (_meshes) ? static_cast<uint32_t>(_meshes->size()) : 0u;
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) MARK_FATAL(Utils::Category::Vulkan, "BindlessResourceSet recreateForSwapchain core expired");

        configureFromCaps(VkCore->bindlessCaps(), meshCount);

        createDescriptorPool(numImages);
        allocateDescriptorSets(numImages);
        updateAllDescriptors(numImages, _ubo, _meshes);
    }

    void VulkanBindlessResourceSet::configureFromCaps(const BindlessCaps& _caps, uint32_t _meshCount)
    {
        // Layout maximums
        m_maxTexturesLayout = safeMin(m_settings.maxTextures, _caps.maxTextureDescriptors);
        if (m_maxTexturesLayout == 0) m_maxTexturesLayout = 1;

        // Mesh layout max: clamp by texture availability
        m_maxMeshesLayout = _caps.maxMeshes / safeMax(1u, m_settings.numAttachableTextures);
        if (m_maxMeshesLayout == 0) m_maxMeshesLayout = 1;

        const uint32_t texPerMesh = safeMax(1u, m_settings.numAttachableTextures);
        const uint32_t maxMeshesByTex = (m_maxTexturesLayout / texPerMesh);
        if (maxMeshesByTex == 0) {
            MARK_FATAL(Utils::Category::Vulkan, "BindlessResourceSet: maxTexturesLayout too small for texturesPerMesh");
        }

        if (m_maxMeshesLayout > maxMeshesByTex) {
            MARK_WARN(Utils::Category::Vulkan,
                "BindlessResourceSet: Clamping maxMeshesLayout (%u) to textures constraint (%u) with texturesPerMesh=%u",
                m_maxMeshesLayout, maxMeshesByTex, texPerMesh);
            m_maxMeshesLayout = maxMeshesByTex;
        }

        // Used mesh count
        m_meshCount = safeMin(_meshCount, m_maxMeshesLayout);

        // Variable descriptor count capacity for textures
        // Current mapping: textureIndex == meshIndex (or meshIndex * texturesPerMesh + slot)
        uint32_t requiredTextures = 1;
        if (_meshCount > 0) {
            requiredTextures = safeMin(_meshCount * texPerMesh, m_maxTexturesLayout);
        }

        // If there are 0 meshes, will still allocate some capacity so command buffers can bind sets
        const uint32_t initialCap = safeMin(m_settings.initialTextureCapacity, m_maxTexturesLayout);
        m_textureDescriptorCount = growPow2Capacity(requiredTextures, m_maxTexturesLayout, initialCap);
    }

    void VulkanBindlessResourceSet::createDescriptorSetLayout()
    {
        if (m_descriptorSetLayout != VK_NULL_HANDLE)
            return;

        m_bindings.clear();
        m_bindingFlags.clear();

        // binding 0: vertices SSBO array
        {
            VkDescriptorSetLayoutBinding binding = {
                .binding = BindlessBinding::verticesSSBO,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = m_maxMeshesLayout,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .pImmutableSamplers = nullptr
            };
            m_bindings.push_back(binding);
            m_bindingFlags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
        }

        // binding 1: indices SSBO array
        {
            VkDescriptorSetLayoutBinding binding = {
                .binding = BindlessBinding::indicesSSBO,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = m_maxMeshesLayout,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .pImmutableSamplers = nullptr
            };
            m_bindings.push_back(binding);
            m_bindingFlags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
        }

        // binding 2: UBO (per swapchain image)
        {
            VkDescriptorSetLayoutBinding binding = {
                .binding = BindlessBinding::UBO,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .pImmutableSamplers = nullptr
            };
            m_bindings.push_back(binding);
            m_bindingFlags.push_back(0);
        }

        // binding 3: bindless textures
        {
            VkDescriptorSetLayoutBinding binding = {
                .binding = BindlessBinding::texture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = m_maxTexturesLayout, // layout maximum
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr
            };
            m_bindings.push_back(binding);
            m_bindingFlags.push_back(
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
            );
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = static_cast<uint32_t>(m_bindingFlags.size()),
            .pBindingFlags = m_bindingFlags.data()
        };

        m_layoutCreateFlags = 0; // No update-after-bind

        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = &flagsInfo,
            .flags = m_layoutCreateFlags,
            .bindingCount = static_cast<uint32_t>(m_bindings.size()),
            .pBindings = m_bindings.data()
        };

        VkResult res = vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout);
        CHECK_VK_RESULT(res, "Create Bindless DescriptorSetLayout");

        m_descSetLayoutHash = hashBindings(m_bindings, m_bindingFlags, m_layoutCreateFlags);

        MARK_VK_NAME(m_device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, m_descriptorSetLayout,
            ("BindlessSet." + m_debugName + ".SetLayout0").c_str());
    }

    void VulkanBindlessResourceSet::createDescriptorPool(uint32_t _numImages)
    {
        //  - binding 0: maxMeshesLayout storage buffers
        //  - binding 1: maxMeshesLayout storage buffers
        //  - binding 2: 1 UBO
        //  - binding 3: textureDescriptorCount combined image samplers (Variable allocated count)
        std::vector<VkDescriptorPoolSize> sizes;
        sizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _numImages * m_maxMeshesLayout * 2u });
        sizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  _numImages });
        sizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _numImages * m_textureDescriptorCount });

        VkDescriptorPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = 0,
            .maxSets = _numImages,
            .poolSizeCount = static_cast<uint32_t>(sizes.size()),
            .pPoolSizes = sizes.data()
        };

        VkResult res = vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool);
        CHECK_VK_RESULT(res, "Create Bindless DescriptorPool");

        MARK_VK_NAME(m_device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, m_descriptorPool,
            ("BindlessSet." + m_debugName + ".DescPool").c_str());
    }

    void VulkanBindlessResourceSet::allocateDescriptorSets(uint32_t _numImages)
    {
        std::vector<VkDescriptorSetLayout> layouts(_numImages, m_descriptorSetLayout);
        std::vector<uint32_t> variableCounts(_numImages, m_textureDescriptorCount);

        VkDescriptorSetVariableDescriptorCountAllocateInfo varInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorSetCount = _numImages,
            .pDescriptorCounts = variableCounts.data()
        };

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = &varInfo,
            .descriptorPool = m_descriptorPool,
            .descriptorSetCount = _numImages,
            .pSetLayouts = layouts.data()
        };

        m_descriptorSets.resize(_numImages);

        VkResult res = vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.data());
        CHECK_VK_RESULT(res, "Allocate Bindless DescriptorSets");

        for (uint32_t i = 0; i < _numImages; i++) {
            MARK_VK_NAME_F(m_device, VK_OBJECT_TYPE_DESCRIPTOR_SET, m_descriptorSets[i],
                "BindlessSet.%s.Set0[%u]", m_debugName.c_str(), i);
        }
    }

    void VulkanBindlessResourceSet::updateAllDescriptors(uint32_t _numImages, const VulkanUniformBuffer& _ubo,
        const std::vector<std::shared_ptr<MeshHandler>>* _meshes)
    {
        std::vector<VkDescriptorBufferInfo> uboInfos(_numImages);
        for (uint32_t img = 0; img < _numImages; img++) {
            uboInfos[img] = _ubo.descriptorInfo(img);
        }

        // If meshes are absent writes UBO and return
        if (!_meshes || _meshes->empty())
        {
            std::vector<VkWriteDescriptorSet> writes;
            writes.reserve(_numImages);

            for (uint32_t img = 0; img < _numImages; img++)
            {
                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = m_descriptorSets[img],
                    .dstBinding = BindlessBinding::UBO,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &uboInfos[img],
                    .pTexelBufferView = nullptr
                });
            }

            vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
            return;
        }

        // Clamp used mesh count
        m_meshCount = safeMin(static_cast<uint32_t>(_meshes->size()), m_maxMeshesLayout);

        // Gather per-mesh infos
        std::vector<VkDescriptorBufferInfo> vbInfos(m_meshCount);
        std::vector<VkDescriptorBufferInfo> ibInfos(m_meshCount);
        std::vector<uint8_t> hasMeshBuffers(m_meshCount, 0);

        // Texture descriptors are only for capacity range allocated
        const uint32_t texPerMesh = safeMax(1u, m_settings.numAttachableTextures);
        const uint32_t texturesUsed = safeMin(m_meshCount * texPerMesh, m_textureDescriptorCount);

        std::vector<VkDescriptorImageInfo> imageInfos(texturesUsed);
        std::vector<uint8_t> hasTexture(texturesUsed, 0);

        for (uint32_t m = 0; m < m_meshCount; m++)
        {
            const auto& mesh = _meshes->at(m);
            if (!mesh) continue;

            if (mesh->hasVertexBuffer() && mesh->hasIndexBuffer())
            {
                vbInfos[m] = { mesh->vertexBuffer(), 0, VK_WHOLE_SIZE };
                ibInfos[m] = { mesh->indexBuffer(), 0, VK_WHOLE_SIZE };
                hasMeshBuffers[m] = 1;
            }

            const uint32_t texIndex = m * texPerMesh;
            if (texIndex < texturesUsed)
            {
                TextureHandler* tex = mesh->texture();
                if (tex)
                {
                    VkDescriptorImageInfo info{
                        .sampler = tex->sampler(),
                        .imageView = tex->imageView(),
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    };
                    imageInfos[texIndex] = info;
                    hasTexture[texIndex] = 1;
                }
            }
        }

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(_numImages * (1u + m_meshCount * 2u + texturesUsed));

        for (uint32_t img = 0; img < _numImages; img++)
        {
            VkDescriptorSet set = m_descriptorSets[img];

            // UBO once per image
            writes.push_back(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set,
                .dstBinding = BindlessBinding::UBO,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pImageInfo = nullptr,
                .pBufferInfo = &uboInfos[img],
                .pTexelBufferView = nullptr
            });

            for (uint32_t m = 0; m < m_meshCount; m++)
            {
                if (!hasMeshBuffers[m])
                    continue;

                // Vertices
                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set,
                    .dstBinding = BindlessBinding::verticesSSBO,
                    .dstArrayElement = m,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &vbInfos[m],
                    .pTexelBufferView = nullptr
                });

                // Indices
                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set,
                    .dstBinding = BindlessBinding::indicesSSBO,
                    .dstArrayElement = m,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &ibInfos[m],
                    .pTexelBufferView = nullptr
                });
            }

            // Texture slots
            for (uint32_t t = 0; t < texturesUsed; t++)
            {
                if (!hasTexture[t]) continue;

                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set,
                    .dstBinding = BindlessBinding::texture,
                    .dstArrayElement = t,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imageInfos[t],
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr
                });
            }
        }

        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    void VulkanBindlessResourceSet::rebuildAll(const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo,
        const std::vector<std::shared_ptr<MeshHandler>>& _meshes)
    {
        const uint32_t numImages = static_cast<uint32_t>(_swapchain.numImages());
        updateAllDescriptors(numImages, _ubo, &_meshes);
    }

    bool VulkanBindlessResourceSet::tryWriteMeshSlot(const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo, uint32_t _meshIndex,
        const MeshHandler& _mesh)
    {
        const uint32_t numImages = static_cast<uint32_t>(_swapchain.numImages());

        if (_meshIndex >= m_maxMeshesLayout)
            return false;

        const uint32_t texPerMesh = safeMax(1u, m_settings.numAttachableTextures);
        const uint32_t firstTexIndex = _meshIndex * texPerMesh;

        if (firstTexIndex >= m_textureDescriptorCount)
            return false;

        // Used mesh count expands
        if (_meshIndex + 1u > m_meshCount)
            m_meshCount = _meshIndex + 1u;

        updateMeshSlotDescriptors(numImages, _meshIndex, _mesh, _ubo);
        return true;
    }

    void VulkanBindlessResourceSet::updateMeshSlotDescriptors(uint32_t _numImages, uint32_t _meshIndex, const MeshHandler& _mesh, const VulkanUniformBuffer& _ubo)
    {
        if (!_mesh.hasVertexBuffer() || !_mesh.hasIndexBuffer()) {
            MARK_WARN(Utils::Category::Vulkan,
                "BindlessResourceSet: mesh %u missing buffers (VB=%s IB=%s) -> leaving slot unwritten.",
                _meshIndex, _mesh.hasVertexBuffer() ? "yes" : "no", _mesh.hasIndexBuffer() ? "yes" : "no");
            return;
        }

        VkDescriptorBufferInfo vbInfo = { _mesh.vertexBuffer(), 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo ibInfo = { _mesh.indexBuffer(), 0, VK_WHOLE_SIZE };

        std::vector<VkDescriptorBufferInfo> uboInfos(_numImages);
        for (uint32_t img = 0; img < _numImages; img++) {
            uboInfos[img] = _ubo.descriptorInfo(img);
        }

        const uint32_t texPerMesh = safeMax(1u, m_settings.numAttachableTextures);
        const uint32_t texIndex = _meshIndex * texPerMesh;

        TextureHandler* tex = _mesh.texture();
        VkDescriptorImageInfo imgInfo{};
        bool writeTexture = false;

        if (tex)
        {
            imgInfo = {
                .sampler = tex->sampler(),
                .imageView = tex->imageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writeTexture = true;
        }

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(_numImages * (3u + (writeTexture ? 1u : 0u)));

        for (uint32_t img = 0; img < _numImages; img++)
        {
            VkDescriptorSet set = m_descriptorSets[img];

            // Keep UBO consistent
            writes.push_back(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set,
                .dstBinding = BindlessBinding::UBO,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pImageInfo = nullptr,
                .pBufferInfo = &uboInfos[img],
                .pTexelBufferView = nullptr
            });

            // VB slot
            writes.push_back(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set,
                .dstBinding = BindlessBinding::verticesSSBO,
                .dstArrayElement = _meshIndex,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pImageInfo = nullptr,
                .pBufferInfo = &vbInfo,
                .pTexelBufferView = nullptr
            });

            // IB slot
            writes.push_back(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set,
                .dstBinding = BindlessBinding::indicesSSBO,
                .dstArrayElement = _meshIndex,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pImageInfo = nullptr,
                .pBufferInfo = &ibInfo,
                .pTexelBufferView = nullptr
            });

            if (writeTexture)
            {
                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set,
                    .dstBinding = BindlessBinding::texture,
                    .dstArrayElement = texIndex,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imgInfo,
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr
                });
            }
        }

        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    void VulkanBindlessResourceSet::bind(VkCommandBuffer _cmd, VkPipelineLayout _layout, uint32_t _imageIndex) const
    {
        if (_imageIndex >= m_descriptorSets.size()) {
            MARK_FATAL(Utils::Category::Vulkan, "BindlessResourceSet bind: imageIndex out of range");
        }

        vkCmdBindDescriptorSets(
            _cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            _layout,
            0,  // firstSet
            1,  // descriptorSetCount
            &m_descriptorSets[_imageIndex],
            0,
            nullptr
        );
    }
} // namespace Mark::RendererVK