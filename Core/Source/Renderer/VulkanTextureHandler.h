#pragma once
#include <Volk/volk.h>

namespace Mark::RendererVK
{
    struct TextureHandler
    {
        TextureHandler(const char* _texturePath);
        void DestroyTextureHandler(VkDevice _device);

    private:
        VkImage m_textureImage{ VK_NULL_HANDLE };
        VkDeviceMemory m_textureMemory{ VK_NULL_HANDLE };
        VkImageView m_textureImageView{ VK_NULL_HANDLE };
        VkSampler m_textureSampler{ VK_NULL_HANDLE };
    };
} // namespace Mark::RendererVK