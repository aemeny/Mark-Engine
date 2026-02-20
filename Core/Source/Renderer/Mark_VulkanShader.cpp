#include "Mark_VulkanShader.h"
#include "Utils/Mark_Utils.h"
#include "Utils/VulkanUtils.h"

#include <string>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iterator>
#include <filesystem>

namespace Mark::RendererVK
{
    static bool readFileToString(const char* _fileName, std::string& _outString)
    {
        std::ifstream file(_fileName, std::ios::binary);
        if (!file)
        {
            MARK_ERROR(Utils::Category::Shader, "Failed to open file: %s", _fileName);
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
            MARK_ERROR(Utils::Category::Shader, "Failed to open SPIR-V binary: %s", _fileName);
            return false;
        }

        std::streamsize sz = file.tellg();
        if (sz <= 0)
        {
            MARK_ERROR(Utils::Category::Shader, "Empty SPIR-V file: %s", _fileName);
            return false;
        }

        file.seekg(0, std::ios::beg);
        std::vector<char> bytes(static_cast<size_t>(sz));
        if (!file.read(bytes.data(), sz))
        {
            MARK_ERROR(Utils::Category::Shader, "Failed to read SPIR-V file: %s", _fileName);
            return false;
        }
        if ((bytes.size() % 4) != 0)
        {
            MARK_ERROR(Utils::Category::Shader, "SPIR-V file size is not a multiple of 4: %s", _fileName);
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

        MARK_FATAL(Utils::Category::Shader, "Unknown shader stage for file: %s", _fileName);
        return GLSLANG_STAGE_VERTEX; // Default
    }
    // Pretty logging for glslang shader failures (preprocess/parse)
    static void logGlslangShaderFailure(const char* _phase, const char* _fileName,
        const std::string& _source, glslang_shader_t* _shader)
    {
        MARK_SCOPE(Utils::Category::Shader, Utils::Level::Error, "%s failed", _phase);
        if (const char* log = glslang_shader_get_info_log(_shader)) 
        {
            MARK_ERROR(Utils::Category::Shader, "InfoLog: %s", log);
        }
        if (const char* dbg = glslang_shader_get_info_debug_log(_shader)) 
        {
            MARK_ERROR(Utils::Category::Shader, "DebugLog: %s", dbg);
        }

        std::istringstream iss(_source);
        std::string line; int ln = 1;

        MARK_ERROR(Utils::Category::Shader, "----- %s (numbered) -----", _fileName);
        while (std::getline(iss, line)) 
        {
            MARK_ERROR(Utils::Category::Shader, "%4d | %s", ln++, line.c_str());
        }
        MARK_ERROR(Utils::Category::Shader, "----- end of source -----");
    }
    // Pretty logging for program link failures
    static void logGlslangProgramFailure(const char* _phase, const char* _fileName, glslang_program_t* _program)
    {
        MARK_SCOPE(Utils::Category::Shader, Utils::Level::Error, "%s failed", _phase);
        if (const char* log = glslang_program_get_info_log(_program)) 
        {
            MARK_ERROR(Utils::Category::Shader, "InfoLog: %s", log);
        }
        if (const char* dbg = glslang_program_get_info_debug_log(_program)) 
        {
            MARK_ERROR(Utils::Category::Shader, "DebugLog: %s", dbg);
        }
    }
    // Write compiled SPIR-V next to the source (simple cache).
    static bool writeWordsToSpv(const std::filesystem::path& _outPath, const std::vector<uint32_t>& _words)
    {
        std::ofstream file(_outPath, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            MARK_ERROR(Utils::Category::Shader, "Failed to open for write: %s", _outPath.string().c_str());
            return false;
        }
        file.write(reinterpret_cast<const char*>(_words.data()), static_cast<std::streamsize>(_words.size() * sizeof(uint32_t)));
        return static_cast<bool>(file);
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
            MARK_SCOPE(Utils::Category::Shader, Utils::Level::Warn, "SPIR-V Generation messages");
            MARK_WARN(Utils::Category::Shader, "%s", SPIRV_messages);
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
        MARK_VK_NAME(_device, VK_OBJECT_TYPE_SHADER_MODULE, _shaderModule.m_shaderModule, "VulkShader.ShaderModule");

        glslang_program_delete(program);
        glslang_shader_delete(shader);

        return _shaderModule.m_SPIRV.size() > 0 && _shaderModule.m_shaderModule != VK_NULL_HANDLE;
    }

    // VulkanShaderCache implementation
    VulkanShaderCache::VulkanShaderCache(VkDevice& _device) :
        m_device(_device)
    {
        glslang_initialize_process();
    }
    void VulkanShaderCache::destroy()
    {
        destroyAll();
        glslang_finalize_process();
    }

    std::string VulkanShaderCache::makeSiblingSpvName(const std::filesystem::path& _glsl)
    {
        return (_glsl.string() + ".spv");
    }

    bool VulkanShaderCache::tryLoadSiblingSpv(const std::filesystem::path& _glsl, std::vector<uint32_t>& _outWords, std::filesystem::file_time_type& _spvTime) const
    {
        const auto spvPath = makeSiblingSpvName(_glsl);
        std::error_code ec;
        if (!std::filesystem::exists(spvPath, ec)) return false;
        if (!readSPIRVFileToWords(spvPath.c_str(), _outWords)) return false;
        auto lwt = std::filesystem::last_write_time(spvPath, ec);
        if (ec) 
        {
            MARK_WARN(Utils::Category::Shader, "last_write_time failed for '%s': %s",
                spvPath.c_str(), ec.message().c_str());
            _spvTime = {};
        }
        else 
        {
            _spvTime = lwt;
        }
        return true;
    }

    VkShaderModule VulkanShaderCache::getOrCreateFromGLSL(const char* _glslPath, const char* _entry)
    {
        std::filesystem::path path(_glslPath);
        std::error_code ec;
        auto abs = std::filesystem::weakly_canonical(path, ec);
        const std::string absString = ec ? path.string() : abs.string();
        const auto stage = shaderStageFromFileName(_glslPath);

        Key k{ absString, stage, _entry ? _entry : "main" };
        if (auto it = m_map.find(k); it != m_map.end()) 
        {
            std::filesystem::file_time_type nowSrc{};
            if (std::filesystem::exists(path, ec)) 
            {
                auto tmp = std::filesystem::last_write_time(path, ec);
                if (!ec) 
                    nowSrc = tmp;
            }

            if (nowSrc == it->second.m_srcTime) 
            {
                return it->second.m_module; // up-to-date
            }
            // Drop and rebuild
            vkDestroyShaderModule(m_device, it->second.m_module, nullptr);
            m_map.erase(it);
        }

        // Try sibling .spv if it's up-to-date
        std::filesystem::file_time_type srcTime{}, spvTime{};
        if (std::filesystem::exists(path, ec)) 
            srcTime = std::filesystem::last_write_time(path, ec);

        std::vector<uint32_t> words;
        if (tryLoadSiblingSpv(path, words, spvTime) && spvTime >= srcTime) 
        {
            // Use cached SPIR-V
            VkShaderModuleCreateInfo moduleCreateInfo = { 
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .codeSize = words.size() * sizeof(uint32_t),
                .pCode = words.data()
            };

            Entry entry{};

            VkResult res = vkCreateShaderModule(m_device, &moduleCreateInfo, nullptr, &entry.m_module);
            CHECK_VK_RESULT(res, "Failed to create shader module");

            entry.m_spirv = std::move(words);
            entry.m_srcTime = srcTime;
            entry.m_spvTime = spvTime;
            m_map.emplace(k, std::move(entry));

            const std::string spvName = makeSiblingSpvName(path);
            const std::string pretty = Utils::ShortPathForLog(spvName);
            MARK_INFO(Utils::Category::Shader, "Loaded cached SPIR-V: %s", pretty.c_str());
            return m_map.find(k)->second.m_module;
        }

        // Compile from GLSL
        std::string src;
        if (!readFileToString(_glslPath, src))
        {
            return VK_NULL_HANDLE;
        }

        ShaderModuleInfo info;
        if (!compileShader(m_device, stage, src.c_str(), info)) 
        {
            return VK_NULL_HANDLE;
        }

        // Write sibling cache and store entry
        writeWordsToSpv(makeSiblingSpvName(path), info.m_SPIRV);

        const std::string spvPath = makeSiblingSpvName(path);
        auto lwt = std::filesystem::last_write_time(spvPath, ec);
        if (ec)
        {
            MARK_WARN(Utils::Category::Shader,
                "last_write_time failed for '%s': %s",
                spvPath.c_str(), ec.message().c_str()
            );
            spvTime = {};
        }
        else 
        {
            spvTime = lwt;
        }

        auto lwt2 = std::filesystem::last_write_time(spvPath, ec);
        Entry entry = {
            .m_module = info.m_shaderModule,
            .m_spirv = std::move(info.m_SPIRV),
            .m_srcTime = srcTime,
            .m_spvTime = ec ? std::filesystem::file_time_type{} : lwt2
        };
        m_map.emplace(k, std::move(entry));

        const std::string pretty = Utils::ShortPathForLog(_glslPath);
        MARK_INFO(Utils::Category::Shader, "Compiled GLSL -> SPIR-V: %s", pretty.c_str());

        return m_map.find(k)->second.m_module;
    }

    VkShaderModule VulkanShaderCache::getOrCreateFromSPV(const char* _spvPath, glslang_stage_t _stage)
    {
        std::filesystem::path path(_spvPath);
        std::error_code ec;
        auto abs = std::filesystem::weakly_canonical(path, ec);
        const std::string absString = ec ? path.string() : abs.string();

        Key k{ absString, _stage, "main" };
        if (auto it = m_map.find(k); it != m_map.end()) 
            return it->second.m_module;

        std::vector<uint32_t> words;
        if (!readSPIRVFileToWords(_spvPath, words)) 
            return VK_NULL_HANDLE;

        VkShaderModuleCreateInfo moduleCreateInfo = { 
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO ,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = words.size() * sizeof(uint32_t),
            .pCode = words.data(),
        };

        Entry entry{};

        VkResult res = vkCreateShaderModule(m_device, &moduleCreateInfo, nullptr, &entry.m_module);
        CHECK_VK_RESULT(res, "Failed to create shader module");

        entry.m_spirv = std::move(words);
        entry.m_spvTime = std::filesystem::last_write_time(path, ec);
        m_map.emplace(k, std::move(entry));

        const std::string pretty = Utils::ShortPathForLog(_spvPath);
        MARK_INFO(Utils::Category::Shader, "Loaded SPIR-V: %s", pretty.c_str());

        return m_map.find(k)->second.m_module;
    }

    void VulkanShaderCache::invalidatePath(const char* _path)
    {
        std::vector<Key> toErase;
        for (auto& [k, e] : m_map) 
        {
            if (k.m_absPath == _path)
            {
                vkDestroyShaderModule(m_device, e.m_module, nullptr);
                toErase.push_back(k);
            }
        }
        for (auto& k : toErase) 
            m_map.erase(k);
    }

    void VulkanShaderCache::destroyAll()
    {
        for (auto& [k, e] : m_map) 
            if (e.m_module)
                vkDestroyShaderModule(m_device, e.m_module, nullptr);
        m_map.clear();
    }
} // namespace Mark::RendererVK