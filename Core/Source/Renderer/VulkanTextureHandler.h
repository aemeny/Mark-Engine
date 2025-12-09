#pragma once
#include <Volk/volk.h>
#include <memory>

namespace Mark::RendererVK
{
    struct VulkanCore;
    struct VulkanCommandBuffers;
    struct TextureHandler
    {
        TextureHandler(std::weak_ptr<VulkanCore> _vulkanCoreRef, VulkanCommandBuffers* _commandBuffersRef, const char* _texturePath);
        TextureHandler(std::weak_ptr<VulkanCore> _vulkanCoreRef);
        void destroyTextureHandler(VkDevice _device);

        VkSampler sampler() const { return m_textureSampler; }
        VkImageView imageView() const { return m_textureImageView; }

    private:
        // Use for depth textures
        friend struct VulkanSwapChain;

        std::weak_ptr<VulkanCore> m_vulkanCoreRef;
        VulkanCommandBuffers* m_commandBuffersRef = nullptr;

        VkImage m_textureImage{ VK_NULL_HANDLE };
        VkDeviceMemory m_textureMemory{ VK_NULL_HANDLE };
        VkImageView m_textureImageView{ VK_NULL_HANDLE };
        VkSampler m_textureSampler{ VK_NULL_HANDLE };

        void generateTexture(const char* _texturePath);

        void createTextureImageFromData(const void* _pixels, int _width, int _height, VkFormat _format);
        void createImage(int _width, int _height, VkFormat _format, VkImageUsageFlags _usage, VkMemoryPropertyFlagBits _properties);
        uint32_t getMemoryTypeIndex(uint32_t _typeFilter, VkMemoryPropertyFlagBits _properties);
        
        void updateTextureImage(const void* _pixels, int _width, int _height, VkFormat _format);
        int getBytesPerTexFormat(VkFormat _format);

        void transitionImageLayout(VkImage& _image, VkFormat _format, VkImageLayout _oldLayout, VkImageLayout _newLayout);
        bool hasStencilComponent(VkFormat _format);
        void copyBufferToImage(VkBuffer _buffer, VkImage _image, uint32_t _width, uint32_t _height);
        void submitCopyCommand();

        VkImageView createImageView(VkFormat _format, VkImageAspectFlags _aspectFlags);
        VkSampler createTextureSampler(VkFilter _minFilter, VkFilter _maxFilter, VkSamplerAddressMode _adressMode);
    };
} // namespace Mark::RendererVK