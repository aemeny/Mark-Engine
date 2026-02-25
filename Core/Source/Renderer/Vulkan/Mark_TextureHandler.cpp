#include "Mark_TextureHandler.h"
#include "Mark_VulkanCore.h"
#include "Mark_BufferAndMemoryHelper.h"
#include "Mark_CommandBuffers.h"

#include "Utils/Mark_Utils.h"
#include "Utils/VulkanUtils.h"
#include <vulkan/vk_enum_string_helper.h>

#include <glm/gtc/constants.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include "Include/stb/stb_image.h"

namespace Mark::RendererVK
{
    TextureHandler::TextureHandler(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanCommandBuffers* _commandBuffersRef) :
        m_vulkanCoreRef(_vulkanCoreRef), m_commandBuffersRef(_commandBuffersRef)
    {
        if (_vulkanCoreRef.expired()) {
            MARK_FATAL(Utils::Category::Vulkan, "Vulkan Core Reference Is Null In TextureHandler!");
        }
        if (!m_commandBuffersRef) {
            MARK_FATAL(Utils::Category::Vulkan, "Vulkan Command Buffer Reference Is Null In TextureHandler!");
        }
    }

    TextureHandler::TextureHandler(std::weak_ptr<VulkanCore> _vulkanCoreRef) :
        m_vulkanCoreRef(_vulkanCoreRef)
    {}

    void TextureHandler::destroyTextureHandler(VkDevice _device)
    {
        if (m_textureSampler != VK_NULL_HANDLE) {
            vkDestroySampler(_device, m_textureSampler, nullptr);
            m_textureSampler = VK_NULL_HANDLE;
        }
        if (m_textureImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(_device, m_textureImageView, nullptr);
            m_textureImageView = VK_NULL_HANDLE;
        }
        if (m_textureImage != VK_NULL_HANDLE) {
            vkDestroyImage(_device, m_textureImage, nullptr);
            m_textureImage = VK_NULL_HANDLE;
        }
        if (m_textureMemory != VK_NULL_HANDLE) {
            vkFreeMemory(_device, m_textureMemory, nullptr);
            m_textureMemory = VK_NULL_HANDLE;
        }
        MARK_INFO(Utils::Category::Vulkan, "Texture Handler Destroyed");
    }

    void TextureHandler::generateTexture(const char* _texturePath)
    {
        int imageWidth, imageHeight, imageChannels;

        stbi_uc* pixels = stbi_load(_texturePath, &imageWidth, &imageHeight, &imageChannels, STBI_rgb_alpha);

        if (!pixels) {
            MARK_ERROR(Utils::Category::Vulkan, "Failed to load texture image: %s  (Check File Path/Type Is Correct)", Utils::ShortPathForLog(_texturePath).c_str());
            
            const auto level = Utils::Level::Error;
            const auto category = Utils::Category::System;

            MARK_SCOPE(category, level, "Failed to load texture from:");
            MARK_IN_SCOPE(category, level, "%s", Utils::ShortPathForLog(_texturePath).c_str());
            MARK_IN_SCOPE(category, level, "Defaulting to " MARK_COL_LABEL2 "[MARK_FALLBACK_TEXTURE]" MARK_COL_RESET);

            pixels = stbi_load(MARK_FALLBACK_TEXTURE, &imageWidth, &imageHeight, &imageChannels, STBI_rgb_alpha);

            if (!pixels) {
                MARK_FATAL(Utils::Category::Vulkan, "Failed to load fallback texture from: %s", Utils::ShortPathForLog(MARK_FALLBACK_TEXTURE).c_str());
            }
        }

        VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        createTextureImageFromData(pixels, imageWidth, imageHeight, imageFormat);

        stbi_image_free(pixels);

        MARK_INFO(Utils::Category::Vulkan, "Texture Loaded To Vulkan From: %s", Utils::ShortPathForLog(_texturePath).c_str());
    }

    void TextureHandler::createTextureImageFromData(const void* _pixels, int _width, int _height, VkFormat _format, bool _isCubemap)
    {
        VkImageUsageFlagBits usage = (VkImageUsageFlagBits)(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        VkMemoryPropertyFlagBits properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        createImage(_width, _height, _format, usage, properties, _isCubemap);

        VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        m_textureImageView = createImageView(_format, aspectFlags, _isCubemap);

        VkFilter minFilter = VK_FILTER_LINEAR;
        VkFilter maxFilter = VK_FILTER_LINEAR;
        VkSamplerAddressMode adressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        m_textureSampler = createTextureSampler(minFilter, maxFilter, adressMode);

        updateTextureImage(_pixels, _width, _height, _format, _isCubemap);
    }

    void TextureHandler::createImage(int _width, int _height, VkFormat _format, VkImageUsageFlags _usage, VkMemoryPropertyFlagBits _properties, bool _isCubemap)
    {
        VkDevice device = m_vulkanCoreRef.lock()->device();

        VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = _isCubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : (VkImageCreateFlags)0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = _format,
            .extent = {
                .width = static_cast<uint32_t>(_width),
                .height = static_cast<uint32_t>(_height),
                .depth = 1u,
            },
            .mipLevels = 1u,
            .arrayLayers = _isCubemap ? 6u : 1u,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = _usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0u,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        VkResult res = vkCreateImage(device, &imageInfo, nullptr, &m_textureImage);
        CHECK_VK_RESULT(res, "Failed to create texture image!");

        VkMemoryRequirements memRequirements = { 0 };
        vkGetImageMemoryRequirements(device, m_textureImage, &memRequirements);
        MARK_DEBUG(Utils::Category::Vulkan, "Texture Image Memory Requirements - Size: %llu Alignment: %llu MemoryTypeBits: 0x%08X", memRequirements.size, memRequirements.alignment, memRequirements.memoryTypeBits);

        uint32_t memoryTypeIndex = getMemoryTypeIndex(memRequirements.memoryTypeBits, _properties);
        MARK_DEBUG(Utils::Category::Vulkan, "Texture Image Memory Type Index: %u", memoryTypeIndex);

        VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = memoryTypeIndex
        };

        res = vkAllocateMemory(device, &allocInfo, nullptr, &m_textureMemory);
        CHECK_VK_RESULT(res, "Failed to allocate texture image memory!");

        res = vkBindImageMemory(device, m_textureImage, m_textureMemory, 0);
        CHECK_VK_RESULT(res, "Failed to bind texture image memory!");
    }

    uint32_t TextureHandler::getMemoryTypeIndex(uint32_t _typeFilter, VkMemoryPropertyFlagBits _properties)
    {
        const VkPhysicalDeviceMemoryProperties& memProperties = m_vulkanCoreRef.lock()->physicalDevices().selected().m_memoryProperties;

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) 
        {
            const VkMemoryType& memoryType = memProperties.memoryTypes[i];
            uint32_t curBitMask = (1 << i);
            bool isCurMemTypeSupported = (_typeFilter & curBitMask);
            bool hasRequiredMemProperties = (memoryType.propertyFlags & _properties) == _properties;

            if (isCurMemTypeSupported && hasRequiredMemProperties) {
                return i;
            }
        }

        MARK_FATAL(Utils::Category::Vulkan, "Failed to find suitable memory type for %x requested mem props %x", _typeFilter, _properties);
        return -1;
    }

    void TextureHandler::updateTextureImage(const void* _pixels, int _width, int _height, VkFormat _format, bool _isCubemap)
    {
        VkDevice device = m_vulkanCoreRef.lock()->device();

        int bytesPerPixel = getBytesPerTexFormat(_format);

        VkDeviceSize layerSize = _width * _height * bytesPerPixel;
        int layerCount = _isCubemap ? 6 : 1;
        VkDeviceSize imageSize = layerSize * layerCount;

        VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        BufferAndMemory stagingTexture = BufferAndMemory(m_vulkanCoreRef.lock(), imageSize, usage, properties, "TextureHandler.StagingTexture");
        stagingTexture.update(device, _pixels, static_cast<size_t>(imageSize));

        transitionImageLayout(m_textureImage, _format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layerCount);

        copyBufferToImage(stagingTexture.m_buffer, m_textureImage, static_cast<uint32_t>(_width), static_cast<uint32_t>(_height), layerSize, layerCount);

        transitionImageLayout(m_textureImage, _format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, layerCount);

        stagingTexture.destroy(device);
    }

    int TextureHandler::getBytesPerTexFormat(VkFormat _format)
    {
        switch (_format)
        {
        case VK_FORMAT_R8_SINT:
        case VK_FORMAT_R8_UNORM:
            return 1;
        case VK_FORMAT_R16_SFLOAT:
            return 2;
        case VK_FORMAT_R16G16_SFLOAT:
        case VK_FORMAT_R16G16_SNORM:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_R8G8B8A8_SRGB:
            return 4;
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return 4 * sizeof(uint16_t);
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return 4 * sizeof(float);
        default:
            MARK_FATAL(Utils::Category::Vulkan, "GetBytesPerTexFormat: Unsupported VkFormat %s", string_VkFormat(_format));
            return 0;
        }
    }

    void TextureHandler::transitionImageLayout(VkImage& _image, VkFormat _format, VkImageLayout _oldLayout, VkImageLayout _newLayout, int _layerCount)
    {
        m_commandBuffersRef->beginCommandBuffer(m_commandBuffersRef->copyCommandBuffer(), VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VkImageMemoryBarrier barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = 0,
            .oldLayout = _oldLayout,
            .newLayout = _newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = _image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = (uint32_t)_layerCount,
            }
        };

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (_newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
            (_format == VK_FORMAT_D16_UNORM) ||
            (_format == VK_FORMAT_D32_SFLOAT) ||
            (_format == VK_FORMAT_X8_D24_UNORM_PACK32) ||
            (_format == VK_FORMAT_S8_UINT) ||
            (_format == VK_FORMAT_D16_UNORM_S8_UINT) ||
            (_format == VK_FORMAT_D24_UNORM_S8_UINT))
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            if (hasStencilComponent(_format)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        if (_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && _newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && _newLayout == VK_IMAGE_LAYOUT_GENERAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && _newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } /* Convert back from read-only to updateable */
        else if (_oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && _newLayout == VK_IMAGE_LAYOUT_GENERAL)
        {
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } /* Convert from updateable texture to shader read-only */
        else if (_oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && _newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } /* Convertdepth texture from undefined state to depth-stencil buffer */
        else if (_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && _newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } /* Wait for render pass to complete */
        else
        {
            MARK_FATAL(Utils::Category::Vulkan, "Unsupported layout transition from %s to %s", string_VkImageLayout(_oldLayout), string_VkImageLayout(_newLayout));
        }

        vkCmdPipelineBarrier(m_commandBuffersRef->copyCommandBuffer(),
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        submitCopyCommand();
    }

    bool TextureHandler::hasStencilComponent(VkFormat _format)
    {
        return (_format == VK_FORMAT_S8_UINT ||
                _format == VK_FORMAT_D16_UNORM_S8_UINT ||
                _format == VK_FORMAT_D24_UNORM_S8_UINT ||
                _format == VK_FORMAT_D32_SFLOAT_S8_UINT);
    }

    void TextureHandler::copyBufferToImage(VkBuffer _buffer, VkImage _image, uint32_t _width, uint32_t _height, VkDeviceSize _layerSize, int _layerCount)
    {
        m_commandBuffersRef->beginCommandBuffer(m_commandBuffersRef->copyCommandBuffer(), VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        std::vector<VkBufferImageCopy> bufferImageCopies(_layerCount);

        for (size_t i = 0; i < _layerCount; i++)
        {
            bufferImageCopies[i] = {
                .bufferOffset = i * _layerSize,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = (uint32_t)i,
                    .layerCount = 1,
                },
                .imageOffset = {
                    .x = 0,
                    .y = 0,
                    .z = 0
                },
                .imageExtent = {
                    .width = _width,
                    .height = _height,
                    .depth = 1,
                }
            };
        }

        vkCmdCopyBufferToImage(
            m_commandBuffersRef->copyCommandBuffer(),
            _buffer,
            _image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            (uint32_t)bufferImageCopies.size(),
            bufferImageCopies.data()
        );

        submitCopyCommand();
    }

    void TextureHandler::submitCopyCommand()
    {
        vkEndCommandBuffer(m_commandBuffersRef->copyCommandBuffer());

        VulkanQueue& graphicsQueue = m_vulkanCoreRef.lock()->graphicsQueue();
        graphicsQueue.submitAsync(&m_commandBuffersRef->copyCommandBuffer(), 1, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

        graphicsQueue.waitIdle();
    }

    VkImageView TextureHandler::createImageView(VkFormat _format, VkImageAspectFlags _aspectFlags, bool _isCubemap)
    {
        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = m_textureImage,
            .viewType = _isCubemap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D,
            .format = _format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = _aspectFlags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = _isCubemap ? 6u : 1u,
            }
        };

        VkImageView imageView;
        VkResult res = vkCreateImageView(m_vulkanCoreRef.lock()->device(), &viewInfo, nullptr, &imageView);
        CHECK_VK_RESULT(res, "Failed to create texture image view!");

        return imageView;
    }

    VkSampler TextureHandler::createTextureSampler(VkFilter _minFilter, VkFilter _maxFilter, VkSamplerAddressMode _adressMode)
    {
        VkSamplerCreateInfo samplerInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = _maxFilter,
            .minFilter = _minFilter,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = _adressMode,
            .addressModeV = _adressMode,
            .addressModeW = _adressMode,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1.0f,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE
        };

        VkSampler textureSampler;
        VkResult res = vkCreateSampler(m_vulkanCoreRef.lock()->device(), &samplerInfo, nullptr, &textureSampler);
        CHECK_VK_RESULT(res, "Failed to create texture sampler!");

        return textureSampler;
    }

    // Cubemap generation constants and helper functions

    #define CUBE_MAP_INDEX_POS_X 0
    #define CUBE_MAP_INDEX_NEG_X 1
    #define CUBE_MAP_INDEX_POS_Y 2
    #define CUBE_MAP_INDEX_NEG_Y 3
    #define CUBE_MAP_INDEX_POS_Z 4
    #define CUBE_MAP_INDEX_NEG_Z 5


    static int types[6] = { CUBE_MAP_INDEX_POS_X,
                            CUBE_MAP_INDEX_NEG_X,
                            CUBE_MAP_INDEX_POS_Y,
                            CUBE_MAP_INDEX_NEG_Y,
                            CUBE_MAP_INDEX_POS_Z,
                            CUBE_MAP_INDEX_NEG_Z };

    #define CUBEMAP_NUM_FACES 6

    void TextureHandler::generateCubemapTexture(const char* _cubemapTexturePath)
    {
        int width, height;
        stbi_uc* pixels = stbi_load(_cubemapTexturePath, &width, &height, nullptr, STBI_rgb_alpha);

        if (!pixels) {
            MARK_ERROR(Utils::Category::Vulkan, "Failed to load cubemap texture image: %s  (Check File Path/Type Is Correct)", Utils::ShortPathForLog(_cubemapTexturePath).c_str());
        }

        Bitmap source(width, height, 4, eBitmapFormat_UnsignedByte, pixels);
        std::vector<Bitmap> cubeMap;
        int faceSize = convertEquirectangularImageToCubemap(source, cubeMap);

        stbi_image_free(pixels);

        VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
        int bytesPerPixel = getBytesPerTexFormat(imageFormat);
        size_t singleFaceNumBytes = faceSize * faceSize * bytesPerPixel;
        size_t totalBytes = singleFaceNumBytes * CUBEMAP_NUM_FACES;
        char* p = (char*)malloc(totalBytes);

        for (size_t i = 0; i < CUBEMAP_NUM_FACES; i++) {
            memcpy(p + i * singleFaceNumBytes, cubeMap[i].m_data.data(), singleFaceNumBytes);
        }

        createTextureImageFromData(p, faceSize, faceSize, imageFormat, true);

        free(p);
    }

    int TextureHandler::convertEquirectangularImageToCubemap(const Bitmap& _source, std::vector<Bitmap>& _cubeMap)
    {
        int FaceSize = _source.m_width / 4;

        _cubeMap.resize(CUBEMAP_NUM_FACES);
        for (int i = 0; i < CUBEMAP_NUM_FACES; i++) {
            _cubeMap[i].Init(FaceSize, FaceSize, _source.m_components, _source.m_format);
        }

        int MaxW = _source.m_width - 1;
        int MaxH = _source.m_height - 1;
        for (int face = 0; face < CUBEMAP_NUM_FACES; face++)
        {
            for (int y = 0; y < FaceSize; y++) 
            {
                for (int x = 0; x < FaceSize; x++) 
                {
                    glm::vec3 P = faceCoordsToXYZ(x, y, face, FaceSize);
                    float R = sqrtf(P.x * P.x + P.y * P.y);
                    float phi = atan2f(P.y, P.x);
                    float theta = atan2f(P.z, R);

                    // Calculate texture coordinates
                    auto PI = glm::pi<float>();
                    float u = (float)((phi + PI) / (2.0f * PI));
                    float v = (float((PI / 2.0f - theta) / PI));

                    // Scale texture coordinates by image size
                    float U = u * _source.m_width;
                    float V = v * _source.m_height;

                    // 4-samples for bilinear interpolation
                    int U1 = glm::clamp(int(floor(U)), 0, MaxW);
                    int V1 = glm::clamp(int(floor(V)), 0, MaxH);
                    int U2 = glm::clamp(U1 + 1, 0, MaxW);
                    int V2 = glm::clamp(V1 + 1, 0, MaxH);

                    // Calculate the fractional part
                    float s = U - U1;
                    float t = V - V1;

                    // Fetch 4-samples
                    glm::vec4 BottomLeft = _source.getPixel(U1, V1);
                    glm::vec4 BottomRight = _source.getPixel(U2, V1);
                    glm::vec4 TopLeft = _source.getPixel(U1, V2);
                    glm::vec4 TopRight = _source.getPixel(U2, V2);

                    // Bilinear interpolation
                    glm::vec4 color = BottomLeft * (1 - s) * (1 - t) +
                        BottomRight * (s) * (1 - t) +
                        TopLeft * (1 - s) * t +
                        TopRight * (s) * (t);

                    _cubeMap[face].setPixel(x, y, color);
                }
            }
        }

        return FaceSize;
    }

    glm::vec3 TextureHandler::faceCoordsToXYZ(int _x, int _y, int _faceID, int _faceSize)
    {
        float A = 2.0f * float(_x) / _faceSize;
        float B = 2.0f * float(_y) / _faceSize;

        glm::vec3 Ret;

        switch (_faceID) {
        case CUBE_MAP_INDEX_POS_X:
            Ret = glm::vec3(A - 1.0f, 1.0f, 1.0f - B);
            break;

        case CUBE_MAP_INDEX_NEG_X:
            Ret = glm::vec3(1.0f - A, -1.0f, 1.0f - B);
            break;

        case CUBE_MAP_INDEX_POS_Y:
            Ret = glm::vec3(1.0f - B, A - 1.0f, 1.0f);
            break;

        case CUBE_MAP_INDEX_NEG_Y:
            Ret = glm::vec3(B - 1.0f, A - 1.0f, -1.0f);
            break;

        case CUBE_MAP_INDEX_POS_Z:
            Ret = glm::vec3(-1.0f, A - 1.0f, 1.0f - B);
            break;

        case CUBE_MAP_INDEX_NEG_Z:
            Ret = glm::vec3(1.0f, 1.0f - A, 1.0f - B);
            break;

        default:
            assert(0);
        }

        return Ret;
    }
} // namespace Mark::RendererVK