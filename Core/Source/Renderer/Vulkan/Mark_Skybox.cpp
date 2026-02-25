#include "Mark_Skybox.h"
#include "Mark_VulkanCore.h"

#include <filesystem>

namespace Mark::RendererVK
{
    VulkanSkybox::VulkanSkybox(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanCommandBuffers* _commandBuffersRef) :
        m_vulkanCoreRef(_vulkanCoreRef), m_commandBuffersRef(_commandBuffersRef)
    {}

    void VulkanSkybox::initialize(uint32_t _numImages, const char* _skyboxTexturePath)
    {
        m_numImages = _numImages;

        m_uniformBuffer.createUniformBuffers(m_numImages);

        m_cubmapTexture.generateCubemapTexture(_skyboxTexturePath);

        std::filesystem::path vertexPath = std::filesystem::path(MARK_ASSETS_DIR) / "SkyboxShader.vert";
        std::filesystem::path fragmentPath = std::filesystem::path(MARK_ASSETS_DIR) / "SkyboxShader.frag";
        m_vertexShader = m_vulkanCoreRef.lock()->shaderCache().getOrCreateFromGLSL(vertexPath.string().c_str());
        m_fragmentShader = m_vulkanCoreRef.lock()->shaderCache().getOrCreateFromGLSL(fragmentPath.string().c_str());
    }
} // namespace Mark::RendererVK