#include "Mark_BindlessMeshResourceSet.h"

#include "Mark_VulkanCore.h"
#include "Mark_SwapChain.h"
#include "Mark_UniformBuffer.h"
#include "Mark_ModelHandler.h" 

#include "Utils/VulkanUtils.h"
#include "Utils/Mark_Utils.h"

#include <algorithm>

namespace Mark::RendererVK
{
    static inline uint32_t safeMin(uint32_t _a, uint32_t _b) { return (_a < _b) ? _a : _b; }
    static inline uint32_t safeMax(uint32_t _a, uint32_t _b) { return (_a > _b) ? _a : _b; }

    uint32_t VulkanBindlessMeshResourceSet::growPow2Capacity(uint32_t _r, uint32_t _maxCap, uint32_t _initialCap)
    {
        _r = safeMax(1u, _r);
        _maxCap = safeMax(1u, _maxCap);

        uint32_t cap = safeMax(1u, _initialCap);
        cap = safeMin(cap, _maxCap);
        while (cap < _r && cap < _maxCap) cap <<= 1u;
        if (cap < _r) cap = _r;
        cap = safeMin(cap, _maxCap);
        return safeMax(1u, cap);
    }

    void VulkanBindlessMeshResourceSet::initialize(std::weak_ptr<VulkanCore> _coreRef, const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo,
        const std::vector<std::shared_ptr<MeshHandler>>* _meshes, const char* _debugName)
    {
        m_vulkanCoreRef = _coreRef;
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) MARK_FATAL(Utils::Category::Vulkan, "VulkanBindlessMeshResourceSet::initialize - VulkanCore expired");

        m_device = VkCore->device();
        m_debugName = (_debugName && _debugName[0]) ? _debugName : "UnamedBindlessMesh";

        const uint32_t numImages = (uint32_t)_swapchain.numImages();
        const uint32_t meshHint = _meshes ? (uint32_t)_meshes->size() : 0u;

        configureFromCaps(VkCore->bindlessCaps(), meshHint);
        ensureLayoutCreated();
        recreatePoolAndSets(numImages);
        updateAllDescriptors(numImages, _ubo, _meshes);
    }

    void VulkanBindlessMeshResourceSet::destroy(VkDevice _device)
    {
        m_set.destroy(_device);
        m_device = VK_NULL_HANDLE;
        m_debugName.clear();
        m_maxMeshesLayout = 0;
        m_maxTexturesLayout = 0;
        m_textureDescriptorCount = 1;
        m_meshCountUsed = 0;
    }

    void VulkanBindlessMeshResourceSet::recreateForSwapchain(const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo,
        const std::vector<std::shared_ptr<MeshHandler>>* _meshes)
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) MARK_FATAL(Utils::Category::Vulkan, "VulkanBindlessMeshResourceSet::recreateForSwapchain - core expired");

        const uint32_t numImages = (uint32_t)_swapchain.numImages();
        const uint32_t meshHint = _meshes ? (uint32_t)_meshes->size() : 0u;

        configureFromCaps(VkCore->bindlessCaps(), meshHint);
        ensureLayoutCreated();

        m_set.destroyPoolAndSets(m_device);
        recreatePoolAndSets(numImages);
        updateAllDescriptors(numImages, _ubo, _meshes);
    }

    void VulkanBindlessMeshResourceSet::rebuildAll(const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo,
        const std::vector<std::shared_ptr<MeshHandler>>& _meshes)
    {
        updateAllDescriptors((uint32_t)_swapchain.numImages(), _ubo, &_meshes);
    }

    bool VulkanBindlessMeshResourceSet::tryWriteMeshSlot(const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo, uint32_t _meshIndex, const MeshHandler& _mesh)
    {
        const uint32_t numImages = (uint32_t)_swapchain.numImages();

        if (_meshIndex >= m_maxMeshesLayout) {
            return false;
        }

        const uint32_t texPerMesh = safeMax(1u, m_settings.numAttachableTextures);
        const uint32_t firstTexIdx = _meshIndex * texPerMesh;

        if (firstTexIdx >= m_textureDescriptorCount) {
            return false;
        }

        if (_meshIndex + 1u > m_meshCountUsed) {
            m_meshCountUsed = _meshIndex + 1u;
        }

        updateMeshSlotDescriptors(numImages, _meshIndex, _mesh, _ubo);

        return true;
    }

    void VulkanBindlessMeshResourceSet::bind(VkCommandBuffer _cmd, VkPipelineLayout _layout, uint32_t _imageIndex) const
    {
        VkDescriptorSet set = m_set.set(_imageIndex);
        if (set == VK_NULL_HANDLE) {
            MARK_FATAL(Utils::Category::Vulkan, "BindlessMeshResourceSet bind: imageIndex out of range");
        }

        vkCmdBindDescriptorSets(_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _layout, 0, 1, &set, 0, nullptr);
    }

    void VulkanBindlessMeshResourceSet::configureFromCaps(const BindlessCaps& _caps, uint32_t _meshCountHint)
    {
        m_maxTexturesLayout = safeMin(m_settings.numAttachableTextures, _caps.maxTextureDescriptors);
        if (m_maxTexturesLayout == 0) {
            m_maxTexturesLayout = 1;
        }
        m_maxMeshesLayout = _caps.maxMeshes;
        if (m_maxMeshesLayout == 0) {
            m_maxMeshesLayout = 1;
        }

        const uint32_t texPerMesh = safeMax(1u, m_settings.numAttachableTextures);
        const uint32_t maxMeshesByTex = (m_maxTexturesLayout / texPerMesh);
        if (maxMeshesByTex == 0) {
            MARK_FATAL(Utils::Category::Vulkan, "BindlessMeshResourceSet: maxTexturesLayout too small for texturesPerMesh");
        }

        if (m_maxMeshesLayout > maxMeshesByTex) {
            m_maxMeshesLayout = maxMeshesByTex;
        }

        m_meshCountUsed = safeMin(_meshCountHint, m_maxMeshesLayout);

        uint32_t requiredTextures = 1;
        if (_meshCountHint > 0) {
            requiredTextures = safeMin(_meshCountHint * texPerMesh, m_maxTexturesLayout);
        }

        const uint32_t initCap = safeMin(m_settings.initialTextureCapacity, m_maxTexturesLayout);
        m_textureDescriptorCount = growPow2Capacity(requiredTextures, m_maxTexturesLayout, initCap);
    }

    void VulkanBindlessMeshResourceSet::ensureLayoutCreated()
    {
        if (m_set.hasLayout()) return;

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        std::vector<VkDescriptorBindingFlags> flags;
        bindings.reserve(4); flags.reserve(4);

        bindings.push_back({ BindlessBinding::verticesSSBO, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_maxMeshesLayout, VK_SHADER_STAGE_VERTEX_BIT, nullptr });
        flags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);

        bindings.push_back({ BindlessBinding::indicesSSBO, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_maxMeshesLayout, VK_SHADER_STAGE_VERTEX_BIT, nullptr });
        flags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);

        bindings.push_back({ BindlessBinding::UBO, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr });
        flags.push_back(0);

        bindings.push_back({ BindlessBinding::texture, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_maxTexturesLayout, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });
        flags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);

        m_set.createLayout(m_device, bindings, flags, 0, ("BindlessMesh." + m_debugName + ".Set0").c_str());
    }

    void VulkanBindlessMeshResourceSet::recreatePoolAndSets(uint32_t _numImages)
    {
        std::vector<VkDescriptorPoolSize> sizes;
        sizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _numImages * m_maxMeshesLayout * 2u });
        sizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  _numImages });
        sizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _numImages * m_textureDescriptorCount });

        m_set.createPool(m_device, sizes, _numImages, 0, ("BindlessMesh." + m_debugName).c_str());
        m_set.allocateSetsVariableCount(m_device, _numImages, m_textureDescriptorCount, ("BindlessMesh." + m_debugName).c_str());
    }

    void VulkanBindlessMeshResourceSet::updateAllDescriptors(uint32_t _numImages, const VulkanUniformBuffer& _ubo,
        const std::vector<std::shared_ptr<MeshHandler>>* _meshes)
    {
        std::vector<VkDescriptorBufferInfo> uboInfos(_numImages);
        for (uint32_t img = 0; img < _numImages; img++) {
            uboInfos[img] = _ubo.descriptorInfo(img);
        }

        if (!_meshes || _meshes->empty())
        {
            std::vector<VkWriteDescriptorSet> writes;
            writes.reserve(_numImages);
            for (uint32_t img = 0; img < _numImages; img++)
            {
                VkDescriptorSet set = m_set.set(img);
                writes.push_back({ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, BindlessBinding::UBO, 0, 1,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &uboInfos[img], nullptr });
            }
            vkUpdateDescriptorSets(m_device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
            return;
        }

        m_meshCountUsed = safeMin((uint32_t)_meshes->size(), m_maxMeshesLayout);

        std::vector<VkDescriptorBufferInfo> vbInfos(m_meshCountUsed);
        std::vector<VkDescriptorBufferInfo> ibInfos(m_meshCountUsed);
        std::vector<uint8_t> hasMesh(m_meshCountUsed, 0);

        const uint32_t texPerMesh = safeMax(1u, m_settings.numAttachableTextures);
        const uint32_t texturesUsed = safeMin(m_meshCountUsed * texPerMesh, m_textureDescriptorCount);

        std::vector<VkDescriptorImageInfo> imgInfos(texturesUsed);
        std::vector<uint8_t> hasTex(texturesUsed, 0);

        for (uint32_t m = 0; m < m_meshCountUsed; m++)
        {
            const auto mesh = _meshes->at(m);
            if (!mesh) continue;

            if (mesh->hasVertexBuffer() && mesh->hasIndexBuffer())
            {
                vbInfos[m] = { mesh->vertexBuffer(), 0, VK_WHOLE_SIZE };
                ibInfos[m] = { mesh->indexBuffer(), 0, VK_WHOLE_SIZE };
                hasMesh[m] = 1;
            }

            const uint32_t texIndex = m * texPerMesh;
            if (texIndex < texturesUsed)
            {
                TextureHandler* t = mesh->texture();
                if (t)
                {
                    VkDescriptorImageInfo info{ t->sampler(), t->imageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
                    imgInfos[texIndex] = info;
                    hasTex[texIndex] = 1;
                }
            }
        }

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(_numImages * (1u + m_meshCountUsed * 2u + texturesUsed));

        for (uint32_t img = 0; img < _numImages; img++)
        {
            VkDescriptorSet set = m_set.set(img);

            writes.push_back({ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, BindlessBinding::UBO, 0, 1,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &uboInfos[img], nullptr });

            for (uint32_t m = 0; m < m_meshCountUsed; m++)
            {
                if (!hasMesh[m]) continue;

                writes.push_back({ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, BindlessBinding::verticesSSBO, m, 1,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &vbInfos[m], nullptr });

                writes.push_back({ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, BindlessBinding::indicesSSBO, m, 1,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &ibInfos[m], nullptr });
            }

            for (uint32_t t = 0; t < texturesUsed; t++)
            {
                if (!hasTex[t]) continue;

                VkWriteDescriptorSet writeSet = { 
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set,
                    .dstBinding = BindlessBinding::texture,
                    .dstArrayElement = t,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imgInfos[t]
                };

                writes.push_back(writeSet);
            }
        }

        vkUpdateDescriptorSets(m_device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
    }

    void VulkanBindlessMeshResourceSet::updateMeshSlotDescriptors(uint32_t _numImages, uint32_t _meshIndex,
        const MeshHandler& _mesh, const VulkanUniformBuffer& _ubo)
    {
        if (!_mesh.hasVertexBuffer() || !_mesh.hasIndexBuffer())
            return;

        VkDescriptorBufferInfo vb{ _mesh.vertexBuffer(), 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo ib{ _mesh.indexBuffer(), 0, VK_WHOLE_SIZE };

        std::vector<VkDescriptorBufferInfo> uboInfos(_numImages);
        for (uint32_t img = 0; img < _numImages; img++) {
            uboInfos[img] = _ubo.descriptorInfo(img);
        }

        const uint32_t texPerMesh = safeMax(1u, m_settings.numAttachableTextures);
        const uint32_t texIndex = _meshIndex * texPerMesh;

        TextureHandler* t = _mesh.texture();
        VkDescriptorImageInfo imgInfo{};
        bool writeTex = false;

        if (t)
        {
            imgInfo = { t->sampler(), t->imageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            writeTex = true;
        }

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(_numImages * (3u + (writeTex ? 1u : 0u)));

        for (uint32_t img = 0; img < _numImages; img++)
        {
            VkDescriptorSet set = m_set.set(img);

            writes.push_back({ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, BindlessBinding::UBO, 0, 1,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &uboInfos[img], nullptr });

            writes.push_back({ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, BindlessBinding::verticesSSBO, _meshIndex, 1,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &vb, nullptr });

            writes.push_back({ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, BindlessBinding::indicesSSBO, _meshIndex, 1,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &ib, nullptr });

            if (writeTex)
            {
                VkWriteDescriptorSet writeSet = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set,
                    .dstBinding = BindlessBinding::texture,
                    .dstArrayElement = texIndex,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imgInfo
                };

                writes.push_back(writeSet);
            }
        }

        vkUpdateDescriptorSets(m_device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
    }
} // namespace Mark::RendererVK