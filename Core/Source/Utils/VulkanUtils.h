#pragma once
#include "ErrorHandling.h"
#include "Mark_Utils.h"
#include <volk.h>
#include <cstdint>
#include <string>

#define CHECK_VK_RESULT(res, msg)      \
    do { if ((res) != VK_SUCCESS)        \
        MARK_ERROR("Error in %s:%d - %s, code %x\n", __FILE__, __LINE__, msg, res); } while(0)

#define REQ_FEATURE(feats, member)     \
    do { if ((feats).member != VK_TRUE)  \
        MARK_ERROR("Required device feature not supported: %s", #member); } while(0)


inline const char* GetDebugSeverityStr(VkDebugUtilsMessageSeverityFlagBitsEXT _severity)
{
    switch (_severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return "Verbose";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    return "Info";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return "Warning";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   return "Error";
    default:                                              return "Invalid severity code";
    }
}
inline ::Mark::Utils::Level VkSeverityToLevel(VkDebugUtilsMessageSeverityFlagBitsEXT _severity)
{
    switch (_severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return ::Mark::Utils::Level::Debug;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    return ::Mark::Utils::Level::Info;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return ::Mark::Utils::Level::Warn;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   return ::Mark::Utils::Level::Error;
    default:                                              return ::Mark::Utils::Level::All;
    }
}
inline std::string GetDebugType(VkDebugUtilsMessageTypeFlagsEXT _type)
{
    std::string s;
    if (_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)     s += "General|";
    if (_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)  s += "Validation|";
    if (_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) s += "Performance|";
    if (s.empty()) s = "Unknown";
    else s.pop_back(); // drop ending '|'
    return s;
}
inline const char* VkObjectTypeToStr(VkObjectType _type)
{
    switch (_type)
    {
    case VK_OBJECT_TYPE_INSTANCE: return "INSTANCE";
    case VK_OBJECT_TYPE_PHYSICAL_DEVICE: return "PHYSICAL_DEVICE";
    case VK_OBJECT_TYPE_DEVICE: return "DEVICE";
    case VK_OBJECT_TYPE_QUEUE: return "QUEUE";
    case VK_OBJECT_TYPE_SEMAPHORE: return "SEMAPHORE";
    case VK_OBJECT_TYPE_COMMAND_BUFFER: return "COMMAND_BUFFER";
    case VK_OBJECT_TYPE_FENCE: return "FENCE";
    case VK_OBJECT_TYPE_DEVICE_MEMORY: return "DEVICE_MEMORY";
    case VK_OBJECT_TYPE_BUFFER: return "BUFFER";
    case VK_OBJECT_TYPE_IMAGE: return "IMAGE";
    case VK_OBJECT_TYPE_EVENT: return "EVENT";
    case VK_OBJECT_TYPE_QUERY_POOL: return "QUERY_POOL";
    case VK_OBJECT_TYPE_BUFFER_VIEW: return "BUFFER_VIEW";
    case VK_OBJECT_TYPE_IMAGE_VIEW: return "IMAGE_VIEW";
    case VK_OBJECT_TYPE_SHADER_MODULE: return "SHADER_MODULE";
    case VK_OBJECT_TYPE_PIPELINE_CACHE: return "PIPELINE_CACHE";
    case VK_OBJECT_TYPE_PIPELINE_LAYOUT: return "PIPELINE_LAYOUT";
    case VK_OBJECT_TYPE_RENDER_PASS: return "RENDER_PASS";
    case VK_OBJECT_TYPE_PIPELINE: return "PIPELINE";
    case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT: return "DESCRIPTOR_SET_LAYOUT";
    case VK_OBJECT_TYPE_SAMPLER: return "SAMPLER";
    case VK_OBJECT_TYPE_DESCRIPTOR_POOL: return "DESCRIPTOR_POOL";
    case VK_OBJECT_TYPE_DESCRIPTOR_SET: return "DESCRIPTOR_SET";
    case VK_OBJECT_TYPE_FRAMEBUFFER: return "FRAMEBUFFER";
    case VK_OBJECT_TYPE_COMMAND_POOL: return "COMMAND_POOL";
    case VK_OBJECT_TYPE_SURFACE_KHR: return "SURFACE_KHR";
    case VK_OBJECT_TYPE_SWAPCHAIN_KHR: return "SWAPCHAIN_KHR";
    default: return "UNKNOWN";
    }
}

// Vulkan debug utils object naming
#ifndef MARK_VK_DEBUG_NAMES
    #ifdef NDEBUG
        #define MARK_VK_DEBUG_NAMES 0
    #else
        #define MARK_VK_DEBUG_NAMES 1
    #endif
#endif

// Converts any Vulkan handle to uint64_t
template <typename HandleT>
inline uint64_t VkHandleToU64(HandleT _handle) noexcept
{
    if constexpr (std::is_pointer_v<HandleT>)
        return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(_handle));
    else
        return static_cast<uint64_t>(_handle);
}

inline bool VkDebugUtilsAvailable() noexcept
{
#if MARK_VK_DEBUG_NAMES
    return vkSetDebugUtilsObjectNameEXT != nullptr;
#else
    return false;
#endif
}

inline void VkSetObjectName(VkDevice _device, VkObjectType _type, uint64_t _handle, const char* _name) noexcept
{
#if MARK_VK_DEBUG_NAMES
    if (!_name || !*_name) return;
    if (_device == VK_NULL_HANDLE || _handle == 0) return;
    if (!vkSetDebugUtilsObjectNameEXT) return;

    VkDebugUtilsObjectNameInfoEXT info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = _type,
        .objectHandle = _handle,
        .pObjectName = _name
    };

    (void)vkSetDebugUtilsObjectNameEXT(_device, &info);
#else
    (void)_device; (void)_type; (void)_handle; (void)_name;
#endif
}

template <typename HandleT>
inline void VkSetObjectName(VkDevice _device, VkObjectType _type, HandleT _handle, const char* _name) noexcept
{
    VkSetObjectName(_device, _type, VkHandleToU64(_handle), _name);
}

inline void VkSetObjectNameF(VkDevice _device, VkObjectType _type, uint64_t _handle, const char* _fmt, ...) noexcept
{
#if MARK_VK_DEBUG_NAMES
    if (!_fmt || !*_fmt) return;
    char buf[256];
    va_list args;
    va_start(args, _fmt);
    std::vsnprintf(buf, sizeof(buf), _fmt, args);
    va_end(args);
    VkSetObjectName(_device, _type, _handle, buf);
#else
    (void)_device; (void)_type; (void)_handle; (void)_fmt;
#endif
}

template <typename HandleT>
inline void VkSetObjectNameF(VkDevice _device, VkObjectType _type, HandleT _handle, const char* _fmt, ...) noexcept
{
#if MARK_VK_DEBUG_NAMES
    if (!_fmt || !*_fmt) return;
    char buf[256];
    va_list args;
    va_start(args, _fmt);
    std::vsnprintf(buf, sizeof(buf), _fmt, args);
    va_end(args);
    VkSetObjectName(_device, _type, _handle, buf);
#else
    (void)_device; (void)_type; (void)_handle; (void)_fmt;
#endif
}

// Command buffer labels (RenderDoc / captures)
inline void VkCmdBeginLabel(VkCommandBuffer _cmd, const char* _label) noexcept
{
#if MARK_VK_DEBUG_NAMES
    if (!_label || !*_label) return;
    if (!vkCmdBeginDebugUtilsLabelEXT) return;
    VkDebugUtilsLabelEXT lab{};
    lab.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    lab.pLabelName = _label;
    vkCmdBeginDebugUtilsLabelEXT(_cmd, &lab);
#else
    (void)_cmd; (void)_label;
#endif
}

inline void VkCmdEndLabel(VkCommandBuffer _cmd) noexcept
{
#if MARK_VK_DEBUG_NAMES
    if (!vkCmdEndDebugUtilsLabelEXT) return;
    vkCmdEndDebugUtilsLabelEXT(_cmd);
#else
    (void)_cmd;
#endif
}

inline void VkCmdInsertLabel(VkCommandBuffer _cmd, const char* _label) noexcept
{
#if MARK_VK_DEBUG_NAMES
    if (!_label || !*_label) return;
    if (!vkCmdInsertDebugUtilsLabelEXT) return;
    VkDebugUtilsLabelEXT lab{};
    lab.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    lab.pLabelName = _label;
    vkCmdInsertDebugUtilsLabelEXT(_cmd, &lab);
#else
    (void)_cmd; (void)_label;
#endif
}

struct VkCmdLabelScope
{
    VkCommandBuffer cmd{ VK_NULL_HANDLE };
    bool active{ false };

    VkCmdLabelScope(VkCommandBuffer _cmd, const char* _label) noexcept : cmd(_cmd)
    {
#if MARK_VK_DEBUG_NAMES
        if (cmd != VK_NULL_HANDLE && _label && *_label && vkCmdBeginDebugUtilsLabelEXT && vkCmdEndDebugUtilsLabelEXT)
        {
            active = true;
            VkCmdBeginLabel(cmd, _label);
        }
#else
        (void)_label;
#endif
    }

    ~VkCmdLabelScope() noexcept
    {
#if MARK_VK_DEBUG_NAMES
        if (active) VkCmdEndLabel(cmd);
#endif
    }
};

// Convenience macros
#define MARK_VK_NAME(_device, _objType, _handle, _name) \
    VkSetObjectName((_device), (_objType), (_handle), (_name))

#define MARK_VK_NAME_F(_device, _objType, _handle, _fmt, ...) \
    VkSetObjectNameF((_device), (_objType), (_handle), (_fmt), ##__VA_ARGS__)