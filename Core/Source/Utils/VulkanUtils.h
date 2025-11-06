#pragma once
#include "ErrorHandling.h"
#include "MarkUtils.h"
#include <volk.h>
#include <cstdint>
#include <string>

#define CHECK_VK_RESULT(res, msg)      \
    do { if (res != VK_SUCCESS)        \
        MARK_ERROR("Error in %s:%d - %s, code %x\n", __FILE__, __LINE__, msg, res); } while(0)

#define REQ_FEATURE(feats, member)     \
    do { if (feats.member != VK_TRUE)  \
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
    return "NOT REAL SEVERITY STRENGTH!";
}
inline ::Mark::Utils::Level VkSeverityToLevel(VkDebugUtilsMessageSeverityFlagBitsEXT _severity)
{
    switch (_severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return ::Mark::Utils::Level::Debug;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    return ::Mark::Utils::Level::Info;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return ::Mark::Utils::Level::Warn;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   return ::Mark::Utils::Level::Error;
    default:                                              return ::Mark::Utils::Level::Trace;
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
// Converts any Vulkan handle to uint64_t (works for dispatchable & non-dispatchable)
template <typename T>
inline uint64_t VkHandleToU64(T _handle)
{
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(_handle));
}
// Naming helper
inline void VkSetObjectName(VkDevice _device, VkObjectType _type, uint64_t _handle, const char* _name)
{
    if (!_name || !*_name) return;

    auto func = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(_device, "vkSetDebugUtilsObjectNameEXT"));
    if (!func) return;

    VkDebugUtilsObjectNameInfoEXT objectNameInfo = { 
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = _type,
        .objectHandle = _handle,
        .pObjectName = _name
    };

    (void)func(_device, &objectNameInfo);
}   
template <typename HandleT>
inline void VkSetObjectName(VkDevice _device, VkObjectType _type, HandleT _handle, const char* _name)
{
    VkSetObjectName(_device, _type, VkHandleToU64(_handle), _name);
}