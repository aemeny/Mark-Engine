#include "Mark_SkyboxResourceSet.h"

#include "Mark_VulkanCore.h"
#include "Mark_SwapChain.h"
#include "Mark_UniformBuffer.h"
#include "Mark_TextureHandler.h"

#include "Utils/VulkanUtils.h"
#include "Utils/Mark_Utils.h"

namespace Mark::RendererVK
{
    void VulkanSkyboxResourceSet::initialize(std::weak_ptr<VulkanCore> _vulkanCoreRef, const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo, const char* _debugName)
    {
        m_vulkanCoreRef = _vulkanCoreRef;
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore)
            MARK_FATAL(Utils::Category::Vulkan, "VulkanSkyboxResourceSet::initialize - VulkanCore expired");

        m_device = VkCore->device();
        m_debugName = (_debugName && _debugName[0]) ? _debugName : "SkyboxSet";

        const uint32_t numImages = (uint32_t)_swapchain.numImages();

        // Layout
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(2);

        bindings.push_back(VkDescriptorSetLayoutBinding{
            .binding = SkyboxBinding::UBO,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        });

        bindings.push_back(VkDescriptorSetLayoutBinding{
            .binding = SkyboxBinding::Cubemap,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        });

        std::vector<VkDescriptorBindingFlags> noFlags;
        m_set.createLayout(m_device, bindings, noFlags, 0, ("Skybox." + m_debugName + ".Set0").c_str());

        // Pool
        std::vector<VkDescriptorPoolSize> sizes;
        sizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numImages });
        sizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numImages });

        m_set.createPool(m_device, sizes, numImages, 0, ("Skybox." + m_debugName).c_str());
        m_set.allocateSets(m_device, numImages, ("Skybox." + m_debugName).c_str());

        // Writes
        writeUBODescriptors(numImages, _ubo);
        if (m_hasCube) {
            writeCubemapDescriptors(numImages);
        }

        MARK_INFO(Utils::Category::Vulkan, "SkyboxResourceSet initialized (%s). Sets=%u", m_debugName.c_str(), numImages);
    }

    void VulkanSkyboxResourceSet::destroy(VkDevice _device)
    {
        m_set.destroy(_device);
        m_device = VK_NULL_HANDLE;
        m_debugName.clear();
        m_hasCube = false;
        m_cubeInfo = {};
    }

    void VulkanSkyboxResourceSet::recreateForSwapchain(const VulkanSwapChain& _swapchain, const VulkanUniformBuffer& _ubo)
    {
        if (m_device == VK_NULL_HANDLE) {
            MARK_FATAL(Utils::Category::Vulkan, "SkyboxResourceSet recreate called with null device");
        }

        const uint32_t numImages = (uint32_t)_swapchain.numImages();

        m_set.destroyPoolAndSets(m_device);

        std::vector<VkDescriptorPoolSize> sizes;
        sizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numImages });
        sizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numImages });

        m_set.createPool(m_device, sizes, numImages, 0, ("Skybox." + m_debugName).c_str());
        m_set.allocateSets(m_device, numImages, ("Skybox." + m_debugName).c_str());

        writeUBODescriptors(numImages, _ubo);
        if (m_hasCube) {
            writeCubemapDescriptors(numImages);
        }
    }

    void VulkanSkyboxResourceSet::setCubemap(TextureHandler& _cubemap)
    {
        m_cubeInfo = VkDescriptorImageInfo{
            .sampler = _cubemap.sampler(),
            .imageView = _cubemap.imageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        m_hasCube = true;

        if (m_device == VK_NULL_HANDLE || !m_set.hasSets()) {
            return; // initialize() not called yet
        }

        writeCubemapDescriptors(m_set.setCount());
    }

    void VulkanSkyboxResourceSet::writeUBODescriptors(uint32_t _numImages, const VulkanUniformBuffer& _ubo)
    {
        std::vector<VkDescriptorBufferInfo> uboInfos(_numImages);
        for (uint32_t i = 0; i < _numImages; i++) {
            uboInfos[i] = _ubo.descriptorInfo(i);
        }

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(_numImages);

        for (uint32_t i = 0; i < _numImages; i++)
        {
            VkDescriptorSet set = m_set.set(i);
            writes.push_back(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set,
                .dstBinding = SkyboxBinding::UBO,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pImageInfo = nullptr,
                .pBufferInfo = &uboInfos[i],
                .pTexelBufferView = nullptr
            });
        }

        vkUpdateDescriptorSets(m_device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
    }

    void VulkanSkyboxResourceSet::writeCubemapDescriptors(uint32_t _numImages)
    {
        if (!m_hasCube) {
            return;
        }

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(_numImages);

        for (uint32_t i = 0; i < _numImages; i++)
        {
            VkDescriptorSet set = m_set.set(i);

            VkWriteDescriptorSet writeSet = { 
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET ,
                .dstSet = set,
                .dstBinding = SkyboxBinding::Cubemap,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &m_cubeInfo
            };

            writes.push_back(writeSet);
        }

        vkUpdateDescriptorSets(m_device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
    }

    void VulkanSkyboxResourceSet::bind(VkCommandBuffer _cmd, VkPipelineLayout _layout, uint32_t _imageIndex) const
    {
        VkDescriptorSet set = m_set.set(_imageIndex);
        if (set == VK_NULL_HANDLE) {
            MARK_FATAL(Utils::Category::Vulkan, "SkyboxResourceSet bind: imageIndex out of range");
        }

        vkCmdBindDescriptorSets(_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _layout, 0, 1, &set, 0, nullptr);
    }
} // namespace Mark::RendererVK