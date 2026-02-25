#pragma once
#include "Engine/Bitmap.h"

#include <Volk/volk.h>
#include <glm/glm.hpp>
#include <memory>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanCommandBuffers;
    struct TextureHandler
    {
        TextureHandler(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanCommandBuffers* _commandBuffersRef);
        TextureHandler(std::weak_ptr<VulkanCore> _vulkanCoreRef);
        void destroyTextureHandler(VkDevice _device);

        void generateTexture(const char* _texturePath);
        void generateCubemapTexture(const char* _cubemapTexturePath);

        VkSampler sampler() const { return m_textureSampler; }
        VkImageView imageView() const { return m_textureImageView; }
        VkImage image() const { return m_textureImage; }

    private:
        // Use for depth textures
        friend struct VulkanSwapChain;

        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VulkanCommandBuffers* m_commandBuffersRef = nullptr;

        VkImage m_textureImage{ VK_NULL_HANDLE };
        VkDeviceMemory m_textureMemory{ VK_NULL_HANDLE };
        VkImageView m_textureImageView{ VK_NULL_HANDLE };
        VkSampler m_textureSampler{ VK_NULL_HANDLE };

        void createTextureImageFromData(const void* _pixels, int _width, int _height, VkFormat _format, bool _isCubemap = false);
        void createImage(int _width, int _height, VkFormat _format, VkImageUsageFlags _usage, VkMemoryPropertyFlagBits _properties, bool _isCubemap = false);
        void updateTextureImage(const void* _pixels, int _width, int _height, VkFormat _format, bool _isCubemap = false);
        
        uint32_t getMemoryTypeIndex(uint32_t _typeFilter, VkMemoryPropertyFlagBits _properties);
        int getBytesPerTexFormat(VkFormat _format);

        void transitionImageLayout(VkImage& _image, VkFormat _format, VkImageLayout _oldLayout, VkImageLayout _newLayout, int _layerCount);
        static bool hasStencilComponent(VkFormat _format);
        void copyBufferToImage(VkBuffer _buffer, VkImage _image, uint32_t _width, uint32_t _height, VkDeviceSize _layerSize, int _layerCount);
        void submitCopyCommand();

        VkImageView createImageView(VkFormat _format, VkImageAspectFlags _aspectFlags, bool _isCubemap = false);
        VkSampler createTextureSampler(VkFilter _minFilter, VkFilter _maxFilter, VkSamplerAddressMode _adressMode);
        
        // Cubemap generation
        int convertEquirectangularImageToCubemap(const Bitmap& _source, std::vector<Bitmap>& _cubeMap);
        glm::vec3 faceCoordsToXYZ(int _x, int _y, int _faceID, int _faceSize);
    };
} // namespace Mark::RendererVK