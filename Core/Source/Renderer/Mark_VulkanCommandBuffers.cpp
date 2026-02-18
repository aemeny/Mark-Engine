#include "Mark_VulkanCommandBuffers.h"
#include "Mark_VulkanCore.h"
#include "Utils/VulkanUtils.h"
#include "Utils/Mark_Utils.h"
#include <array>

namespace Mark::RendererVK
{
    VulkanCommandBuffers::VulkanCommandBuffers(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanSwapChain& _swapChainRef, VulkanGraphicsPipeline& _graphicsPipelineRef) :
        m_vulkanCoreRef(_vulkanCoreRef), m_swapChainRef(_swapChainRef), m_graphicsPipelineRef(_graphicsPipelineRef)
    {}

    void VulkanCommandBuffers::destroyCommandBuffers()
    {
        if (m_vulkanCoreRef.expired())
        {
            MARK_LOG_ERROR_C(Utils::Category::Vulkan, "VulkanCore reference expired, cannot destroy command buffers");
        }
        auto VkCore = m_vulkanCoreRef.lock();

        if (m_commandPool != VK_NULL_HANDLE)
        {
            if (!m_commandBuffers.withGUI.empty()) {
                freeCommandBuffers(m_commandBuffers.withGUI.size(), m_commandBuffers.withGUI.data());
                m_commandBuffers.withGUI.clear();
            }
            if (!m_commandBuffers.withoutGUI.empty()) {
                freeCommandBuffers(m_commandBuffers.withoutGUI.size(), m_commandBuffers.withoutGUI.data());
                m_commandBuffers.withoutGUI.clear();
            }

            freeCommandBuffers(1, &m_copyCommandBuffer);
 
            vkDestroyCommandPool(VkCore->device(), m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }

        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Command Buffers & Pool Destroyed");
    }

    void VulkanCommandBuffers::freeCommandBuffers(size_t _bufferSize, const VkCommandBuffer* _bufferData)
    {
        vkFreeCommandBuffers(m_vulkanCoreRef.lock()->device(),
            m_commandPool,
            static_cast<uint32_t>(_bufferSize),
            _bufferData
        );
    }

    void VulkanCommandBuffers::createCommandPool()
    {
        VkCommandPoolCreateInfo cmdPoolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_vulkanCoreRef.lock()->graphicsQueueFamilyIndex()
        };
        VkResult res = vkCreateCommandPool(m_vulkanCoreRef.lock()->device(), &cmdPoolCreateInfo, nullptr, &m_commandPool);
        CHECK_VK_RESULT(res, "Create command pool");

        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Command Pool Created");
    }

    void VulkanCommandBuffers::createCommandBuffers(uint32_t _numImages, std::vector<VkCommandBuffer>& _commandBuffers)
    {
        _commandBuffers.resize(_numImages);
        VkCommandBufferAllocateInfo cmdBufferAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<uint32_t>(_commandBuffers.size())
        };
        VkResult res = vkAllocateCommandBuffers(m_vulkanCoreRef.lock()->device(), &cmdBufferAllocInfo, _commandBuffers.data());
        CHECK_VK_RESULT(res, "Allocate command buffers");
        MARK_DEBUG_C(Utils::Category::Vulkan, "Vulkan Command Buffers Allocated: %zu", _commandBuffers.size());
    }

    void VulkanCommandBuffers::createCopyCommandBuffer()
    {
        VkCommandBufferAllocateInfo cmdBufferAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        VkResult res = vkAllocateCommandBuffers(m_vulkanCoreRef.lock()->device(), &cmdBufferAllocInfo, &m_copyCommandBuffer);
        CHECK_VK_RESULT(res, "Allocate copy command buffers");
        MARK_DEBUG_C(Utils::Category::Vulkan, "Vulkan Copy Command Buffer Allocated");
    }

    void VulkanCommandBuffers::recordCommandBuffers(VkClearColorValue _clearColour)
    {
        recordCommanBuffersInternal(_clearColour, m_commandBuffers.withoutGUI, true);

        recordCommanBuffersInternal(_clearColour, m_commandBuffers.withGUI, false);
    }

    void VulkanCommandBuffers::recordCommanBuffersInternal(VkClearColorValue _clearColour, std::vector<VkCommandBuffer> _commandBuffers, bool _withSecondBarrier)
    {
        // Record each command buffer
        for (uint32_t i = 0; i < _commandBuffers.size(); i++)
        {
            VkCommandBuffer commandBuffer = _commandBuffers[i];

            beginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

            VkClearValue clearColourValue = { .color = _clearColour };
            VkClearValue pDepthClearValue = { .depthStencil = { 1.0f, 0 } };
            beginDynamicRendering(commandBuffer, i, &clearColourValue, &pDepthClearValue);

            setViewportAndScissor(commandBuffer, m_swapChainRef.extent());

            const uint32_t instanceCount = 1;
            const uint32_t firstVertex = 0;
            const uint32_t firstInstance = 0;

            struct PushConstants
            {
                uint32_t meshIndex;
                uint32_t textureIndex;
            };
            
            // Bind pipeline + descriptor set once per swapchain image
            m_graphicsPipelineRef.bindPipeline(commandBuffer, i);

            for (uint32_t m = 0; m < m_graphicsPipelineRef.meshCount(); m++)
            {
                const uint32_t indexCountForMesh = m_graphicsPipelineRef.indexCountForMesh(m);
                const uint32_t vertexCount = indexCountForMesh > 0
                    ? indexCountForMesh   // SSBO index path
                    : m_graphicsPipelineRef.vertexCountForMesh(m); // fallback if no indices

                if (vertexCount == 0) continue; // Skip empty meshes

                // 1 texture per mesh for now: textureIndex == meshIndex
                PushConstants pushConstant{ m, m };
                vkCmdPushConstants(commandBuffer,
                    m_graphicsPipelineRef.pipelineLayout(),
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0, sizeof(PushConstants), 
                    &pushConstant
                );

                vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
            }

            endDynamicRendering(commandBuffer, i, _withSecondBarrier);
        }

        MARK_INFO_C(Utils::Category::Vulkan, "Vulkan Command Buffers Recorded");
    }

    void VulkanCommandBuffers::beginCommandBuffer(VkCommandBuffer _cmdBuffer, VkCommandBufferUsageFlags _usageFlags)
    {
        VkCommandBufferBeginInfo cmdBufferBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = _usageFlags,
            .pInheritanceInfo = nullptr
        };

        VkResult res = vkBeginCommandBuffer(_cmdBuffer, &cmdBufferBeginInfo);
        CHECK_VK_RESULT(res, "Begin command buffer recording");
    }

    void VulkanCommandBuffers::beginDynamicRendering(VkCommandBuffer _cmdBuffer, uint32_t _imageIndex, VkClearValue* _clearColour, VkClearValue* _depthValue, bool _transitionFromPresent)
    {
        if (_transitionFromPresent) {
            // Colour image: PRESENT_SRC_KHR -> COLOR_ATTACHMENT_OPTIMAL
            VkImageMemoryBarrier toColourAttachment = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = m_swapChainRef.swapChainImageAt(_imageIndex),
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };

            vkCmdPipelineBarrier(
                _cmdBuffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // src
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dst
                0,
                0, nullptr,
                0, nullptr,
                1, &toColourAttachment
            );
        }

        // Dynamic rendering attachments
        VkRenderingAttachmentInfo colourAttachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = m_swapChainRef.swapChainImageViewAt(_imageIndex),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = _clearColour ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE
        };
        if (_clearColour) {
            colourAttachment.clearValue = *_clearColour;
        }

        VkRenderingAttachmentInfo depthAttachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = m_swapChainRef.depthImageAt(_imageIndex).imageView(),
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = _depthValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE
        };
        if (_depthValue) {
            depthAttachment.clearValue = *_depthValue;
        }

        VkRenderingInfo renderingInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderArea = { { 0, 0 }, m_swapChainRef.extent() },
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colourAttachment,
            .pDepthAttachment = &depthAttachment,
            .pStencilAttachment = nullptr
        };

        vkCmdBeginRendering(_cmdBuffer, &renderingInfo);
    }

    void VulkanCommandBuffers::endDynamicRendering(VkCommandBuffer _cmdBuffer, uint32_t _imageIndex, bool _withSecondBarrier)
    {
        vkCmdEndRendering(_cmdBuffer);

        if (_withSecondBarrier) {
            // Colour image: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
            VkImageMemoryBarrier toPresent = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = 0,
                .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = m_swapChainRef.swapChainImageAt(_imageIndex),
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };

            vkCmdPipelineBarrier(
                _cmdBuffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &toPresent
            );
        }

        VkResult res = vkEndCommandBuffer(_cmdBuffer);
        CHECK_VK_RESULT(res, "End command buffer recording");
    }

    void VulkanCommandBuffers::setViewportAndScissor(VkCommandBuffer _cmdBuffer, const VkExtent2D& _extent)
    {
        VkViewport viewport{ 0.f, 0.f, static_cast<float>(_extent.width), static_cast<float>(_extent.height), 0.f, 1.f };
        VkRect2D scissor{ {0,0}, _extent };
        vkCmdSetViewport(_cmdBuffer, 0, 1, &viewport);
        vkCmdSetScissor(_cmdBuffer, 0, 1, &scissor);
    }
} // namespace Mark::RendererVK