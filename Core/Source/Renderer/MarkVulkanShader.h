#pragma once
#include <volk.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/resource_limits_c.h>
#include <glslang/Include/glslang_c_interface.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>

namespace Mark::RendererVK
{
    bool readSPIRVFileToWords(const char* _fileName, std::vector<uint32_t>& outWords);

    struct ShaderModuleInfo
    {
        std::vector<uint32_t> m_SPIRV;
        VkShaderModule m_shaderModule = VK_NULL_HANDLE;

        void initialize(glslang_program_t* _program)
        {
            size_t programSize = glslang_program_SPIRV_get_size(_program);
            m_SPIRV.resize(programSize);
            glslang_program_SPIRV_get(_program, m_SPIRV.data());
        }
    };

    // GLSL / SPIR-V -> VkShaderModule caching system
    struct VulkanShaderCache
    {
        explicit VulkanShaderCache(VkDevice& _device);
        ~VulkanShaderCache() = default;
        void destroy();

        // Compile from GLSL if needed; otherwise use up-to-date sibling .spv (e.g., "file.vert.spv")
        VkShaderModule getOrCreateFromGLSL(const char* _glslPath, const char* _entry = "main");
        // Load directly from .spv
        VkShaderModule getOrCreateFromSPV(const char* _spvPath, glslang_stage_t _stage = GLSLANG_STAGE_VERTEX);

        // Invalidate one path (forces recompile on next request)
        void invalidatePath(const char* _path);

    private:
        void destroyAll();

        struct Key
        {
            std::string m_absPath;
            glslang_stage_t m_stage;
            std::string m_entry;
            bool operator==(const Key& _o) const noexcept
            {
                return m_stage == _o.m_stage && m_entry == _o.m_entry && m_absPath == _o.m_absPath;
            }
        };
        struct KeyHasher
        {
            size_t operator()(const Key& _k) const noexcept
            {
                size_t hash = std::hash<std::string>{}(_k.m_absPath);
                hash ^= std::hash<int>{}(static_cast<int>(_k.m_stage)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                hash ^= std::hash<std::string>{}(_k.m_entry) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                return hash;
            }
        };
        struct Entry
        {
            VkShaderModule m_module{ VK_NULL_HANDLE };
            std::vector<uint32_t> m_spirv;
            std::filesystem::file_time_type m_srcTime{};
            std::filesystem::file_time_type m_spvTime{};
        };

        VkDevice m_device;
        std::unordered_map<Key, Entry, KeyHasher> m_map;
        bool tryLoadSiblingSpv(const std::filesystem::path& _glsl, std::vector<uint32_t>& _outWords,
            std::filesystem::file_time_type& _spvTime) const;
        static std::string makeSiblingSpvName(const std::filesystem::path& _glsl);
    };
} // namespace Mark::RendererVK