#include "MarkVulkanShader.h"
#include "Utils/ErrorHandling.h"
#include "Utils/MarkUtils.h"
#include "Utils/VulkanUtils.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iterator>

namespace Mark::RendererVK
{
    static bool readFileToString(const char* _fileName, std::string& _outString)
    {
        std::ifstream file(_fileName, std::ios::binary);
        if (!file)
        {
            MARK_LOG_ERROR("Failed to open file: {}", _fileName);
            return false;
        }

        _outString.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        return true;
    }
    bool readSPIRVFileToWords(const char* _fileName, std::vector<uint32_t>& _outWords)
    {
        std::ifstream file(_fileName, std::ios::binary | std::ios::ate);
        if (!file)
        {
            MARK_LOG_ERROR("Failed to open SPIR-V binary: {}", _fileName);
            return false;
        }

        std::streamsize sz = file.tellg();
        if (sz <= 0)
        {
            MARK_LOG_ERROR("Empty SPIR-V file: {}", _fileName);
            return false;
        }

        file.seekg(0, std::ios::beg);
        std::vector<char> bytes(static_cast<size_t>(sz));
        if (!file.read(bytes.data(), sz))
        {
            MARK_LOG_ERROR("Failed to read SPIR-V file: {}", _fileName);
            return false;
        }
        if ((bytes.size() % 4) != 0)
        {
            MARK_LOG_ERROR("SPIR-V file size is not a multiple of 4: {}", _fileName);
            return false;
        }

        _outWords.resize(bytes.size() / 4);
        std::memcpy(_outWords.data(), bytes.data(), bytes.size());
        return true;
    }
    glslang_stage_t shaderStageFromFileName(const char* _fileName)
    {
        std::string fileStr(_fileName);
        if (fileStr.ends_with(".vert"))
            return GLSLANG_STAGE_VERTEX;
        if (fileStr.ends_with(".frag"))
            return GLSLANG_STAGE_FRAGMENT;
        if (fileStr.ends_with(".comp"))
            return GLSLANG_STAGE_COMPUTE;
        if (fileStr.ends_with(".geom"))
            return GLSLANG_STAGE_GEOMETRY;
        if (fileStr.ends_with(".tesc"))
            return GLSLANG_STAGE_TESSCONTROL;
        if (fileStr.ends_with(".tese"))
            return GLSLANG_STAGE_TESSEVALUATION;

        MARK_ERROR("Unknown shader stage for file: {}", _fileName);
        return GLSLANG_STAGE_VERTEX; // Default
    }
    // Pretty logging for glslang shader failures (preprocess/parse)
    static void logGlslangShaderFailure(const char* _phase, const char* _fileName,
        const std::string& _source, glslang_shader_t* _shader)
    {
        MARK_SCOPE_C_L(Utils::Category::Shader, Utils::Level::Error, "%s failed", _phase);
        if (const char* log = glslang_shader_get_info_log(_shader)) 
        {
            MARK_LOG_ERROR_C(Utils::Category::Shader, "InfoLog: %s", log);
        }
        if (const char* dbg = glslang_shader_get_info_debug_log(_shader)) 
        {
            MARK_LOG_ERROR_C(Utils::Category::Shader, "DebugLog: %s", dbg);
        }

        std::istringstream iss(_source);
        std::string line; int ln = 1;

        MARK_LOG_ERROR_C(Utils::Category::Shader, "----- %s (numbered) -----", _fileName);
        while (std::getline(iss, line)) 
        {
            MARK_LOG_ERROR_C(Utils::Category::Shader, "%4d | %s", ln++, line.c_str());
        }
        MARK_LOG_ERROR_C(Utils::Category::Shader, "----- end of source -----");
    }
    // Pretty logging for program link failures
    static void logGlslangProgramFailure(const char* _phase, const char* _fileName, glslang_program_t* _program)
    {
        MARK_SCOPE_C_L(Utils::Category::Shader, Utils::Level::Error, "%s failed", _phase);
        if (const char* log = glslang_program_get_info_log(_program)) 
        {
            MARK_LOG_ERROR_C(Utils::Category::Shader, "InfoLog: %s", log);
        }
        if (const char* dbg = glslang_program_get_info_debug_log(_program)) 
        {
            MARK_LOG_ERROR_C(Utils::Category::Shader, "DebugLog: %s", dbg);
        }
    }

    static bool compileShader(VkDevice& _device, glslang_stage_t _stage, const char* _sourceCode, ShaderModuleInfo& _shaderModule)
    {
        glslang_input_t shaderInput = {
            .language = GLSLANG_SOURCE_GLSL,
            .stage = _stage,
            .client = GLSLANG_CLIENT_VULKAN,
            .client_version = GLSLANG_TARGET_VULKAN_1_3,
            .target_language = GLSLANG_TARGET_SPV,
            .target_language_version = GLSLANG_TARGET_SPV_1_6,
            .code = _sourceCode,
            .default_version = 450,
            .default_profile = GLSLANG_NO_PROFILE,
            .force_default_version_and_profile = false,
            .forward_compatible = false,
            .messages = GLSLANG_MSG_DEFAULT_BIT,
            .resource = glslang_default_resource(),
            .callbacks = nullptr,
            .callbacks_ctx = nullptr
        };

        glslang_shader_t* shader = glslang_shader_create(&shaderInput);

        if (!glslang_shader_preprocess(shader, &shaderInput))
        {
            logGlslangShaderFailure("Preprocess", "<memory>", _sourceCode, shader);
            glslang_shader_delete(shader);
            return false;
        }

        if (!glslang_shader_parse(shader, &shaderInput))
        {
            logGlslangShaderFailure("Parse", "<memory>", _sourceCode, shader);
            glslang_shader_delete(shader);
            return false;
        }

        glslang_program_t* program = glslang_program_create();
        glslang_program_add_shader(program, shader);

        if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
        {
            logGlslangProgramFailure("Link", "<memory>", program);
            glslang_program_delete(program);
            glslang_shader_delete(shader);
            return false;
        }

        glslang_program_SPIRV_generate(program, _stage);

        _shaderModule.initialize(program);

        const char* SPIRV_messages = glslang_program_SPIRV_get_messages(program);

        if (SPIRV_messages)
        {
            MARK_SCOPE_C_L(Utils::Category::Shader, Utils::Level::Warn, "SPIR-V Generation messages");
            MARK_WARN_C(Utils::Category::Shader, "%s", SPIRV_messages);
        }

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = _shaderModule.m_SPIRV.size() * sizeof(uint32_t),
            .pCode = (const uint32_t*)_shaderModule.m_SPIRV.data()
        };

        VkResult res = vkCreateShaderModule(_device, &shaderModuleCreateInfo, nullptr, &_shaderModule.m_shaderModule);
        CHECK_VK_RESULT(res, "Create Shader Module");

        glslang_program_delete(program);
        glslang_shader_delete(shader);

        return _shaderModule.m_SPIRV.size() > 0 && _shaderModule.m_shaderModule != VK_NULL_HANDLE;
    }

    VkShaderModule createShaderModuleFromText(VkDevice& _device, const char* _fileName)
    {
        std::string sourceCode;

        if (!readFileToString(_fileName, sourceCode))
        {
            MARK_ERROR("Failed to read shader source code from file: {}", _fileName);
        }

        ShaderModuleInfo shaderModuleInfo;
        glslang_stage_t shaderStage = shaderStageFromFileName(_fileName);
        VkShaderModule rtn = VK_NULL_HANDLE;

        glslang_initialize_process();

        bool success = compileShader(_device, shaderStage, sourceCode.c_str(), shaderModuleInfo);

        if (success)
        {
            MARK_INFO_C(Utils::Category::Shader, "Shader compiled successfully: {}", _fileName);
            rtn = shaderModuleInfo.m_shaderModule;
            std::string binaryFileName = std::string(_fileName) + ".spv";
            // TODO: Write SPIR-V binary to file for caching here
        }
        else
        {
            MARK_ERROR("Shader compilation failed: {}", _fileName);
        }

        glslang_finalize_process();

        return rtn;
    }

    VkShaderModule createShaderModuleFromBinary(VkDevice& _device, const char* _fileName)
    {
        std::vector<uint32_t> shaderCode;
        if (!readSPIRVFileToWords(_fileName, shaderCode))
        {
            return VK_NULL_HANDLE;
        }

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = shaderCode.size() * sizeof(uint32_t),
            .pCode = shaderCode.data()
        };

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        VkResult res = vkCreateShaderModule(_device, &shaderModuleCreateInfo, nullptr, &shaderModule);
        CHECK_VK_RESULT(res, "Create Shader Module from binary");

        MARK_INFO_C(Utils::Category::Shader, "Shader module created from binary: {}", _fileName);

        return shaderModule;
    }
} // namespace Mark::RendererVK