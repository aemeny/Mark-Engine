#include "Mark_Skybox.h"
#include "Mark_VulkanCore.h"
#include "Mark_GraphicsPipeline.h"
#include "Mark_SwapChain.h"

#include "Utils/Mark_Utils.h"

#include <filesystem>

namespace Mark::RendererVK
{
    VulkanSkybox::VulkanSkybox(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanCommandBuffers* _commandBuffersRef) :
        m_vulkanCoreRef(_vulkanCoreRef), m_commandBuffersRef(_commandBuffersRef)
    {}

    void VulkanSkybox::initialize(const VulkanSwapChain& _swapChain, const char* _skyboxTexturePath)
    {
        m_numImages = _swapChain.numImages();

        m_uniformBuffer.createUniformBuffers(m_numImages);

        m_cubmapTexture.generateCubemapTexture(_skyboxTexturePath);

        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) {
            MARK_FATAL(Utils::Category::Vulkan, "VulkanCore expired in VulkanSkybox::initialize");
        }

        m_resourceSet.initialize(m_vulkanCoreRef, _swapChain, m_uniformBuffer, "SkyboxSet0");
        m_resourceSet.setCubemap(m_cubmapTexture);

        std::filesystem::path vertexPath = std::filesystem::path(MARK_CORE_ASSETS) / "SkyboxShader.vert";
        std::filesystem::path fragmentPath = std::filesystem::path(MARK_CORE_ASSETS) / "SkyboxShader.frag";
        m_vertexShader = m_vulkanCoreRef.lock()->shaderCache().getOrCreateFromGLSL(vertexPath.string().c_str());
        m_fragmentShader = m_vulkanCoreRef.lock()->shaderCache().getOrCreateFromGLSL(fragmentPath.string().c_str());

        const PipelineDesc pipelineDesc = {
            .m_device = VkCore->device(),
            .m_cache = VkCore->graphicsPipelineCache(),
            .m_vertexShader = m_vertexShader,
            .m_fragmentShader = m_fragmentShader,
            .m_colourFormat = _swapChain.surfaceFormat().format,
            .m_depthFormat = VkCore->physicalDevices().selected().m_depthFormat,
            .m_depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
        };

        m_pipelineRef = new VulkanGraphicsPipeline();
        m_pipelineRef->setResourceLayout(m_resourceSet.layout(), m_resourceSet.layoutHash());
        m_pipelineRef->createGraphicsPipeline(pipelineDesc);
    }

    void VulkanSkybox::destroy()
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) return;

        // Shader modules are owned by shaderCache so not destroyed here

        if (m_pipelineRef) {
            m_pipelineRef->destroyGraphicsPipeline();
            delete m_pipelineRef;
            m_pipelineRef = nullptr;
        }

        m_cubmapTexture.destroyTextureHandler(VkCore->device());

        m_resourceSet.destroy(VkCore->device());
        m_uniformBuffer.destroyUniformBuffers(VkCore->device());
    }

    void VulkanSkybox::update(int _imageIndex, const glm::mat4& _transformation)
    {
        m_uniformBuffer.updateUniformBuffer(_imageIndex, {_transformation });
    }

    void VulkanSkybox::recordCommandBuffer(VkCommandBuffer _cmdBuffer, int _imageIndex)
    {
        if (!m_pipelineRef) return;

        m_pipelineRef->bindPipeline(_cmdBuffer);

        m_resourceSet.bind(_cmdBuffer, m_pipelineRef->pipelineLayout(), _imageIndex);

        int numVertices = 36; // 6 faces * 2 triangles/face * 3 vertices/triangle
        int instance = 1;
        int baseVertex = 0;
        int firstInstance = 0;
        vkCmdDraw(_cmdBuffer, numVertices, instance, baseVertex, firstInstance);
    }

    void VulkanSkybox::recreateForSwapchain(const VulkanSwapChain& _swapChain)
    {
        auto VkCore = m_vulkanCoreRef.lock();
        if (!VkCore) {
            MARK_FATAL(Utils::Category::Vulkan, "VulkanCore expired in VulkanSkybox::recreateForSwapchain");
        }

        m_numImages = _swapChain.numImages();

        m_uniformBuffer.destroyUniformBuffers(VkCore->device());
        m_uniformBuffer.createUniformBuffers(m_numImages);

        m_resourceSet.recreateForSwapchain(_swapChain, m_uniformBuffer);

        if (m_pipelineRef) {
            const PipelineDesc pipelineDesc = {
                .m_device = VkCore->device(),
                .m_cache = VkCore->graphicsPipelineCache(),
                .m_vertexShader = m_vertexShader,
                .m_fragmentShader = m_fragmentShader,
                .m_colourFormat = _swapChain.surfaceFormat().format,
                .m_depthFormat = VkCore->physicalDevices().selected().m_depthFormat,
                .m_depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
            };
            m_pipelineRef->setResourceLayout(m_resourceSet.layout(), m_resourceSet.layoutHash());
            m_pipelineRef->createGraphicsPipeline(pipelineDesc);
        }
    }
} // namespace Mark::RendererVK