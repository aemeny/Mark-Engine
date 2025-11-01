#pragma once
#include <volk.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/resource_limits_c.h>
#include <glslang/Include/glslang_c_interface.h>

#include <vector>

namespace Mark::RendererVK
{
    VkShaderModule createShaderModuleFromText(VkDevice& _device, const char* _fileName);
    VkShaderModule createShaderModuleFromBinary(VkDevice& _device, const char* _fileName);

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
} // namespace Mark::RendererVK